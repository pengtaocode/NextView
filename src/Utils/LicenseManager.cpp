#include "LicenseManager.h"

#include <QSettings>
#include <QNetworkInterface>
#include <QCryptographicHash>
#include <QStorageInfo>
#include <QRegularExpression>
#include <QDate>
#include <QUrl>
#include <QDebug>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>

// Ed25519 公钥（SubjectPublicKeyInfo PEM，硬编码进客户端，无法推导私钥）
static const char kPublicKeyPem[] =
    "-----BEGIN PUBLIC KEY-----\n"
    "MCowBQYDK2VwAyEAXQN+Os+C+rLtRKc7yBqKyJMxZJu7X27zJ/FoDysWL1s=\n"
    "-----END PUBLIC KEY-----\n";

// ──────────────────────────────────────────────────────────────────
// Base32 工具（RFC 4648，不含填充字符）
// ──────────────────────────────────────────────────────────────────

static QByteArray base32Decode(const QString& input) {
    static const QString kAlpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    QByteArray out;
    int buf = 0, bits = 0;
    for (QChar c : input) {
        int val = kAlpha.indexOf(c.toUpper());
        if (val < 0) continue;
        buf = (buf << 5) | val;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            out.append(char((buf >> bits) & 0xFF));
        }
    }
    return out;
}

// ──────────────────────────────────────────────────────────────────
// AES-256-CBC 加解密（OpenSSL EVP，PKCS7 填充）
// ──────────────────────────────────────────────────────────────────

static QByteArray aesEncrypt(const QByteArray& plain,
                              const QByteArray& key,
                              const QByteArray& iv)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray out(plain.size() + 16, '\0');
    int len1 = 0, len2 = 0;
    bool ok =
        EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
            reinterpret_cast<const unsigned char*>(key.constData()),
            reinterpret_cast<const unsigned char*>(iv.constData())) == 1 &&
        EVP_EncryptUpdate(ctx,
            reinterpret_cast<unsigned char*>(out.data()), &len1,
            reinterpret_cast<const unsigned char*>(plain.constData()), plain.size()) == 1 &&
        EVP_EncryptFinal_ex(ctx,
            reinterpret_cast<unsigned char*>(out.data()) + len1, &len2) == 1;

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};
    out.resize(len1 + len2);
    return out;
}

static QByteArray aesDecrypt(const QByteArray& cipher,
                              const QByteArray& key,
                              const QByteArray& iv)
{
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    QByteArray out(cipher.size(), '\0');
    int len1 = 0, len2 = 0;
    bool ok =
        EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
            reinterpret_cast<const unsigned char*>(key.constData()),
            reinterpret_cast<const unsigned char*>(iv.constData())) == 1 &&
        EVP_DecryptUpdate(ctx,
            reinterpret_cast<unsigned char*>(out.data()), &len1,
            reinterpret_cast<const unsigned char*>(cipher.constData()), cipher.size()) == 1 &&
        EVP_DecryptFinal_ex(ctx,
            reinterpret_cast<unsigned char*>(out.data()) + len1, &len2) == 1;

    EVP_CIPHER_CTX_free(ctx);
    if (!ok) return {};
    out.resize(len1 + len2);
    return out;
}

// ──────────────────────────────────────────────────────────────────
// LicenseManager
// ──────────────────────────────────────────────────────────────────

LicenseManager* LicenseManager::instance() {
    static LicenseManager inst;
    return &inst;
}

LicenseManager::LicenseManager() {
    m_machineCode = computeMachineCode();
    load();
}

QString LicenseManager::computeMachineCode() const {
    QStringList parts;

    // 第一块非回环且已启用的网卡 MAC
    for (const QNetworkInterface& iface : QNetworkInterface::allInterfaces()) {
        auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsLoopBack) &&
             (flags & QNetworkInterface::IsUp) &&
            !iface.hardwareAddress().isEmpty()) {
            parts << iface.hardwareAddress().remove(':');
            break;
        }
    }

    // 根卷总字节数（补充熵）
    for (const QStorageInfo& vol : QStorageInfo::mountedVolumes()) {
        if (vol.isRoot()) {
            parts << QString::number(vol.bytesTotal());
            break;
        }
    }

    QByteArray raw = parts.join('|').toUtf8();
    return QCryptographicHash::hash(raw, QCryptographicHash::Sha256)
               .toHex().left(16).toUpper();
}

QByteArray LicenseManager::deriveAesKey() const {
    return QCryptographicHash::hash(
        m_machineCode.toUtf8() + "|NextView2026_KEY",
        QCryptographicHash::Sha256);      // 32 bytes
}

QByteArray LicenseManager::deriveAesIv() const {
    return QCryptographicHash::hash(
        m_machineCode.toUtf8() + "|NextView2026_IVX",
        QCryptographicHash::Sha256).left(16);   // 16 bytes
}

