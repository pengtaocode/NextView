#include <QApplication>
#include <QFont>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QIcon>
#include <QPixmap>
#include <QLocalServer>
#include <QLocalSocket>
#ifdef Q_OS_WIN
#  include <windows.h>
#endif
#include "MainWindow.h"
#include "ProjectManager.h"
#include "MediaFile.h"
#include "ScanWorker.h"

static const QString kSingleInstanceKey = "NextViewApp_SingleInstance";

int main(int argc, char* argv[]) {
    // 开启高 DPI 缩放支持
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    QApplication app(argc, argv);

    // ── 单实例检测 ────────────────────────────────────────────────
    // 尝试连接已运行的实例；连接成功则通知其激活窗口后直接退出
    {
        QLocalSocket probe;
        probe.connectToServer(kSingleInstanceKey);
        if (probe.waitForConnected(300)) {
            probe.write("activate\n");
            probe.flush();
            probe.waitForBytesWritten(500);
            return 0;
        }
    }
    // 当前进程是第一个实例，启动本地服务器等待后续实例连接
    QLocalServer singleInstServer;
    QLocalServer::removeServer(kSingleInstanceKey);  // 清理可能残留的 socket 文件
    singleInstServer.listen(kSingleInstanceKey);

    // 设置应用元信息
    app.setApplicationName("NextView");
    app.setOrganizationName("NextView");
    app.setApplicationVersion("1.0.0");

    // 应用图标（从资源中读取 placeholder，缩放到 256×256 后用作多尺寸图标）
    {
        QPixmap src(":/icons/placeholder.png");
        if (!src.isNull()) {
            QIcon icon;
            for (int sz : {16, 32, 48, 256})
                icon.addPixmap(src.scaled(sz, sz, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            app.setWindowIcon(icon);
        }
    }

    // 全局字体：Segoe UI（中文自动 fallback 到微软雅黑）
    QFont appFont("Segoe UI", 10);
    appFont.setStyleStrategy(
        static_cast<QFont::StyleStrategy>(QFont::PreferAntialias | QFont::PreferQuality));
    app.setFont(appFont);

    // 所有按钮字体统一设置为粗体
    QFont btnFont = appFont;
    btnFont.setBold(true);
    app.setFont(btnFont, "QPushButton");

    // 确保应用数据目录存在
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataDir);
    QDir().mkpath(appDataDir + "/cache");

    qDebug() << "应用数据目录：" << appDataDir;

    // 初始化 ProjectManager 单例（触发加载保存的项目）
    ProjectManager::instance();

    // 创建并显示主窗口
    MainWindow window;
    window.show();

    // 收到第二个实例的激活请求时，将窗口带回前台
    QObject::connect(&singleInstServer, &QLocalServer::newConnection, &app, [&]() {
        QLocalSocket* socket = singleInstServer.nextPendingConnection();
        if (socket) socket->deleteLater();

        if (window.isMinimized())
            window.showNormal();
        window.raise();
        window.activateWindow();
#ifdef Q_OS_WIN
        SetForegroundWindow(reinterpret_cast<HWND>(window.winId()));
#endif
    });

    return app.exec();
}
