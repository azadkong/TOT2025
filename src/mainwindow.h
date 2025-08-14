#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QHash>
#include <QSet>
#include <QAction>
#include <QVector>

class QPlainTextEdit;
class QTreeView;
class QTableWidget;
class QStandardItemModel;
class QLabel;

#include "expressparser.h"
#include "gfcparser.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);

private slots:

    // 文件
    void newFile();
    void openGfc();
    void saveGfc();
    void saveGfcAs();
    void openSchemaExp();

    // 视图/工具
    void toggleClassDock(bool checked);
    void togglePropDock(bool checked);
    void toggleToolbar(bool checked);
    void toggleStatusbar(bool checked);

    // 编辑
    void doFind();
    void doFindNext();
    void doReplace();

    // 交互
    void onCursorPosChanged();
    void onClassTreeClicked(const QModelIndex& idx);

    // 导航
    void goBack();
    void goForward();
    void locateAtCursor();

private:
    // UI
    QPlainTextEdit* editor_;
    QPointer<QTreeView> classTree_;
    QPointer<QTableWidget> propTable_;
    QPointer<QStandardItemModel> classModel_;
    QLabel* lblPos_;
    QLabel* lblSize_;
    QString lastReplaceText_;

    // 状态
    QString currentFilePath_;
    QString currentSchemaPath_;
    ExpressParser schema_;
    QHash<QString, QSet<QString>> children_; // 继承子类映射
    QHash<QString, int> classCounts_;        // 该GFC中每类的直接实例数（不含子类）
    QString lastFindText_;

        // 导航状态
    QAction* actBack_{};
    QAction* actForward_{};
    QAction* actLocate_{};
    QVector<int> navBackStack_{};
    QVector<int> navFwdStack_{};
    void updateNavActions();
    int findInstancePosition(int id) const;
    void navigateTo(int pos, bool fromBackOrForward);

// 构建UI
    void buildMenusAndToolbar();
    void showHelpDocument(); 
    void showAboutDialog();
    void buildDocks();
    void buildStatusBar();

    // 文件相关
    bool loadGfcFromFile(const QString& path);
    bool saveGfcToFile(const QString& path);
    void updateWindowTitle();
    void rebuildClassTree();                 // 依据 schema_ + classCounts_ 构树
    int  computeInclusiveCount(const QString& cls) const; // 递归计算含子类总数

    // 高亮辅助
    void highlightOccurrences(const QString& token); // 高亮 "CLASS(" 或其它 token
    void clearHighlights();

    // 属性区
    void showClassProperties(const QString& cls);

    // 在 class MainWindow 里 private: 区域补充
    QVector<GfcInstanceRef> instanceRefs_;
    QHash<QString, QVector<GfcInstanceRef>> instancesByClass_;

    enum {
        RoleClassName = Qt::UserRole + 1,
        RoleNodeType = Qt::UserRole + 2,
        RoleDocPos = Qt::UserRole + 3,
        NodeClass = 1,
        NodeInstance = 2
    };

    void updateParentInstances(const QString& cls);

    // ==== 新增：用于大小写无关匹配（不改变展示用的驼峰原名） ====
// lower -> CamelCase(展示名)
    QHash<QString, QString> lowerToCamel_;

    // 计数（key 一律用 CamelCase，与 schema_.classes() 的 key 一致）
    QHash<QString, int> directCountCamel_;      // 本类直接实例数
    QHash<QString, int> inclusiveCountCamel_;   // 包含子类的实例总数（由下往上累加）

    // 实例列表（key 用 CamelCase）
    QHash<QString, QVector<GfcInstanceRef>> instancesByCamel_;

    // ==== 辅助 ====
    void prepareSchemaIndex();  // 从 schema_ 构建 lowerToCamel_

};
