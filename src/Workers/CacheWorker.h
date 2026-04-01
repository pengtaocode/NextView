#pragma once

#include <QObject>
#include <QList>
#include <QString>
#include <QThreadPool>
#include <QSemaphore>
#include <QAtomicInt>
#include <QMutex>
#include "MediaFile.h"

/**
 * @brief 缓存生成工作对象
 *
 * 架构：
 *  - 图片缩略图：私有线程池，并发数 = max(1, cores/2)
 *  - 视频缩略图+预览：通过信号量限制同时运行的 FFmpeg 进程数，
 *    避免多进程并发时 CPU 过载导致卡死
 *  - 取消：设置 m_cancelled 标志后，所有任务在下次轮询（≤300ms）时
 *    主动 kill 正在运行的 FFmpeg 进程并退出
 */
class CacheWorker : public QObject {
    Q_OBJECT

public:
    explicit CacheWorker(QObject* parent = nullptr);
    ~CacheWorker();

    /**
     * @brief 开始为指定项目的未缓存媒体文件生成缓存
     *        已存在缓存的文件会被自动跳过
     */
    void startCaching(const QString& projectId,
                      const QList<MediaFile>& files,
                      const QString& quality = "medium");

    void cancel();
    bool isCancelled() const { return m_cancelled.loadRelaxed() != 0; }

signals:
    void thumbnailReady(const QString& filePath, const QString& thumbnailPath);
    void previewReady(const QString& filePath, const QString& previewPath);
    void progressUpdated(int completed, int total);
    void allComplete();

private:
    QString    m_projectId;
    QString    m_quality;
    int        m_ffmpegThreads;   // 每个 FFmpeg 进程的内部线程数

    QAtomicInt m_cancelled;
    QAtomicInt m_completedCount;
    QAtomicInt m_totalCount;
    QAtomicInt m_pendingCount;

    QThreadPool* m_imagePool;     // 图片缩略图线程池
    QThreadPool* m_videoPool;     // 视频任务线程池（数量已受信号量限制）
    QSemaphore*  m_videoSem;      // 限制同时运行的 FFmpeg 进程数

    friend class ThumbnailTask;
    friend class VideoPreviewTask;
};

// ---------- QRunnable 任务 ----------

class ThumbnailTask : public QRunnable {
public:
    ThumbnailTask(CacheWorker* worker, const MediaFile& file)
        : m_worker(worker), m_file(file) { setAutoDelete(true); }
    void run() override;
private:
    CacheWorker* m_worker;
    MediaFile    m_file;
};

class VideoPreviewTask : public QRunnable {
public:
    VideoPreviewTask(CacheWorker* worker, const MediaFile& file, bool generatePreview = true)
        : m_worker(worker), m_file(file), m_generatePreview(generatePreview)
    { setAutoDelete(true); }
    void run() override;
private:
    CacheWorker* m_worker;
    MediaFile    m_file;
    bool         m_generatePreview; // 未激活且超出限额时为 false，仅生成缩略图
};
