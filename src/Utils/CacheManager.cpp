#include "CacheManager.h"

#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>
#include <QDirIterator>

CacheManager* CacheManager::instance() {
    static CacheManager inst;
    return &inst;
}

QString CacheManager::cacheRootDir() {
    // 使用应用数据目录下的 cache 子目录
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appDataDir + "/cache";
}

QString CacheManager::projectCacheDir(const QString& projectId) {
    return cacheRootDir() + "/" + projectId;
}

QString CacheManager::hashFilePath(const QString& filePath) {
    // 使用 MD5 哈希对文件路径进行编码，避免路径中的特殊字符问题
    QByteArray hash = QCryptographicHash::hash(
        filePath.toUtf8(),
        QCryptographicHash::Md5
    );
    return QString::fromLatin1(hash.toHex());
}

QString CacheManager::thumbnailPath(const QString& projectId, const QString& filePath) {
    QString hash = hashFilePath(filePath);
    return projectCacheDir(projectId) + "/thumb_" + hash + ".jpg";
}

QString CacheManager::previewPath(const QString& projectId, const QString& filePath) {
    QString hash = hashFilePath(filePath);
    return projectCacheDir(projectId) + "/preview_" + hash + ".mp4";
}

bool CacheManager::ensureProjectCacheDir(const QString& projectId) {
    QString dirPath = projectCacheDir(projectId);
    QDir dir;
    if (!dir.exists(dirPath)) {
        bool ok = dir.mkpath(dirPath);
        if (!ok) {
            qWarning() << "无法创建项目缓存目录：" << dirPath;
        }
        return ok;
    }
    return true;
}

bool CacheManager::deleteProjectCache(const QString& projectId) {
    QString dirPath = projectCacheDir(projectId);
    QDir dir(dirPath);
    if (!dir.exists()) {
        return true; // 目录不存在，视为成功
    }

    bool ok = dir.removeRecursively();
    if (!ok) {
        qWarning() << "无法删除项目缓存目录：" << dirPath;
    } else {
        qDebug() << "已删除项目缓存：" << projectId;
    }
    return ok;
}

bool CacheManager::deleteAllCache() {
    QString rootDir = cacheRootDir();
    QDir dir(rootDir);
    if (!dir.exists()) {
        return true;
    }

    // 只删除子目录（每个子目录对应一个项目），保留根目录
    bool allOk = true;
    QStringList subdirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString& subdir : subdirs) {
        QString subdirPath = rootDir + "/" + subdir;
        QDir sd(subdirPath);
        if (!sd.removeRecursively()) {
            qWarning() << "无法删除缓存目录：" << subdirPath;
            allOk = false;
        }
    }

    if (allOk) {
        qDebug() << "已清除所有缓存";
    }
    return allOk;
}

qint64 CacheManager::dirSize(const QString& dirPath) {
    qint64 total = 0;
    QDirIterator it(dirPath, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (it.fileInfo().isFile()) {
            total += it.fileInfo().size();
        }
    }
    return total;
}

qint64 CacheManager::calculateCacheSize(const QString& projectId) {
    QString dirPath = projectCacheDir(projectId);
    if (!QDir(dirPath).exists()) {
        return 0;
    }
    return dirSize(dirPath);
}

qint64 CacheManager::calculateTotalCacheSize() {
    QString rootDir = cacheRootDir();
    if (!QDir(rootDir).exists()) {
        return 0;
    }
    return dirSize(rootDir);
}
