#pragma once
#include <QString>
#include <QByteArray>

/**
 * @brief 授权管理单例
 * 完整激活流程见 code_plan.md
 *
 * 激活分两步：
 *   1. parseCode()    —— 本地解析激活码（无网络），提取 api_url 和 key1
 *   2. verifyAndSave() —— 验证服务端返回的 KEY 并持久化（无网络）
 *      中间的 HTTPS 请求由 ActivationDialog 完成
 *
 * KEY 格式（服务端签发）：
 *   raw = Ed25519签名(64字节) + payload(UTF-8)
 *   payload = machine_code(16字符) + "|" + expire_date(YYYYMMDD)
 *   传输时以 Base32 编码
 *
 * 本地存储：AES-256-CBC(机器码派生密钥) 加密后 Base64 存入 QSettings
 */
class LicenseManager {
public:
    static LicenseManager* instance();

    static constexpr int FreeVideoPreviewLimit = 20;

    bool    isActivated() const { return m_activated; }
    QString machineCode() const { return m_machineCode; }

    /**
     * @brief 解析激活码（纯本地，无网络）
     * @param code      用户输入的激活码（含或不含连字符均可）
     * @param outApiUrl 成功时填入服务端激活 URL
     * @param outKey1   成功时填入 key1 原始字节（32字节）
     * @return 空字符串表示格式合法，否则为错误提示
     */
    QString parseCode(const QString& code,
                      QString&       outApiUrl,
                      QByteArray&    outKey1) const;

    /**
     * @brief 验证服务端返回的 KEY 并持久化（纯本地，无网络）
     * @param keyB32 服务端响应 JSON "key" 字段（Base32 编码）
     * @return 空字符串表示激活成功，否则为错误提示
     */
    QString verifyAndSave(const QString& keyB32);

private:
    LicenseManager();

    bool    m_activated  = false;
    QString m_machineCode;

    void    load();
    QString computeMachineCode() const;

    // KEY 本地加解密存储
    bool       saveKey(const QByteArray& rawKey);
    QByteArray loadKeyRaw() const;
    QByteArray deriveAesKey() const;
    QByteArray deriveAesIv()  const;

    // Ed25519 验签（公钥硬编码在 .cpp）
    static bool verifyEd25519(const QByteArray& payload, const QByteArray& sig);

    // 验证 rawKey 的签名、机器码前缀、有效期
    bool validateKeyData(const QByteArray& rawKey) const;
};
