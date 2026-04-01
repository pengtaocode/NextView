#include "FFmpegHelper.h"

#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QStandardPaths>
#include <QCoreApplication>

// 轮询间隔（ms）：每 300ms 检查一次取消标志
static constexpr int kPollMs = 300;

/**
 * @brief 启动进程并等待结束，支持中途取消
 */
static bool startAndWait(QProcess& proc, int timeoutMs, QAtomicInt* cancelled) {
    if (!proc.waitForStarted(5000)) return false;

    int elapsed = 0;
    while (true) {
        if (proc.waitForFinished(kPollMs)) break;  // 正常结束

        elapsed += kPollMs;
        if (cancelled && cancelled->loadRelaxed()) {
            proc.kill();
            proc.waitForFinished(3000);
            return false;
        }
        if (elapsed >= timeoutMs) {
            qWarning() << "FFmpeg/FFprobe 超时，正在终止";
            proc.kill();
            proc.waitForFinished(3000);
            return false;
        }
    }
    return true;
}

bool FFmpegHelper::isAvailable() {
    QProcess proc;
    proc.start(ffmpegPath(), {"-version"});
    return proc.waitForStarted(3000) && proc.waitForFinished(5000) && proc.exitCode() == 0;
}

QString FFmpegHelper::ffmpegPath() {
    QString path = QStandardPaths::findExecutable("ffmpeg");
    if (!path.isEmpty()) return path;

#ifdef Q_OS_WIN
    // 优先查找与 exe 同目录的 ffmpeg.exe（单文件打包场景）
    QString exeDir = QCoreApplication::applicationDirPath();
    {
        QString c = exeDir + "/ffmpeg.exe";
        if (QFile::exists(c)) return c;
    }
    QString home = QDir::homePath();
    QDir fileDir(home + "/file");
    if (fileDir.exists()) {
        for (const QString& sub : fileDir.entryList({"ffmpeg*"}, QDir::Dirs)) {
            QString c = fileDir.absoluteFilePath(sub) + "/bin/ffmpeg.exe";
            if (QFile::exists(c)) return c;
        }
    }
    QDir dlDir(home + "/Downloads");
    if (dlDir.exists()) {
        for (const QString& sub : dlDir.entryList({"ffmpeg*"}, QDir::Dirs)) {
            QString c = dlDir.absoluteFilePath(sub) + "/bin/ffmpeg.exe";
            if (QFile::exists(c)) return c;
        }
    }
    for (const QString& c : QStringList{
             "C:/ffmpeg/bin/ffmpeg.exe",
             "C:/Program Files/ffmpeg/bin/ffmpeg.exe",
             home + "/ffmpeg/bin/ffmpeg.exe"}) {
        if (QFile::exists(c)) return c;
    }
#endif
#ifdef Q_OS_MAC
    for (const QString& c : QStringList{
             "/opt/homebrew/bin/ffmpeg", "/usr/local/bin/ffmpeg", "/opt/local/bin/ffmpeg"}) {
        if (QFile::exists(c)) return c;
    }
#endif
    return "ffmpeg";
}

QString FFmpegHelper::ffprobePath() {
    QString path = QStandardPaths::findExecutable("ffprobe");
    if (!path.isEmpty()) return path;
    QString dir = QFileInfo(ffmpegPath()).absolutePath();
#ifdef Q_OS_WIN
    QString c = dir + "/ffprobe.exe";
#else
    QString c = dir + "/ffprobe";
#endif
    return QFile::exists(c) ? c : "ffprobe";
}

QString FFmpegHelper::runFFprobe(const QStringList& args, int timeoutMs, QAtomicInt* cancelled) {
    QProcess proc;
    proc.start(ffprobePath(), args);
    if (!startAndWait(proc, timeoutMs, cancelled)) {
        qWarning() << "FFprobe 执行失败";
        return QString();
    }
    return QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
}

bool FFmpegHelper::runFFmpeg(const QStringList& args, int timeoutMs, QAtomicInt* cancelled) {
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(ffmpegPath(), args);
    if (!startAndWait(proc, timeoutMs, cancelled)) return false;

    int exitCode = proc.exitCode();
    if (exitCode != 0) {
        QByteArray err = proc.readAllStandardError();
        qWarning() << "FFmpeg 失败，退出码：" << exitCode
                   << "\n命令：" << ffmpegPath() << args.join(" ")
                   << "\n错误：" << err.right(400);
        return false;
    }
    return true;
}

double FFmpegHelper::getVideoDuration(const QString& videoPath) {
    if (!QFile::exists(videoPath)) return -1.0;
    QStringList args = {"-v","quiet","-show_entries","format=duration","-of","csv=p=0",videoPath};
    QString out = runFFprobe(args);
    bool ok = false;
    double d = out.toDouble(&ok);
    return ok ? d : -1.0;
}

QSize FFmpegHelper::getVideoSize(const QString& videoPath) {
    if (!QFile::exists(videoPath)) return {};
    QStringList args = {"-v","quiet","-select_streams","v:0",
                        "-show_entries","stream=width,height","-of","csv=p=0",videoPath};
    QString out = runFFprobe(args);
    QStringList parts = out.split(',');
    if (parts.size() < 2) return {};
    bool wOk, hOk;
    int w = parts[0].trimmed().toInt(&wOk);
    int h = parts[1].trimmed().toInt(&hOk);
    return (wOk && hOk && w > 0 && h > 0) ? QSize(w, h) : QSize();
}

