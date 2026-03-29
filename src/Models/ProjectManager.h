#pragma once

#include <QObject>
#include <QList>
#include "Project.h"

/**
 * @brief 项目管理器（单例模式）
 * 负责项目数据的增删改查和持久化存储
 */
class ProjectManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取单例实例
     */
    static ProjectManager* instance();

    /**
     * @brief 获取所有项目列表
     */
    QList<Project> projects() const;

    /**
     * @brief 根据 ID 获取项目
     */
    Project* projectById(const QString& id);

    /**
     * @brief 添加新项目（指定文件夹路径）
     * @return 新项目的 ID，失败返回空字符串
     */
    QString addProject(const QString& folderPath);

    /**
     * @brief 删除项目（通过 ID）
     */
    void removeProject(const QString& id);

    /**
     * @brief 更新项目信息
     */
    void updateProject(const Project& project);

    /**
     * @brief 获取数据存储路径
     */
    static QString storagePath();

signals:
    /**
     * @brief 新项目被添加
     */
    void projectAdded(const Project& project);

    /**
     * @brief 项目信息被更新
     */
    void projectUpdated(const Project& project);

    /**
     * @brief 项目被删除
     */
    void projectRemoved(const QString& id);

private:
    explicit ProjectManager(QObject* parent = nullptr);
    ~ProjectManager() = default;

    // 禁止拷贝和赋值
    ProjectManager(const ProjectManager&) = delete;
    ProjectManager& operator=(const ProjectManager&) = delete;

    /**
     * @brief 从文件加载项目数据
     */
    void load();

    /**
     * @brief 保存项目数据到文件
     */
    void save();

    QList<Project> m_projects;  // 项目列表
    static ProjectManager* s_instance;  // 单例实例
};
