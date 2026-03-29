#include "CacheWorker.h"
#include "CacheManager.h"
#include "FFmpegHelper.h"

#include <QImage>
#include <QImageReader>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QMetaObject>
#include <QThread>

// ============================================================
// 辅助：在 worker 所在线程发射信号
// ============================================================
static void emitSignal(CacheWorker* w, std::function<void()> fn) {
    QMetaObject::invokeMethod(w, fn, Qt::QueuedConnection);
}

// ============================================================
// ThumbnailTask
// ============================================================

void ThumbnailTask::run() {
    CacheWorker* w = m_worker;
    if (w->isCancelled()) {
        // 即使跳过，也要更新计数以保证 allComplete 能正确触发
        int pending = w->m_pendingCount.fetchAndAddRelaxed(-1) - 1;
        int done    = w->m_completedCount.fetchAndAddRelaxed(1) + 1;
        int total   = w->m_totalCount.loadRelaxed();
        emitSignal(w, [w, done, total, pending]() {
            emit w->progressUpdated(done, total);
            if (pending <= 0) emit w->allComplete();
        });
        return;
    }

    QString projectId = w->m_projectId;
    QString filePath  = m_file.path;
    QString thumbPath = CacheManager::thumbnailPath(projectId, filePath);

    bool success = false;

    if (QFile::exists(thumbPath)) {
        success = true;  // 已存在，直接视为成功
    } else {
        CacheManager::ensureProjectCacheDir(projectId);

        QImageReader reader(filePath);
        reader.setAutoTransform(true);
        QSize origSize = reader.size();
        if (origSize.isValid())
            reader.setScaledSize(origSize.scaled(2000, 2000, Qt::KeepAspectRatio));

        QImage img = reader.read();
        if (!img.isNull())
            success = img.save(thumbPath, "JPEG", 95);
        if (!success)
            qWarning() << "图片缩略图生成失败：" << filePath;
    }

    int pending   = w->m_pendingCount.fetchAndAddRelaxed(-1) - 1;
    int completed = w->m_completedCount.fetchAndAddRelaxed(1) + 1;
    int total     = w->m_totalCount.loadRelaxed();

    emitSignal(w, [w, filePath, thumbPath, success, completed, total, pending]() {
        if (success) emit w->thumbnailReady(filePath, thumbPath);
        emit w->progressUpdated(completed, total);
        if (pending <= 0) emit w->allComplete();
    });
}

// ============================================================
// VideoPreviewTask
// ============================================================

void VideoPreviewTask::run() {
    CacheWorker* w = m_worker;

    // 通过信号量限制同时运行的 FFmpeg 进程数
    // tryAcquire 带超时，以便定期检查取消标志
    while (!w->m_videoSem->tryAcquire(1, 500)) {
        if (w->isCancelled()) {
            int pending = w->m_pendingCount.fetchAndAddRelaxed(-1) - 1;
            int done    = w->m_completedCount.fetchAndAddRelaxed(1) + 1;
            int total   = w->m_totalCount.loadRelaxed();
            emitSignal(w, [w, done, total, pending]() {
                emit w->progressUpdated(done, total);
                if (pending <= 0) emit w->allComplete();
            });
            return;
        }
    }

    // 已获取信号量槽位
    QString projectId = w->m_projectId;
    QString filePath  = m_file.path;
    QString thumbPath = CacheManager::thumbnailPath(projectId, filePath);
    QString prevPath  = CacheManager::previewPath(projectId, filePath);

    CacheManager::ensureProjectCacheDir(projectId);

    bool thumbDone = QFile::exists(thumbPath);
    bool prevDone  = QFile::exists(prevPath);

    // 生成视频缩略图
    if (!thumbDone && !w->isCancelled()) {
        double duration = m_file.duration > 0 ? m_file.duration
                                              : FFmpegHelper::getVideoDuration(filePath);
        double pos = (duration > 0) ? duration * 0.2 : 1.0;
        thumbDone = FFmpegHelper::extractFrame(filePath, thumbPath, pos, &w->m_cancelled);
        if (!thumbDone && !w->isCancelled())
            thumbDone = FFmpegHelper::extractFrame(filePath, thumbPath, 0.5, &w->m_cancelled);
    }

    if (thumbDone && !w->isCancelled()) {
        emitSignal(w, [w, filePath, thumbPath]() {
            emit w->thumbnailReady(filePath, thumbPath);
        });
    }

    // 生成视频预览
    if (!prevDone && !w->isCancelled()) {
        prevDone = FFmpegHelper::generatePreview(
            filePath, prevPath, w->m_quality, w->m_ffmpegThreads, &w->m_cancelled);
    }

    if (prevDone && !w->isCancelled()) {
        emitSignal(w, [w, filePath, prevPath]() {
            emit w->previewReady(filePath, prevPath);
        });
    }

    w->m_videoSem->release();

    int pending   = w->m_pendingCount.fetchAndAddRelaxed(-1) - 1;
    int completed = w->m_completedCount.fetchAndAddRelaxed(1) + 1;
    int total     = w->m_totalCount.loadRelaxed();

    emitSignal(w, [w, completed, total, pending]() {
        emit w->progressUpdated(completed, total);
        if (pending <= 0) emit w->allComplete();
    });
}