bool FFmpegHelper::extractFrame(const QString& videoPath,
                                const QString& outputJpg,
                                double positionSeconds,
                                QAtomicInt* cancelled)
{
    if (!QFile::exists(videoPath)) return false;
    QDir().mkpath(QFileInfo(outputJpg).absolutePath());

    QStringList args = {
        "-ss", QString::number(positionSeconds, 'f', 2),
        "-i", videoPath,
        "-frames:v", "1",
        "-vf", "scale=w=min(iw\\,1280):h=-2",  // 限制缩略图最大宽度，避免全分辨率图像占满内存
        "-q:v", "2",
        "-threads", "1",     // 限制内部线程，避免单进程占满 CPU
        "-loglevel", "quiet",
        "-y", outputJpg
    };
    return runFFmpeg(args, 30000, cancelled);
}

int FFmpegHelper::qualityToWidth(const QString& quality) {
    if (quality == "low")  return 480;
    if (quality == "high") return 1080;
    return 720;
}

bool FFmpegHelper::generatePreview(const QString& videoPath,
                                   const QString& outputMp4,
                                   const QString& quality,
                                   int ffmpegThreads,
                                   QAtomicInt* cancelled)
{
    if (!QFile::exists(videoPath)) return false;
    QDir().mkpath(QFileInfo(outputMp4).absolutePath());

    // 传入 cancelled，避免取消后 FFprobe 仍等待最多 15s
    double duration = [&]() -> double {
        if (!QFile::exists(videoPath)) return -1.0;
        QStringList a = {"-v","quiet","-show_entries","format=duration","-of","csv=p=0",videoPath};
        QString out = runFFprobe(a, 15000, cancelled);
        bool ok = false;
        double d = out.toDouble(&ok);
        return ok ? d : -1.0;
    }();
    if (duration <= 0) return false;
    if (cancelled && cancelled->loadRelaxed()) return false;

    int width = qualityToWidth(quality);

    // FFmpeg 内部线程数：由调用方根据硬件动态决定，默认 1
    QString threads = QString::number(qMax(1, ffmpegThreads));

    QStringList codecArgs = {
        "-threads", threads,
        "-c:v", "libx264",
        "-crf", "7",
        "-preset", "fast",
        "-tune", "fastdecode",
        "-pix_fmt", "yuv420p"
    };

    // 短视频：直接转换全程
    if (duration < 10.0) {
        QStringList args = {"-i", videoPath,
                            "-vf", QString("scale=%1:-2").arg(width),
                            "-an"};
        args << codecArgs << "-loglevel" << "quiet" << "-y" << outputMp4;
        return runFFmpeg(args, 60000, cancelled);
    }

    // 截取 20%/40%/60%/80% 各 2 秒拼接
    // 关键：每个片段用独立的 -ss 放在 -i 之前（容器级快速跳转），
    // 避免 trim 滤镜从头解码整个视频导致卡住
    double d1 = duration * 0.20, d2 = duration * 0.40;
    double d3 = duration * 0.60, d4 = duration * 0.80;
    double seg = qMin(2.0, duration - d4 - 0.1);
    if (seg < 0.5) seg = 0.5;

    // 四路独立输入，各自在 -i 前 -ss 快速跳转，
    // filter_complex 仅需对已定位的短片段做 trim 精确裁剪
    QString fc = QString(
        "[0:v]trim=duration=%5,setpts=PTS-STARTPTS,scale=%6:-2[v1];"
        "[1:v]trim=duration=%5,setpts=PTS-STARTPTS,scale=%6:-2[v2];"
        "[2:v]trim=duration=%5,setpts=PTS-STARTPTS,scale=%6:-2[v3];"
        "[3:v]trim=duration=%5,setpts=PTS-STARTPTS,scale=%6:-2[v4];"
        "[v1][v2][v3][v4]concat=n=4:v=1:a=0[out]"
    ).arg(seg,0,'f',3).arg(width);

    QStringList args = {
        "-ss", QString::number(d1,'f',3), "-i", videoPath,
        "-ss", QString::number(d2,'f',3), "-i", videoPath,
        "-ss", QString::number(d3,'f',3), "-i", videoPath,
        "-ss", QString::number(d4,'f',3), "-i", videoPath,
        "-filter_complex", fc, "-map", "[out]", "-an"
    };
    args << codecArgs << "-loglevel" << "quiet" << "-y" << outputMp4;

    bool ok = runFFmpeg(args, 120000, cancelled);
    if (!ok && !(cancelled && cancelled->loadRelaxed())) {
        // libx264 不可用时降级为 mpeg4
        QStringList args2 = {
            "-ss", QString::number(d1,'f',3), "-i", videoPath,
            "-ss", QString::number(d2,'f',3), "-i", videoPath,
            "-ss", QString::number(d3,'f',3), "-i", videoPath,
            "-ss", QString::number(d4,'f',3), "-i", videoPath,
            "-filter_complex", fc, "-map", "[out]", "-an",
            "-threads", threads,
            "-c:v", "mpeg4", "-q:v", "6",
            "-loglevel", "quiet", "-y", outputMp4
        };
        ok = runFFmpeg(args2, 120000, cancelled);
    }
    return ok;
}
