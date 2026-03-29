#include "ProjectManager.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>

// 静态单例实例初始化
ProjectManager* ProjectManager::s_instance = nullptr;

ProjectManager* ProjectManager::instance() {
    if (!s_instance) {
        s_instance = new ProjectManager();
    }
    return s_instance;
}

ProjectManager::ProjectManager(QObject* parent)
    : QObject(parent)
{
    // 确保存储目录存在
    QString dirPath = QFileInfo(storagePath()).absolutePath();
    QDir().mkpath(dirPath);

    // 加载已有项目数据
    load();
}

QString ProjectManager::storagePath() {
    // 使用应用数据目录存储 projects.json
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return appDataDir + "/projects.json";
}

QList<Project> ProjectManager::projects() const {
    return m_projects;
}

Project* ProjectManager::projectById(const QString& id) {
    for (auto& p : m_projects) {
        if (p.id == id) {
            return &p;
        }
    }
    return nullptr;
}

QString ProjectManager::addProject(const QString& folderPath) {
    // 检查路径是否有效
    QDir dir(folderPath);
    if (!dir.exists()) {
        qWarning() << "项目文件夹不存在：" << folderPath;
        return QString();
    }

    // 检查是否已存在相同路径的项目
    for (const auto& p : m_projects) {
        if (QDir(p.folderPath) == QDir(folderPath)) {
            qWarning() << "该文件夹已作为项目存在：" << folderPath;
            return p.id;
        }
    }

    // 创建新项目
    Project project = Project::create(folderPath);
    m_projects.append(project);

    // 立即保存
    save();

    // 发出信号
    emit projectAdded(project);

    qDebug() << "项目已添加：" << project.name << "(" << project.id << ")";
    return project.id;
}

void ProjectManager::removeProject(const QString& id) {
    for (int i = 0; i < m_projects.size(); ++i) {
        if (m_projects[i].id == id) {
            m_projects.removeAt(i);
            save();
            emit projectRemoved(id);
            qDebug() << "项目已删除：" << id;
            return;
        }
    }
    qWarning() << "未找到要删除的项目：" << id;
}

void ProjectManager::updateProject(const Project& project) {
    for (int i = 0; i < m_projects.size(); ++i) {
        if (m_projects[i].id == project.id) {
            m_projects[i] = project;
            save();
            emit projectUpdated(project);
            return;
        }
    }
    qWarning() << "未找到要更新的项目：" << project.id;
}

void ProjectManager::load() {
    QString path = storagePath();
    QFile file(path);

    if (!file.exists()) {
        qDebug() << "项目数据文件不存在，将创建新文件：" << path;
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开项目数据文件：" << path;
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "项目数据文件 JSON 解析失败：" << error.errorString();
        return;
    }

    if (!doc.isArray()) {
        qWarning() << "项目数据文件格式错误：根节点应为数组";
        return;
    }

    m_projects.clear();
    QJsonArray arr = doc.array();
    for (const auto& val : arr) {
        if (val.isObject()) {
            Project p = Project::fromJson(val.toObject());
            if (!p.id.isEmpty()) {
                m_projects.append(p);
            }
        }
    }

    qDebug() << "已加载" << m_projects.size() << "个项目";
}

void ProjectManager::save() {
    QString path = storagePath();

    // 确保目录存在
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonArray arr;
    for (const auto& p : m_projects) {
        arr.append(p.toJson());
    }

    QJsonDocument doc(arr);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "无法保存项目数据文件：" << path;
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    qDebug() << "项目数据已保存，共" << m_projects.size() << "个项目";
}
