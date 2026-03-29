#pragma once

#include <QString>
#include <QSize>
#include <QAtomicInt>

/**
 * @brief FFmpeg 工具类
 * 通过 QProcess 调用外部 ffmpeg/ffprobe 程序处理视频
 */
class FFmpegHelper {
public:
    static bool isAvailable();
    static QString ffmpegPath();
    static QString ffprobePath();

    static double getVideoDuration(const QString& videoPath);
    static QSize  getVideoSize(const QString& videoPath);

    /**
     * @brief 提取视频帧为 JPG
     * @param cancelled 取消标志，非空时每 500ms 检查一次，置位则杀进程返回 false
     */
    static bool extractFrame(const QString& videoPath,
                             const QString& outputJpg,
                             double positionSeconds,
                             QAtomicInt* cancelled = nullptr);

    /**
     * @brief 生成视频预览片段
     * @param ffmpegThreads FFmpeg 内部线程数（0 = 不限制）
     * @param cancelled     取消标志
     */
    static bool generatePreview(const QString& videoPath,
                                const QString& outputMp4,
                                const QString& quality = "medium",
                                int ffmpegThreads = 0,
                                QAtomicInt* cancelled = nullptr);

private:
    static int     qualityToWidth(const QString& quality);
    static QString runFFprobe(const QStringList& args,
                              int timeoutMs = 15000,
                              QAtomicInt* cancelled = nullptr);
    static bool    runFFmpeg(const QStringList& args,
                             int timeoutMs = 120000,
                             QAtomicInt* cancelled = nullptr);
};
