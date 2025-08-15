#pragma once
#include <QMainWindow>
#include <QPointer>
#include <QHash>
#include <QSet>
#include <QAction>
#include <QVector>
#include<QTextDocument>
#include <QTimer>   
#include <QRegularExpression>
#include <QTextCursor>
#include <QTextEdit>
#include <QTableWidgetItem>
#include <QColor>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QMessageBox>


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
    explicit MainWindow(QWidget* parent = nullptr);

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

    // 查找结果：双击定位
    void onFindResultActivated(int row, int col);

    void onEditorTextChanged();  // 文本改变 -> 启动防抖
    void reparseFromEditor();    // 到点重算并刷新

    void highlightRangeColored(int start, int end, const QColor& bg);
    void onPropTableCellClicked(int row, int col);
private:
    QList<QString> recentFiles_;  // 用于存储最近打开的文件路径
    void updateRecentFilesMenu();  // 更新最近打开文件的菜单
    void openRecentFile(const QString& filePath);  // 打开最近的文件
    QMenu* mRecentFilesMenu_;     // 最近文件菜单
    // UI
    QPlainTextEdit* editor_;
    QPointer<QTreeView> classTree_;
    QPointer<QTableWidget> propTable_;
    QPointer<QStandardItemModel> classModel_;
    QLabel* lblPos_;
    QLabel* lblSize_;
    QString lastReplaceText_;

    // （新增）查找结果列表
    QPointer<QTableWidget> findResults_;

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

    // 覆写事件过滤器：处理 Ctrl+点击、Ctrl+移动改鼠标样式
    bool eventFilter(QObject* obj, QEvent* ev) override;


    //Ctrl+点击时的跳转逻辑（viewPos 为 viewport 坐标）
    bool ctrlClickJumpToInstance(const QPoint& viewPos);

    // 从“光标所在行的某位置”提取 #数字（若 outId 非空则回写）
    bool extractHashIdAtCursor(const QTextCursor& cur, int* outId) const;

    // 在 pos 处“选中并高亮” #id= 这一小段
    void highlightIdTokenAt(int pos, int id);

    // 高亮辅助
    void highlightOccurrences(const QString& token); // 高亮 "CLASS(" 或其它 token
    void clearHighlights();
    void enableGfcSyntaxColors();

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
    QHash<QString, int> inclusiveCountCamel_;   // 包含子类总数

    // 实例列表（key 用 CamelCase）
    QHash<QString, QVector<GfcInstanceRef>> instancesByCamel_;

    // ==== 辅助 ====
    void prepareSchemaIndex();  // 从 schema_ 构建 lowerToCamel_

    // （新增）把全文匹配结果填充到结果表（不改变原有查找/替换逻辑）
    void runFindAll(const QString& pattern, QTextDocument::FindFlags flags);

    QTimer* editRefreshTimer_ = nullptr;  // ★ 新增：文本编辑防抖
    bool suppressReparse_ = false;        // ★ 新增：程序性改文本时抑制重算

    // ★ 新增：把“从文本重算计数/实例”的逻辑抽出来，供打开文件 & 编辑时复用
    struct RecomputeStats {
        int instances = 0;
        int unknown = 0;
        int mappedCls = 0;
    };
    RecomputeStats recomputeFromText(const QString& text);

    void showInstanceByPos(int pos, bool moveCaret = true);
    void highlightRange(int start, int end);
    QString camelFromUpper(const QString& upper) const;
    QStringList schemaAttrNames(const QString& camel) const;
    void showParsedInstanceProperties(const ParsedInstance& pi, const QString& camel);
    QList<QTextEdit::ExtraSelection> currentSelections_;

    QString findSchemaExpNearCMake();
    void autoLoadSchemaOnStartup();


    QPair<int, int> paramRangeInInstance(const ParsedInstance& pi, int paramIndex, const QString& wholeText);

    ParsedInstance currentInstance_;
};