bool LicenseManager::saveKey(const QByteArray& rawKey) {
    QByteArray cipher = aesEncrypt(rawKey, deriveAesKey(), deriveAesIv());
    if (cipher.isEmpty()) {
        qWarning() << "[License] AES 加密失败";
        return false;
    }
    QSettings s("NextView", "NextView");
    s.setValue("license/key", QString::fromLatin1(cipher.toBase64()));
    return true;
}

QByteArray LicenseManager::loadKeyRaw() const {
    QSettings s("NextView", "NextView");
    QString stored = s.value("license/key").toString();
    if (stored.isEmpty()) return {};
    QByteArray cipher = QByteArray::fromBase64(stored.toLatin1());
    return aesDecrypt(cipher, deriveAesKey(), deriveAesIv());
}

// static
bool LicenseManager::verifyEd25519(const QByteArray& payload, const QByteArray& sig) {
    BIO* bio = BIO_new_mem_buf(kPublicKeyPem, -1);
    if (!bio) return false;

    EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        qWarning() << "[License] 公钥加载失败";
        return false;
    }

    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    bool ok = false;
    if (EVP_DigestVerifyInit(ctx, nullptr, nullptr, nullptr, pkey) == 1) {
        ok = (EVP_DigestVerify(ctx,
                reinterpret_cast<const unsigned char*>(sig.constData()),
                static_cast<size_t>(sig.size()),
                reinterpret_cast<const unsigned char*>(payload.constData()),
                static_cast<size_t>(payload.size())) == 1);
    }
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pkey);
    return ok;
}

bool LicenseManager::validateKeyData(const QByteArray& rawKey) const {
    // rawKey = signature(64 bytes) + payload(variable UTF-8)
    if (rawKey.size() <= 64) return false;

    QByteArray sig     = rawKey.left(64);
    QByteArray payload = rawKey.mid(64);

    // Ed25519 验签
    if (!verifyEd25519(payload, sig)) {
        qWarning() << "[License] 签名验证失败";
        return false;
    }

    // payload 格式：machine_code(16字符) + "|" + expire_date(YYYYMMDD)
    QString payloadStr = QString::fromUtf8(payload);
    QStringList parts  = payloadStr.split('|');
    if (parts.size() < 2) return false;

    QString keyMachine  = parts[0].trimmed();
    QString expireDateS = parts[1].trimmed();

    // 机器码前 12 位匹配（容忍末尾 4 位硬件小幅变动）
    if (keyMachine.left(12) != m_machineCode.left(12)) {
        qWarning() << "[License] 机器码不匹配";
        return false;
    }

    // 有效期
    QDate expireDate = QDate::fromString(expireDateS, "yyyyMMdd");
    if (!expireDate.isValid() || expireDate < QDate::currentDate()) {
        qWarning() << "[License] KEY 已过期：" << expireDateS;
        return false;
    }

    return true;
}

void LicenseManager::load() {
    QByteArray raw = loadKeyRaw();
    m_activated = !raw.isEmpty() && validateKeyData(raw);
    qDebug() << "[License] 启动验证：" << (m_activated ? "已激活" : "未激活");
}

// ──────────────────────────────────────────────────────────────────
// 公开接口
// ──────────────────────────────────────────────────────────────────

QString LicenseManager::parseCode(const QString& code,
                                   QString&       outApiUrl,
                                   QByteArray&    outKey1) const
{
    QString cleaned = code.trimmed().remove('-').remove(' ').toUpper();
    if (cleaned.isEmpty()) return "请输入激活码";

    // Base32 格式初验
    static const QRegularExpression kBase32Re("^[A-Z2-7]{24,}$");
    if (!kBase32Re.match(cleaned).hasMatch())
        return "激活码无效，请重新输入";

    // Base32 解码：格式为 api_url(UTF-8) + "|" + key1(32字节)
    QByteArray decoded = base32Decode(cleaned);
    int sep = decoded.lastIndexOf('|');
    if (sep < 0 || (decoded.size() - sep - 1) != 32)
        return "激活码无效，请重新输入";

    QString    apiUrl = QString::fromUtf8(decoded.left(sep));
    QByteArray key1   = decoded.mid(sep + 1);

    QUrl url(apiUrl);
    if (!url.isValid() || (url.scheme() != "https" && url.scheme() != "http"))
        return "激活码无效，请重新输入";

    outApiUrl = apiUrl;
    outKey1   = key1;
    return {};
}

QString LicenseManager::verifyAndSave(const QString& keyB32) {
    if (keyB32.trimmed().isEmpty())
        return "激活码无效，请重新输入";

    QByteArray rawKey = base32Decode(
        keyB32.trimmed().remove('-').remove(' ').toUpper());

    if (!validateKeyData(rawKey))
        return "激活码无效，请重新输入";

    if (!saveKey(rawKey))
        return "激活码无效，请重新输入";

    m_activated = true;
    qDebug() << "[License] 激活成功，KEY 已持久化";
    return {};
}