// ============================================================
// CacheWorker
// ============================================================

CacheWorker::CacheWorker(QObject* parent)
    : QObject(parent)
    , m_ffmpegThreads(1)
    , m_cancelled(0)
    , m_completedCount(0)
    , m_totalCount(0)
    , m_pendingCount(0)
    , m_imagePool(new QThreadPool(this))
    , m_videoPool(new QThreadPool(this))
    , m_videoSem(new QSemaphore(1))
{
    // 初始线程数：在 startCaching 时根据当前硬件动态设置
    m_imagePool->setMaxThreadCount(1);
    m_videoPool->setMaxThreadCount(2);
}

CacheWorker::~CacheWorker() {
    cancel();
    m_imagePool->waitForDone(5000);
    m_videoPool->waitForDone(5000);
    delete m_videoSem;
}

void CacheWorker::startCaching(const QString& projectId,
                                const QList<MediaFile>& files,
                                const QString& quality)
{
    m_projectId = projectId;
    m_quality   = quality;
    m_cancelled.storeRelaxed(0);
    m_completedCount.storeRelaxed(0);

    // ---- 根据硬件动态决定并发参数 ----
    int cores = qMax(1, QThread::idealThreadCount());

    // 图片线程数：最多使用一半核心，保留给系统和 UI
    int imageThreads = qMax(1, cores / 2);
    m_imagePool->setMaxThreadCount(imageThreads);

    // 同时运行的 FFmpeg 进程数：
    //   4 核以下 → 1 个；4-7 核 → 2 个；8+ 核 → 3 个
    int maxVideoConcurrent = (cores >= 8) ? 3 : (cores >= 4) ? 2 : 1;
    // 重建信号量前先等旧任务全部退出，避免旧任务 release() 写已释放内存
    m_videoPool->waitForDone(5000);
    delete m_videoSem;
    m_videoSem = new QSemaphore(maxVideoConcurrent);

    // 每个 FFmpeg 进程的内部线程数 = cores / 并发数（至少 1）
    m_ffmpegThreads = qMax(1, cores / maxVideoConcurrent);

    // 视频线程池容量足够大即可，并发由信号量控制
    m_videoPool->setMaxThreadCount(qMax(maxVideoConcurrent * 2, 4));

    qDebug() << "缓存参数 | 核心数:" << cores
             << "| 图片线程:" << imageThreads
             << "| 最大并发FFmpeg:" << maxVideoConcurrent
             << "| 每进程线程数:" << m_ffmpegThreads;

    // ---- 只队列化尚未缓存的文件 ----
    QList<MediaFile> imageWork, videoWork;
    for (const MediaFile& f : files) {
        if (m_cancelled.loadRelaxed()) break;
        if (f.type == MediaFile::Image) {
            if (!QFile::exists(CacheManager::thumbnailPath(projectId, f.path)))
                imageWork.append(f);
        } else if (f.type == MediaFile::Video) {
            bool needThumb   = !QFile::exists(CacheManager::thumbnailPath(projectId, f.path));
            bool needPreview = !QFile::exists(CacheManager::previewPath(projectId, f.path));
            if (needThumb || needPreview)
                videoWork.append(f);
        }
    }

    int total = imageWork.size() + videoWork.size();
    m_totalCount.storeRelaxed(total);
    m_pendingCount.storeRelaxed(total);

    if (total == 0) {
        emit allComplete();
        return;
    }

    qDebug() << "需缓存：图片" << imageWork.size() << "个，视频" << videoWork.size() << "个";

    for (const MediaFile& f : imageWork) {
        if (m_cancelled.loadRelaxed()) break;
        m_imagePool->start(new ThumbnailTask(this, f), 5);
    }
    for (const MediaFile& f : videoWork) {
        if (m_cancelled.loadRelaxed()) break;
        m_videoPool->start(new VideoPreviewTask(this, f), 1);
    }
}

void CacheWorker::cancel() {
    m_cancelled.storeRelaxed(1);
    qDebug() << "缓存生成任务已取消（等待任务在 ≤500ms 内自行退出）";
}
