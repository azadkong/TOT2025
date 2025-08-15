#include "mainwindow.h"

#include <QPlainTextEdit>
#include <QTreeView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QDockWidget>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QActionGroup>
#include <QInputDialog>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QVector>
#include <QHeaderView>
#include <QTextDocument>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QDebug>
#include <QPushButton>
#include <QAbstractItemView>
#include <QSyntaxHighlighter>
#include <QTimer> 
#include <QTextEdit>
#include <QColor>
#include<QApplication>

#include "gfcparser.h"

// ---------- 工具：根据平台设置UTF-8 ----------
static void setUtf8(QTextStream& ts) {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    ts.setEncoding(QStringConverter::Utf8);
#else
    ts.setCodec("utf-8");
#endif
}

//MainWindow::MainWindow(QWidget* parent)
//    : QMainWindow(parent),
//    editor_(new QPlainTextEdit(this)),
//    lblPos_(new QLabel(this)),
//    lblSize_(new QLabel(this))
//{
//    setCentralWidget(editor_);
//    buildMenusAndToolbar();
//    updateNavActions();
//
//    buildDocks();
//    buildStatusBar();
//
//    // 信号：更新状态栏
//    editor_->viewport()->setMouseTracking(true);         // 为了在 Ctrl+移动时改鼠标
//    editor_->viewport()->installEventFilter(this);       // 安装事件过滤器
//
//    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onCursorPosChanged);
//    connect(editor_, &QPlainTextEdit::textChanged, this, [this] {
//        const int bytes = editor_->toPlainText().toUtf8().size();
//        lblSize_->setText(QStringLiteral("大小: %1 KB").arg(QString::number(bytes / 1024.0, 'f', 2)));
//        });
//
//    editRefreshTimer_ = new QTimer(this);
//    editRefreshTimer_->setSingleShot(true);
//    editRefreshTimer_->setInterval(300); // 防抖毫秒，可按需调整
//
//    connect(editor_, &QPlainTextEdit::textChanged,
//        this, &MainWindow::onEditorTextChanged);
//    connect(editRefreshTimer_, &QTimer::timeout,
//        this, &MainWindow::reparseFromEditor);
//
//    updateWindowTitle();
//}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
    editor_(new QPlainTextEdit(this)),
    lblPos_(new QLabel(this)),
    lblSize_(new QLabel(this))
{
    setCentralWidget(editor_);
    buildMenusAndToolbar();
    updateNavActions();

    buildDocks();
    buildStatusBar();

    // 信号：更新状态栏
    editor_->viewport()->setMouseTracking(true);
    editor_->viewport()->installEventFilter(this);

    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onCursorPosChanged);
    connect(editor_, &QPlainTextEdit::textChanged, this, [this] {
        const int bytes = editor_->toPlainText().toUtf8().size();
        lblSize_->setText(QStringLiteral("大小: %1 KB").arg(QString::number(bytes / 1024.0, 'f', 2)));
        });

    editRefreshTimer_ = new QTimer(this);
    editRefreshTimer_->setSingleShot(true);
    editRefreshTimer_->setInterval(300);

    connect(editor_, &QPlainTextEdit::textChanged, this, &MainWindow::onEditorTextChanged);
    connect(editRefreshTimer_, &QTimer::timeout, this, &MainWindow::reparseFromEditor);

    updateWindowTitle();

    // ★★★ 自动加载 CMakeLists 同目录下的 .exp
    autoLoadSchemaOnStartup();
}


void MainWindow::onEditorTextChanged()
{
    if (suppressReparse_) return;        // 打开文件时 setPlainText 不触发重算
    if (schema_.classes().isEmpty()) return;  // 未加载 .exp 时可直接返回（或也允许重算为全0）
    editRefreshTimer_->start();          // 重启防抖计时
}

void MainWindow::reparseFromEditor()
{
    const QString text = editor_->toPlainText();
    RecomputeStats st = recomputeFromText(text);
    rebuildClassTree();                  // 你的 rebuild 已含 (0/0) 裁剪的话会生效

#ifdef QT_DEBUG
    qDebug() << "[reparseFromEditor] instances=" << st.instances
        << "unknown=" << st.unknown
        << "mappedCls=" << st.mappedCls;
#endif
    statusBar()->showMessage(
        QStringLiteral("已根据文本刷新：实例 %1，映射到 %2 个类，忽略未知类 %3。")
        .arg(st.instances).arg(st.mappedCls).arg(st.unknown),
        2000);
}

MainWindow::RecomputeStats MainWindow::recomputeFromText(const QString& text)
{
    RecomputeStats stats;

    // 1) 扫描 .gfc 实例（只提取 #idx / CLASS(大写) / pos）
    QVector<GfcInstanceRef> refs;
    GfcParser::countClasses(text, &refs);
    stats.instances = refs.size();

    // 2) 以 CamelCase 为唯一键；展示仍用驼峰
    directCountCamel_.clear();
    inclusiveCountCamel_.clear();
    instancesByCamel_.clear();

    if (schema_.classes().isEmpty()) return stats;
    if (lowerToCamel_.isEmpty()) prepareSchemaIndex();

    // 3) 直接计数 + 实例清单
    int unknown = 0;
    for (const auto& r : refs) {
        const QString camel = lowerToCamel_.value(r.cls.toLower());
        if (camel.isEmpty()) { ++unknown; continue; }
        directCountCamel_[camel] += 1;
        instancesByCamel_[camel].push_back(r);
    }
    stats.unknown = unknown;
    stats.mappedCls = directCountCamel_.size();

    // 4) 沿父链累加得到 inclusive
    inclusiveCountCamel_ = directCountCamel_;
    for (auto it = directCountCamel_.cbegin(); it != directCountCamel_.cend(); ++it) {
        QString p = schema_.classes().value(it.key()).parent; // CamelCase
        while (!p.isEmpty()) {
            inclusiveCountCamel_[p] += it.value();
            p = schema_.classes().value(p).parent;
        }
    }
    return stats;
}


// ================== UI 构建 ==================
//void MainWindow::buildMenusAndToolbar()
//{
//    // 文件
//    auto mFile = menuBar()->addMenu(QStringLiteral("文件"));
//    auto actNew = mFile->addAction(QStringLiteral("新建 (&N)"));
//    actNew->setShortcut(QKeySequence::New);
//    connect(actNew, &QAction::triggered, this, &MainWindow::newFile);
//
//    // 创建并设置“最近打开的文件”菜单，并设置objectName
//    mRecentFilesMenu_ = new QMenu(QStringLiteral("最近打开的文件"), this);
//    mRecentFilesMenu_->setObjectName(QStringLiteral("recentFilesMenu"));  // 设置 objectName
//    mFile->addMenu(mRecentFilesMenu_);
//    mRecentFilesMenu_->setEnabled(false);  // 初始化时禁用菜单
//    // 更新最近文件菜单
//    updateRecentFilesMenu();
//
//    auto actOpen = mFile->addAction(QStringLiteral("打开GFC (&O) ..."));
//    actOpen->setShortcut(QKeySequence::Open);
//    connect(actOpen, &QAction::triggered, this, &MainWindow::openGfc);
//
//    auto actSave = mFile->addAction(QStringLiteral("保存 (&S)"));
//    actSave->setShortcut(QKeySequence::Save);
//    connect(actSave, &QAction::triggered, this, &MainWindow::saveGfc);
//
//    auto actSaveAs = mFile->addAction(QStringLiteral("另存为 (&A) ..."));
//    connect(actSaveAs, &QAction::triggered, this, &MainWindow::saveGfcAs);
//
//    mFile->addSeparator();
//    auto actOpenExp = mFile->addAction(QStringLiteral("打开Schema(.exp) ..."));
//    connect(actOpenExp, &QAction::triggered, this, &MainWindow::openSchemaExp);
//
//    mFile->addSeparator();
//    auto actQuit = mFile->addAction(QStringLiteral("退出"));
//    actQuit->setShortcut(QKeySequence::Quit);
//    connect(actQuit, &QAction::triggered, this, &QWidget::close);
//
//    // 导航
//    auto mNav = menuBar()->addMenu(QStringLiteral("导航"));
//    actBack_ = mNav->addAction(QStringLiteral("后退"));
//    actBack_->setShortcut(QKeySequence::Back);
//    connect(actBack_, &QAction::triggered, this, &MainWindow::goBack);
//    actForward_ = mNav->addAction(QStringLiteral("前进"));
//    actForward_->setShortcut(QKeySequence::Forward);
//    connect(actForward_, &QAction::triggered, this, &MainWindow::goForward);
//    mNav->addSeparator();
//    actLocate_ = mNav->addAction(QStringLiteral("定位"));
//    connect(actLocate_, &QAction::triggered, this, &MainWindow::locateAtCursor);
//
//    // 编辑
//    auto mEdit = menuBar()->addMenu(QStringLiteral("编辑"));
//    auto actUndo = mEdit->addAction(QStringLiteral("撤销"));
//    actUndo->setShortcut(QKeySequence::Undo);
//    connect(actUndo, &QAction::triggered, editor_, &QPlainTextEdit::undo);
//
//    auto actRedo = mEdit->addAction(QStringLiteral("重复"));
//    actRedo->setShortcut(QKeySequence::Redo);
//    connect(actRedo, &QAction::triggered, editor_, &QPlainTextEdit::redo);
//
//    mEdit->addSeparator();
//    auto actCut = mEdit->addAction(QStringLiteral("剪切"));
//    actCut->setShortcut(QKeySequence::Cut);
//    connect(actCut, &QAction::triggered, editor_, &QPlainTextEdit::cut);
//
//    auto actCopy = mEdit->addAction(QStringLiteral("复制"));
//    actCopy->setShortcut(QKeySequence::Copy);
//    connect(actCopy, &QAction::triggered, editor_, &QPlainTextEdit::copy);
//
//    auto actPaste = mEdit->addAction(QStringLiteral("粘贴"));
//    actPaste->setShortcut(QKeySequence::Paste);
//    connect(actPaste, &QAction::triggered, editor_, &QPlainTextEdit::paste);
//
//    mEdit->addSeparator();
//    auto actFind = mEdit->addAction(QStringLiteral("查找..."));
//    actFind->setShortcut(QKeySequence::Find);
//    connect(actFind, &QAction::triggered, this, &MainWindow::doFind);
//
//    auto actFindNext = mEdit->addAction(QStringLiteral("查找下一个"));
//    actFindNext->setShortcut(QKeySequence(Qt::Key_F3));
//    connect(actFindNext, &QAction::triggered, this, &MainWindow::doFindNext);
//
//    auto actReplace = mEdit->addAction(QStringLiteral("替换..."));
//    actReplace->setShortcut(QKeySequence::Replace);
//    connect(actReplace, &QAction::triggered, this, &MainWindow::doReplace);
//
//    // 工具
//    auto mView = menuBar()->addMenu(QStringLiteral("工具"));
//    auto actToolbar = mView->addAction(QStringLiteral("工具栏"));
//    actToolbar->setCheckable(true);
//    actToolbar->setChecked(true);
//    connect(actToolbar, &QAction::triggered, this, &MainWindow::toggleToolbar);
//
//    auto actClassDock = mView->addAction(QStringLiteral("类视图(视图区)"));
//    actClassDock->setCheckable(true);
//    actClassDock->setChecked(true);
//    connect(actClassDock, &QAction::triggered, this, &MainWindow::toggleClassDock);
//
//    auto actPropDock = mView->addAction(QStringLiteral("属性区"));
//    actPropDock->setCheckable(true);
//    actPropDock->setChecked(true);
//    connect(actPropDock, &QAction::triggered, this, &MainWindow::togglePropDock);
//
//    auto actStatusbar = mView->addAction(QStringLiteral("状态栏"));
//    actStatusbar->setCheckable(true);
//    actStatusbar->setChecked(true);
//    connect(actStatusbar, &QAction::triggered, this, &MainWindow::toggleStatusbar);
//
//    //帮助
//    auto mHelp = menuBar()->addMenu(QStringLiteral("帮助"));
//    auto actHelp = mHelp->addAction(QStringLiteral("帮助文档"));
//    connect(actHelp, &QAction::triggered, this, &MainWindow::showHelpDocument);
//    auto actAbout = mHelp->addAction(QStringLiteral("关于"));
//    connect(actAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);
//
//    // 工具栏（常用）
//    auto tb = addToolBar(QStringLiteral("工具栏"));
//    tb->addAction(actNew);
//    tb->addAction(actOpen);
//    tb->addAction(actSave);
//    tb->addSeparator();
//    tb->addAction(actUndo);
//    tb->addAction(actRedo);
//    tb->addSeparator();
//    tb->addAction(actFind);
//    tb->addAction(actReplace);
//    tb->addSeparator();
//    tb->addAction(actBack_);
//    tb->addAction(actForward_);
//    tb->addAction(actLocate_);
//}

void MainWindow::buildMenusAndToolbar()
{
    // 文件
    auto mFile = menuBar()->addMenu(QStringLiteral("文件"));
    auto actNew = mFile->addAction(QStringLiteral("新建 (&N)"));
    actNew->setShortcut(QKeySequence::New);
    connect(actNew, &QAction::triggered, this, &MainWindow::newFile);

    // 最近文件
    mRecentFilesMenu_ = new QMenu(QStringLiteral("最近打开的文件"), this);
    mRecentFilesMenu_->setObjectName(QStringLiteral("recentFilesMenu"));
    mFile->addMenu(mRecentFilesMenu_);
    mRecentFilesMenu_->setEnabled(false);
    updateRecentFilesMenu();

    auto actOpen = mFile->addAction(QStringLiteral("打开GFC (&O) ..."));
    actOpen->setShortcut(QKeySequence::Open);
    connect(actOpen, &QAction::triggered, this, &MainWindow::openGfc);

    auto actSave = mFile->addAction(QStringLiteral("保存 (&S)"));
    actSave->setShortcut(QKeySequence::Save);
    connect(actSave, &QAction::triggered, this, &MainWindow::saveGfc);

    auto actSaveAs = mFile->addAction(QStringLiteral("另存为 (&A) ..."));
    connect(actSaveAs, &QAction::triggered, this, &MainWindow::saveGfcAs);

    // —— 原本这里有 “打开Schema(.exp)” —— 已删除 —— //
    // mFile->addSeparator();
    // auto actOpenExp = mFile->addAction(QStringLiteral("打开Schema(.exp) ..."));
    // connect(actOpenExp, &QAction::triggered, this, &MainWindow::openSchemaExp);

    mFile->addSeparator();
    auto actQuit = mFile->addAction(QStringLiteral("退出"));
    actQuit->setShortcut(QKeySequence::Quit);
    connect(actQuit, &QAction::triggered, this, &QWidget::close);

    // 导航
    auto mNav = menuBar()->addMenu(QStringLiteral("导航"));
    actBack_ = mNav->addAction(QStringLiteral("后退"));
    actBack_->setShortcut(QKeySequence::Back);
    connect(actBack_, &QAction::triggered, this, &MainWindow::goBack);
    actForward_ = mNav->addAction(QStringLiteral("前进"));
    actForward_->setShortcut(QKeySequence::Forward);
    connect(actForward_, &QAction::triggered, this, &MainWindow::goForward);
    mNav->addSeparator();
    actLocate_ = mNav->addAction(QStringLiteral("定位"));
    connect(actLocate_, &QAction::triggered, this, &MainWindow::locateAtCursor);

    // 编辑
    auto mEdit = menuBar()->addMenu(QStringLiteral("编辑"));
    auto actUndo = mEdit->addAction(QStringLiteral("撤销"));
    actUndo->setShortcut(QKeySequence::Undo);
    connect(actUndo, &QAction::triggered, editor_, &QPlainTextEdit::undo);

    auto actRedo = mEdit->addAction(QStringLiteral("重复"));
    actRedo->setShortcut(QKeySequence::Redo);
    connect(actRedo, &QAction::triggered, editor_, &QPlainTextEdit::redo);

    mEdit->addSeparator();
    auto actCut = mEdit->addAction(QStringLiteral("剪切"));
    actCut->setShortcut(QKeySequence::Cut);
    connect(actCut, &QAction::triggered, editor_, &QPlainTextEdit::cut);

    auto actCopy = mEdit->addAction(QStringLiteral("复制"));
    actCopy->setShortcut(QKeySequence::Copy);
    connect(actCopy, &QAction::triggered, editor_, &QPlainTextEdit::copy);

    auto actPaste = mEdit->addAction(QStringLiteral("粘贴"));
    actPaste->setShortcut(QKeySequence::Paste);
    connect(actPaste, &QAction::triggered, editor_, &QPlainTextEdit::paste);

    mEdit->addSeparator();
    auto actFind = mEdit->addAction(QStringLiteral("查找..."));
    actFind->setShortcut(QKeySequence::Find);
    connect(actFind, &QAction::triggered, this, &MainWindow::doFind);

    auto actFindNext = mEdit->addAction(QStringLiteral("查找下一个"));
    actFindNext->setShortcut(QKeySequence(Qt::Key_F3));
    connect(actFindNext, &QAction::triggered, this, &MainWindow::doFindNext);

    auto actReplace = mEdit->addAction(QStringLiteral("替换..."));
    actReplace->setShortcut(QKeySequence::Replace);
    connect(actReplace, &QAction::triggered, this, &MainWindow::doReplace);

    // 工具
    auto mView = menuBar()->addMenu(QStringLiteral("工具"));
    auto actToolbar = mView->addAction(QStringLiteral("工具栏"));
    actToolbar->setCheckable(true);
    actToolbar->setChecked(true);
    connect(actToolbar, &QAction::triggered, this, &MainWindow::toggleToolbar);

    auto actClassDock = mView->addAction(QStringLiteral("类视图(视图区)"));
    actClassDock->setCheckable(true);
    actClassDock->setChecked(true);
    connect(actClassDock, &QAction::triggered, this, &MainWindow::toggleClassDock);

    auto actPropDock = mView->addAction(QStringLiteral("属性区"));
    actPropDock->setCheckable(true);
    actPropDock->setChecked(true);
    connect(actPropDock, &QAction::triggered, this, &MainWindow::togglePropDock);

    auto actStatusbar = mView->addAction(QStringLiteral("状态栏"));
    actStatusbar->setCheckable(true);
    actStatusbar->setChecked(true);
    connect(actStatusbar, &QAction::triggered, this, &MainWindow::toggleStatusbar);

    // 帮助
    auto mHelp = menuBar()->addMenu(QStringLiteral("帮助"));
    auto actHelp = mHelp->addAction(QStringLiteral("帮助文档"));
    connect(actHelp, &QAction::triggered, this, &MainWindow::showHelpDocument);
    auto actAbout = mHelp->addAction(QStringLiteral("关于"));
    connect(actAbout, &QAction::triggered, this, &MainWindow::showAboutDialog);

    // 工具栏（常用）
    auto tb = addToolBar(QStringLiteral("工具栏"));
    tb->addAction(actNew);
    tb->addAction(actOpen);
    tb->addAction(actSave);
    tb->addSeparator();
    tb->addAction(actUndo);
    tb->addAction(actRedo);
    tb->addSeparator();
    tb->addAction(actFind);
    tb->addAction(actReplace);
    tb->addSeparator();
    tb->addAction(actBack_);
    tb->addAction(actForward_);
    tb->addAction(actLocate_);
}


void MainWindow::showHelpDocument() {
    QString helpContent = "欢迎使用本程序！\n\n"
        "功能简介：\n"
        "1. 文件菜单：打开、保存、另存为等功能。\n"
        "2. 编辑菜单：查找、替换等功能。\n"
        "3. 视图菜单：切换不同的视图模式。\n"
        "4. 工具菜单：设置文本颜色、帮助等功能。\n\n"
        "更多帮助请参考官方文档。";
    QMessageBox::information(this, QStringLiteral("帮助文档"), helpContent);
}
void MainWindow::showAboutDialog() {
    QMessageBox::about(this, QStringLiteral("关于"),
        QStringLiteral("应用名称: GFC 编辑器\n")
        + QStringLiteral("版本: 1.0.0\n")
        + QStringLiteral("作者: 你的名字\n")
        + QStringLiteral("说明: 这是一个用于编辑 GFC 文件的应用。"));
}

/*void MainWindow::buildDocks()
{
    // 视图区（类继承树）
    classTree_ = new QTreeView(this);
    classTree_->setHeaderHidden(true);
    classModel_ = new QStandardItemModel(this);
    classModel_->setHorizontalHeaderLabels({ QStringLiteral("类/统计") });
    classTree_->setModel(classModel_);
    connect(classTree_, &QTreeView::clicked, this, &MainWindow::onClassTreeClicked);

    auto dockClass = new QDockWidget(QStringLiteral("视图区 - 类继承"), this);
    dockClass->setObjectName("dockClassView");
    dockClass->setWidget(classTree_);
    addDockWidget(Qt::LeftDockWidgetArea, dockClass);

    // 属性区（显示 Schema 中该类属性定义）
    propTable_ = new QTableWidget(this);
    propTable_->setColumnCount(2);
    propTable_->setHorizontalHeaderLabels({ QStringLiteral("属性名/定义"), QStringLiteral("类型/备注") });
    propTable_->horizontalHeader()->setStretchLastSection(true);

    auto dockProp = new QDockWidget(QStringLiteral("属性区"), this);
    dockProp->setObjectName("dockProp");
    dockProp->setWidget(propTable_);
    addDockWidget(Qt::RightDockWidgetArea, dockProp);

    // ==== （新增）查找结果区 ====
    findResults_ = new QTableWidget(this);
    findResults_->setColumnCount(3);
    findResults_->setHorizontalHeaderLabels({ QStringLiteral("行"), QStringLiteral("列"), QStringLiteral("内容") });
    findResults_->horizontalHeader()->setStretchLastSection(true);
    findResults_->setSelectionBehavior(QAbstractItemView::SelectRows);
    findResults_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(findResults_, &QTableWidget::cellDoubleClicked,
        this, &MainWindow::onFindResultActivated);

    auto dockFind = new QDockWidget(QStringLiteral("查找结果"), this);
    dockFind->setObjectName("dockFindResults");
    dockFind->setWidget(findResults_);
    addDockWidget(Qt::BottomDockWidgetArea, dockFind);

    // 允许浮动/停靠
    dockClass->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    dockProp->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    dockFind->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
}*/


void MainWindow::buildDocks()
{
    // 视图区（类继承树）
    classTree_ = new QTreeView(this);
    classTree_->setHeaderHidden(true);
    classModel_ = new QStandardItemModel(this);
    classModel_->setHorizontalHeaderLabels({ QStringLiteral("类/统计") });
    classTree_->setModel(classModel_);
    connect(classTree_, &QTreeView::clicked, this, &MainWindow::onClassTreeClicked);

    auto dockClass = new QDockWidget(QStringLiteral("视图区 - 类继承"), this);
    dockClass->setObjectName("dockClassView");
    dockClass->setWidget(classTree_);
    addDockWidget(Qt::LeftDockWidgetArea, dockClass);

    // 属性区
    propTable_ = new QTableWidget(this);
    propTable_->setColumnCount(2);
    propTable_->setHorizontalHeaderLabels({ QStringLiteral("属性名/定义"), QStringLiteral("类型/备注") });
    propTable_->horizontalHeader()->setStretchLastSection(true);

    auto dockProp = new QDockWidget(QStringLiteral("属性区"), this);
    dockProp->setObjectName("dockProp");
    dockProp->setWidget(propTable_);
    addDockWidget(Qt::RightDockWidgetArea, dockProp);

    // === 新增：查找结果区（底部列表，用于定位） ===
    // 说明：不新增成员变量，使用 objectName 通过 findChild 获取
    auto* resultsTable = new QTableWidget(this);
    resultsTable->setObjectName("findResultsTable");
    resultsTable->setColumnCount(3);
    resultsTable->setHorizontalHeaderLabels({ QStringLiteral("行"), QStringLiteral("列"), QStringLiteral("内容") });
    resultsTable->horizontalHeader()->setStretchLastSection(true);
    resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

    // 双击定位到对应匹配处
    connect(resultsTable, &QTableWidget::cellDoubleClicked, this,
        [this](int row, int col) {
            Q_UNUSED(col);
            auto* table = this->findChild<QTableWidget*>("findResultsTable");
            if (!table) return;
            auto* it = table->item(row, 0);
            if (!it) return;

            const int pos = it->data(Qt::UserRole).toInt();
            const int len = it->data(Qt::UserRole + 1).toInt();

            QTextCursor c(editor_->document());
            c.setPosition(pos);
            if (len > 0) {
                c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, len);
            }
            editor_->setTextCursor(c);
            editor_->ensureCursorVisible();
        });

    auto dockFind = new QDockWidget(QStringLiteral("查找结果"), this);
    dockFind->setObjectName("dockFindResults");
    dockFind->setWidget(resultsTable);
    addDockWidget(Qt::BottomDockWidgetArea, dockFind);

    // 允许浮动/停靠
    dockClass->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    dockProp->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    dockFind->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
}


void MainWindow::buildStatusBar()
{
    statusBar()->addPermanentWidget(lblPos_);
    statusBar()->addPermanentWidget(lblSize_);
    onCursorPosChanged();
    const int bytes = editor_->toPlainText().toUtf8().size();
    lblSize_->setText(QStringLiteral("大小: %1 KB").arg(QString::number(bytes / 1024.0, 'f', 2)));
}

// ================== 文件相关 ==================
//void MainWindow::newFile()
//{
//    editor_->clear();
//    currentFilePath_.clear();
//    classCounts_.clear();
//    rebuildClassTree();
//    updateWindowTitle();
//    if (findResults_) findResults_->setRowCount(0); // 清空查找结果
//}

void MainWindow::newFile()
{
    // 清空状态
    editor_->clear();
    currentFilePath_.clear();
    classCounts_.clear();

    // 写入默认头段 + 空白数据段
    static const char* kGfcTemplate =
        "HEADER;\n"
        "FILE_DESCRIPTION(('GFC3X4'),'65001');\n"
        "FILE_NAME('');\n"          // 注意反斜杠要转义成 \\\\
        "FILE_SCHEMA(('GFC3X4'));\n"
        "ENDSEC;\n"
        "DATA;\n";                            // 空白数据段从这里开始

    editor_->setPlainText(QString::fromUtf8(kGfcTemplate));

    // 光标移动到 DATA; 后（空白数据段内）
    QTextCursor c = editor_->textCursor();
    c.movePosition(QTextCursor::End);
    editor_->setTextCursor(c);

    // 视图/标题等收尾
    rebuildClassTree();      // 新文件无实例，刷新左侧树
    enableGfcSyntaxColors();
    updateWindowTitle();
}

void MainWindow::openGfc() {
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 GFC 文件"), QString(), "GFC (*.gfc)");
    if (path.isEmpty()) return;

    if (loadGfcFromFile(path)) {  // 假设loadGfcFromFile函数已正确实现
        // 将文件添加到最近文件列表
        recentFiles_.removeAll(path);  // 移除已存在的文件路径
        recentFiles_.prepend(path);  // 将当前文件添加到列表最前面
        if (recentFiles_.size() > 5) {
            recentFiles_.removeLast();  // 保持列表大小为 5
        }

        // 更新菜单
        updateRecentFilesMenu();  // 刷新最近文件菜单
    }
}


void MainWindow::saveGfc()
{
    if (currentFilePath_.isEmpty()) {
        saveGfcAs();
        return;
    }
    if (saveGfcToFile(currentFilePath_)) {
        statusBar()->showMessage(QStringLiteral("已保存：%1").arg(currentFilePath_), 2000);
    }
}

void MainWindow::saveGfcAs()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("另存为 GFC 文件"), currentFilePath_.isEmpty() ? QString() : currentFilePath_, "GFC (*.gfc)");
    if (path.isEmpty()) return;
    if (saveGfcToFile(path)) {
        currentFilePath_ = path;
        updateWindowTitle();
        statusBar()->showMessage(QStringLiteral("已保存：%1").arg(currentFilePath_), 2000);
    }
}

void MainWindow::openSchemaExp()
{
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 Schema (.exp)"), QString(), "EXP (*.exp)");
    if (path.isEmpty()) return;

    QString err;
    if (!schema_.parseFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("解析失败"), err);
        return;
    }
    currentSchemaPath_ = path;
    children_ = schema_.buildChildrenMap();
    prepareSchemaIndex();
    directCountCamel_.clear();
    inclusiveCountCamel_.clear();
    instancesByCamel_.clear();
    statusBar()->showMessage(QStringLiteral("已加载 Schema：%1").arg(path), 3000);
}

//bool MainWindow::loadGfcFromFile(const QString& path)
//{
//    QFile f(path);
//    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
//        QMessageBox::warning(this, QStringLiteral("打开失败"),
//            QStringLiteral("无法打开文件：%1").arg(path));
//        return false;
//    }
//
//    QTextStream in(&f);
//    setUtf8(in);
//    const QString text = in.readAll();
//    editor_->setPlainText(text);
//
//    instanceRefs_.clear();
//    QVector<GfcInstanceRef> refs;
//    GfcParser::countClasses(text, &refs);
//
//    directCountCamel_.clear();
//    inclusiveCountCamel_.clear();
//    instancesByCamel_.clear();
//
//    if (schema_.classes().isEmpty()) {
//        statusBar()->showMessage(QStringLiteral("提示：未加载 Schema(.exp)，只能显示文本，视图区计数为0。"), 4000);
//        rebuildClassTree();
//        return true;
//    }
//    if (lowerToCamel_.isEmpty()) {
//        prepareSchemaIndex();
//    }
//
//    int unknown = 0;
//    for (const auto& r : refs) {
//        const QString camel = lowerToCamel_.value(r.cls.toLower());
//        if (camel.isEmpty()) { ++unknown; continue; }
//        directCountCamel_[camel] += 1;
//        instancesByCamel_[camel].push_back(r);
//    }
//
//    inclusiveCountCamel_ = directCountCamel_;
//    for (auto it = directCountCamel_.cbegin(); it != directCountCamel_.cend(); ++it) {
//        QString p = schema_.classes().value(it.key()).parent;
//        while (!p.isEmpty()) {
//            inclusiveCountCamel_[p] += it.value();
//            p = schema_.classes().value(p).parent;
//        }
//    }
//
//    currentFilePath_ = path;
//    updateWindowTitle();
//    rebuildClassTree();
//
//#ifdef QT_DEBUG
//    qDebug() << "==== loadGfcFromFile ===="
//        << "parsed instances:" << refs.size()
//        << "unknown classes:" << unknown;
//#endif
//
//    statusBar()->showMessage(
//        QStringLiteral("已加载 GFC：解析到 %1 个实例，映射到 %2 个类，忽略未知类 %3。")
//        .arg(refs.size()).arg(directCountCamel_.size()).arg(unknown),
//        4000);
//
//    return true;
//}


bool MainWindow::loadGfcFromFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), QStringLiteral("无法打开文件：%1").arg(path));
        return false;
    }
    QTextStream in(&f);
    setUtf8(in);
    const QString text = in.readAll();
    suppressReparse_ = true;
    editor_->setPlainText(text);
    suppressReparse_ = false;

    // ★ 新增：启用 .gfc 语法着色
    enableGfcSyntaxColors();

    // （你原有的：统计/刷新等）
    classCounts_ = GfcParser::countClasses(text, nullptr);
    RecomputeStats st = recomputeFromText(text);
    currentFilePath_ = path;
    updateWindowTitle();
    rebuildClassTree();
#ifdef QT_DEBUG
    qDebug() << "[loadGfcFromFile] instances=" << st.instances
        << "unknown=" << st.unknown << "mappedCls=" << st.mappedCls;
#endif
    statusBar()->showMessage(
        QStringLiteral("已加载 GFC：解析到 %1 个实例，映射到 %2 个类，忽略未知类 %3。")
        .arg(st.instances).arg(st.mappedCls).arg(st.unknown),
        3500);
    return true;
}


bool MainWindow::saveGfcToFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入文件：%1").arg(path));
        return false;
    }
    QTextStream out(&f);
    setUtf8(out);
    out << editor_->toPlainText();
    return true;
}

void MainWindow::updateWindowTitle()
{
    QString title = QStringLiteral("GFC Editor");
    if (!currentFilePath_.isEmpty()) {
        title += QStringLiteral(" - [%1]").arg(currentFilePath_);
    }
    if (!currentSchemaPath_.isEmpty()) {
        title += QStringLiteral(" {Schema: %1}").arg(currentSchemaPath_);
    }
    setWindowTitle(title);
}

// ================== 视图区（类继承树） ==================
int MainWindow::computeInclusiveCount(const QString& cls) const
{
    int sum = classCounts_.value(cls, 0);
    auto it = children_.find(cls);
    if (it != children_.end()) {
        for (const auto& ch : it.value()) {
            sum += computeInclusiveCount(ch);
        }
    }
    return sum;
}

void MainWindow::prepareSchemaIndex()
{
    lowerToCamel_.clear();
    for (auto it = schema_.classes().cbegin(); it != schema_.classes().cend(); ++it) {
        const QString camel = it.key();
        lowerToCamel_.insert(camel.toLower(), camel);
    }
}

void MainWindow::rebuildClassTree()
{
    classModel_->removeRows(0, classModel_->rowCount());

    if (schema_.classes().isEmpty()) {
        auto* tip = new QStandardItem(QStringLiteral("未加载 Schema(.exp)。在“文件”菜单打开 .exp"));
        tip->setEditable(false);
        classModel_->appendRow(tip);
        return;
    }

    bool hasAnyInstance = false;
    for (auto it = inclusiveCountCamel_.cbegin(); it != inclusiveCountCamel_.cend(); ++it) {
        if (it.value() > 0) { hasAnyInstance = true; break; }
    }

    QSet<QString> roots;
    for (auto it = schema_.classes().cbegin(); it != schema_.classes().cend(); ++it) {
        if (it->parent.isEmpty()) roots.insert(it.key());
    }

    std::function<QStandardItem* (const QString&)> makeNode =
        [&](const QString& camel) -> QStandardItem*
        {
            const int direct = directCountCamel_.value(camel, 0);
            const int incl = inclusiveCountCamel_.value(camel, 0);

            if (hasAnyInstance && incl == 0) return nullptr;

            auto* item = new QStandardItem(
                QStringLiteral("%1 (%2/%3)").arg(camel).arg(direct).arg(incl));
            item->setEditable(false);
            item->setData(camel, RoleClassName);
            item->setData(NodeClass, RoleNodeType);

            for (const auto& ch : children_.value(camel)) {
                if (auto* childItem = makeNode(ch)) item->appendRow(childItem);
            }

            const auto insts = instancesByCamel_.value(camel);
            for (const auto& ref : insts) {
                auto* inst = new QStandardItem(
                    QString("#%1 %2").arg(ref.index).arg(ref.cls));
                inst->setEditable(false);
                inst->setData(NodeInstance, RoleNodeType);
                inst->setData(ref.pos, RoleDocPos);
                item->appendRow(inst);
            }
            return item;
        };

    int added = 0;
    for (const auto& r : roots) {
        if (auto* rootItem = makeNode(r)) {
            classModel_->appendRow(rootItem);
            ++added;
        }
    }

    if (added == 0) {
        auto* tip = new QStandardItem(
            hasAnyInstance
            ? QStringLiteral("所有 (0/0) 类已隐藏，无可显示节点。")
            : QStringLiteral("当前无实例，未启用隐藏规则（请加载 .gfc）。"));
        tip->setEditable(false);
        classModel_->appendRow(tip);
    }
}

void MainWindow::updateParentInstances(const QString& cls)
{
    const auto& ci = schema_.classes().value(cls);
    if (!ci.parent.isEmpty()) {
        classCounts_[ci.parent] += 1;
        instancesByClass_[ci.parent].push_back({ -1, ci.parent, -1 });
        updateParentInstances(ci.parent);
    }
}

// ================== 高亮辅助 ==================
void MainWindow::clearHighlights()
{
    editor_->setExtraSelections({});
}

void MainWindow::highlightOccurrences(const QString& token)
{
    clearHighlights();
    if (token.trimmed().isEmpty()) return;

    QList<QTextEdit::ExtraSelection> sels;

    QString text = editor_->toPlainText();
    QRegularExpression re(QString(R"((\b%1\b\s*\())").arg(QRegularExpression::escape(token)));

    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        auto m = it.next();
        int pos = m.capturedStart(1);

        QTextCursor c(editor_->document());
        c.setPosition(pos);
        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, token.size());

        QTextEdit::ExtraSelection sel;
        sel.cursor = c;
        QTextCharFormat fmt;
        fmt.setBackground(QColor(255, 255, 0, 120));
        sel.format = fmt;
        sels << sel;
    }

    editor_->setExtraSelections(sels);
}

// ================== 属性区展示 ==================
void MainWindow::showClassProperties(const QString& cls)
{
    propTable_->clearContents();
    propTable_->setRowCount(0);
    propTable_->setHorizontalHeaderLabels({ QStringLiteral("属性定义"), QStringLiteral("备注") });

    if (!schema_.classes().contains(cls)) return;

    const auto& info = schema_.classes()[cls];
    propTable_->setRowCount(info.attributes.size());
    for (int i = 0; i < info.attributes.size(); ++i) {
        auto* a0 = new QTableWidgetItem(info.attributes[i]);
        auto* a1 = new QTableWidgetItem(QString());
        a0->setFlags(a0->flags() & ~Qt::ItemIsEditable);
        propTable_->setItem(i, 0, a0);
        propTable_->setItem(i, 1, a1);
    }
}

// ================== 交互 ==================
void MainWindow::onCursorPosChanged()
{
    auto c = editor_->textCursor();
    int line = c.blockNumber() + 1;
    int col = c.positionInBlock() + 1;
    lblPos_->setText(QStringLiteral("行: %1  列: %2").arg(line).arg(col));

    // 当前行是否是实例创建开头？如果是，仅展示属性与高亮，但不移动光标
    const QString blockText = c.block().text();
    QRegularExpression re(R"(^\s*#\s*([0-9]+)\s*=\s*([A-Za-z0-9_]+)\s*\()");
    auto m = re.match(blockText);
    if (m.hasMatch()) {
        const int startInBlock = m.capturedStart(0);
        const int absStart = c.block().position() + startInBlock;
        showInstanceByPos(absStart, /*moveCaret=*/false);  // ✅ 文本点击：不跳光标，便于编辑
    }
}


void MainWindow::highlightRange(int start, int end)
{
    QList<QTextEdit::ExtraSelection> sels;
    QTextCursor cur(editor_->document());
    cur.setPosition(start);
    cur.setPosition(end, QTextCursor::KeepAnchor);

    QTextEdit::ExtraSelection sel;
    sel.cursor = cur;
    QTextCharFormat fmt;
    fmt.setBackground(QColor(255, 255, 0, 120)); // 半透明黄
    sel.format = fmt;
    sels << sel;

    currentSelections_ = sels;
    editor_->setExtraSelections(currentSelections_);
}

QString MainWindow::camelFromUpper(const QString& upper) const
{
    // 使用 lowerToCamel_ 建立的大小写无关映射（.exp 加载后 prepareSchemaIndex() 填充）
    return lowerToCamel_.value(upper.toLower());
}

QStringList MainWindow::schemaAttrNames(const QString& camel) const
{
    QStringList names;
    if (!schema_.classes().contains(camel)) return names;
    const auto& attrs = schema_.classes()[camel].attributes; // "name : TYPE"
    QRegularExpression nameRe(R"(^\s*([A-Za-z_][A-Za-z0-9_]*)\s*:)");
    for (const QString& line : attrs) {
        auto m = nameRe.match(line);
        names << (m.hasMatch() ? m.captured(1) : line.trimmed());
    }
    return names;
}

void MainWindow::showParsedInstanceProperties(const ParsedInstance& pi, const QString& camel)
{
    propTable_->clearContents();
    propTable_->setRowCount(0);
    propTable_->setHorizontalHeaderLabels({ QStringLiteral("属性名"), QStringLiteral("值") });

    const QStringList names = schemaAttrNames(camel);     // 按 .exp 顺序
    const int rows = qMax(names.size(), pi.params.size());
    propTable_->setRowCount(rows);

    for (int i = 0; i < rows; ++i) {
        const QString n = (i < names.size()) ? names[i] : QStringLiteral("<extra #%1>").arg(i + 1);
        const QString v = (i < pi.params.size()) ? pi.params[i] : QStringLiteral("<missing>");

        auto* c0 = new QTableWidgetItem(n);
        c0->setFlags(c0->flags() & ~Qt::ItemIsEditable);
        auto* c1 = new QTableWidgetItem(v);
        c1->setFlags(c1->flags() & ~Qt::ItemIsEditable);

        propTable_->setItem(i, 0, c0);
        propTable_->setItem(i, 1, c1);
    }
}

void MainWindow::showInstanceByPos(int pos, bool moveCaret)
{
    ParsedInstance pi;
    const QString text = editor_->toPlainText();
    if (!GfcParser::parseInstanceAt(text, pos, &pi)) {
        return; // 不是实例或解析失败：不动属性表/高亮
    }

    const QString camel = camelFromUpper(pi.classUpper);
    if (!camel.isEmpty()) {
        showParsedInstanceProperties(pi, camel);
    }
    else {
        // 未映射到 schema 的类：也按顺序展示参数
        showParsedInstanceProperties(pi, QString());
    }

    // 仅做“额外高亮”，不影响编辑光标
    highlightRange(pi.start, pi.end);

    // 只有在明确要求时才移动/居中光标（例如：从类树点击实例时）
    if (moveCaret) {
        QTextCursor tc(editor_->document());
        tc.setPosition(pi.start);
        editor_->setTextCursor(tc);
        editor_->centerCursor();
    }
}



void MainWindow::onClassTreeClicked(const QModelIndex& idx)
{
    if (!idx.isValid()) return;
    const int nodeType = idx.data(RoleNodeType).toInt();

    if (nodeType == NodeInstance) {
        const int pos = idx.data(RoleDocPos).toInt();
        showInstanceByPos(pos, /*moveCaret=*/true);   // ✅ 类视图点击：允许跳转
        return;
    }

    // 类节点：显示 Schema 属性 + 高亮类名出现处
    const QString cls = idx.data(RoleClassName).toString();
    if (cls.isEmpty()) return;
    showClassProperties(cls);
    highlightOccurrences(cls);
}



// ================== 查找/替换/高亮辅助（保持你原有逻辑不变；仅在“查找”后填充列表） ==================


void MainWindow::doFind() {
    // （下面这段是你原有的对话框与控件，保持不变）
    QDialog* findDialog = new QDialog(this);
    findDialog->setWindowTitle(QStringLiteral("查找/替换"));

    QLineEdit* findLineEdit = new QLineEdit(findDialog);
    findLineEdit->setText(lastFindText_);
    QLineEdit* replaceLineEdit = new QLineEdit(findDialog);
    replaceLineEdit->setText(lastReplaceText_);

    QCheckBox* caseCheckBox = new QCheckBox(QStringLiteral("区分大小写"), findDialog);
    caseCheckBox->setChecked(true);
    QCheckBox* wordCheckBox = new QCheckBox(QStringLiteral("全字匹配"), findDialog);
    wordCheckBox->setChecked(false);

    QPushButton* findButton = new QPushButton(QStringLiteral("查找"), findDialog);
    QPushButton* findNextButton = new QPushButton(QStringLiteral("查找下一个"), findDialog);
    QPushButton* replaceButton = new QPushButton(QStringLiteral("替换"), findDialog);
    QPushButton* replaceAllButton = new QPushButton(QStringLiteral("替换所有"), findDialog);

    QVBoxLayout* layout = new QVBoxLayout(findDialog);
    layout->addWidget(new QLabel(QStringLiteral("查找文本:")));
    layout->addWidget(findLineEdit);
    layout->addWidget(new QLabel(QStringLiteral("替换为:")));
    layout->addWidget(replaceLineEdit);
    layout->addWidget(caseCheckBox);
    layout->addWidget(wordCheckBox);
    layout->addWidget(findButton);
    layout->addWidget(findNextButton);
    layout->addWidget(replaceButton);
    layout->addWidget(replaceAllButton);

    // —— 查找：定位 +（新增）把所有匹配填入底部列表
    connect(findButton, &QPushButton::clicked, this,
        [this, findLineEdit, caseCheckBox, wordCheckBox] {
            const QString searchText = findLineEdit->text();
            if (searchText.isEmpty()) return;

            lastFindText_ = searchText;

            // 使用你现有的查找选项
            QTextDocument::FindFlags flags;
            if (caseCheckBox->isChecked()) flags |= QTextDocument::FindCaseSensitively;
            if (wordCheckBox->isChecked()) flags |= QTextDocument::FindWholeWords;

            // 先做一次“当前光标处向后查找；若失败则从头查找”的定位（保持原逻辑）
            QTextCursor cursor = editor_->textCursor();
            cursor = editor_->document()->find(searchText, cursor, flags);
            if (cursor.isNull()) {
                QTextCursor start(editor_->document());
                cursor = editor_->document()->find(searchText, start, flags);
            }
            if (!cursor.isNull()) {
                editor_->setTextCursor(cursor);
                editor_->ensureCursorVisible();
            }
            else {
                QMessageBox::information(this, QStringLiteral("查找"), QStringLiteral("找不到：%1").arg(searchText));
            }

            // === 新增：把全文匹配结果填入底部“查找结果”表 ===
            auto* table = this->findChild<QTableWidget*>("findResultsTable");
            if (!table) return;
            table->setRowCount(0);

            QTextCursor c = editor_->document()->find(searchText, QTextCursor(editor_->document()), flags);
            int row = 0;
            while (!c.isNull()) {
                const int selStart = c.selectionStart();
                const int selLen = c.selectedText().length();

                QTextBlock block = editor_->document()->findBlock(selStart);
                const int line = block.blockNumber() + 1;
                const int col = selStart - block.position() + 1;
                QString context = block.text();
                if (context.size() > 200) context = context.left(200) + QStringLiteral("...");

                table->insertRow(row);
                auto* itLine = new QTableWidgetItem(QString::number(line));
                auto* itCol = new QTableWidgetItem(QString::number(col));
                auto* itCtx = new QTableWidgetItem(context);

                // 在第一列的 UserRole 保存定位信息（起始位置/长度）
                itLine->setData(Qt::UserRole, selStart);
                itLine->setData(Qt::UserRole + 1, selLen);

                itLine->setFlags(itLine->flags() & ~Qt::ItemIsEditable);
                itCol->setFlags(itCol->flags() & ~Qt::ItemIsEditable);
                itCtx->setFlags(itCtx->flags() & ~Qt::ItemIsEditable);

                table->setItem(row, 0, itLine);
                table->setItem(row, 1, itCol);
                table->setItem(row, 2, itCtx);

                // 继续从当前匹配末尾位置向后查找
                QTextCursor next(editor_->document());
                next.setPosition(c.selectionEnd());
                c = editor_->document()->find(searchText, next, flags);
                ++row;
            }

            if (auto dock = this->findChild<QDockWidget*>("dockFindResults")) {
                dock->setVisible(true);
                if (row > 0) dock->raise();
            }
            statusBar()->showMessage(QStringLiteral("共找到 %1 处。").arg(row), 3000);
        });

    // —— 以下三个按钮逻辑保持你原有实现（不变） ——
    connect(findNextButton, &QPushButton::clicked, this,
        [this, findLineEdit, caseCheckBox, wordCheckBox] {
            const QString searchText = findLineEdit->text();
            if (searchText.isEmpty()) return;
            lastFindText_ = searchText;

            QTextDocument::FindFlags flags;
            if (caseCheckBox->isChecked()) flags |= QTextDocument::FindCaseSensitively;
            if (wordCheckBox->isChecked()) flags |= QTextDocument::FindWholeWords;

            QTextCursor cursor = editor_->textCursor();
            cursor = editor_->document()->find(searchText, cursor, flags);
            if (!cursor.isNull()) {
                editor_->setTextCursor(cursor);
                editor_->ensureCursorVisible();
            }
            else {
                QMessageBox::information(this, QStringLiteral("查找"), QStringLiteral("已到文末。"));
            }
        });

    connect(replaceButton, &QPushButton::clicked, this,
        [this, findLineEdit, replaceLineEdit, caseCheckBox, wordCheckBox] {
            QString searchText = findLineEdit->text();
            QString replaceText = replaceLineEdit->text();
            bool caseSensitive = caseCheckBox->isChecked();
            bool wholeWord = wordCheckBox->isChecked();

            if (!searchText.isEmpty() && !replaceText.isEmpty()) {
                lastFindText_ = searchText;
                lastReplaceText_ = replaceText;

                QTextDocument::FindFlags flags;
                if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
                if (wholeWord)     flags |= QTextDocument::FindWholeWords;

                QTextCursor cursor = editor_->textCursor();
                if (cursor.selectedText() == searchText.toUpper()) {
                    cursor.insertText(replaceText);
                    editor_->setTextCursor(cursor);
                    editor_->ensureCursorVisible();
                }
                else {
                    cursor = editor_->document()->find(searchText, cursor, flags);
                    if (!cursor.isNull()) {
                        cursor.insertText(replaceText);
                        editor_->setTextCursor(cursor);
                        editor_->ensureCursorVisible();
                    }
                }
                cursor = editor_->document()->find(searchText, cursor, flags);
                if (!cursor.isNull()) {
                    editor_->setTextCursor(cursor);
                    editor_->ensureCursorVisible();
                }
            }
        });

    connect(replaceAllButton, &QPushButton::clicked, this,
        [this, findLineEdit, replaceLineEdit, caseCheckBox, wordCheckBox] {
            QString searchText = findLineEdit->text();
            QString replaceText = replaceLineEdit->text();
            bool caseSensitive = caseCheckBox->isChecked();
            bool wholeWord = wordCheckBox->isChecked();

            if (!searchText.isEmpty() && !replaceText.isEmpty()) {
                lastFindText_ = searchText;
                lastReplaceText_ = replaceText;

                QTextDocument::FindFlags flags;
                if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
                if (wholeWord)     flags |= QTextDocument::FindWholeWords;

                QTextCursor cursor = editor_->textCursor();
                cursor = editor_->document()->find(searchText, cursor, flags);
                while (!cursor.isNull()) {
                    cursor.insertText(replaceText);
                    editor_->setTextCursor(cursor);
                    editor_->ensureCursorVisible();
                    cursor = editor_->document()->find(searchText, cursor, flags);
                }
            }
        });

    findDialog->exec();
}

void MainWindow::doFindNext()
{
    if (lastFindText_.isEmpty()) { doFind(); return; }
    QTextDocument::FindFlags flags;
    auto c = editor_->textCursor();
    c = editor_->document()->find(lastFindText_, c, flags);
    if (!c.isNull()) {
        editor_->setTextCursor(c);
        editor_->ensureCursorVisible();
    }
    else {
        QMessageBox::information(this, QStringLiteral("查找"), QStringLiteral("已到文末。"));
    }
}

void MainWindow::doReplace()
{
    if (lastFindText_.isEmpty()) {
        doFind();
        if (lastFindText_.isEmpty()) return;
    }
    bool ok = false;
    QString repl = QInputDialog::getText(this, QStringLiteral("替换"),
        QStringLiteral("将当前选中的/下一处匹配替换为："),
        QLineEdit::Normal, QString(), &ok);
    if (!ok) return;

    auto c = editor_->textCursor();
    if (c.hasSelection() && c.selectedText() == lastFindText_) {
        c.insertText(repl);
    }
    else {
        QTextDocument::FindFlags flags;
        c = editor_->document()->find(lastFindText_, c, flags);
        if (!c.isNull()) {
            editor_->setTextCursor(c);
            editor_->ensureCursorVisible();
            c.insertText(repl);
        }
        else {
            QMessageBox::information(this, QStringLiteral("替换"), QStringLiteral("没有更多匹配。"));
        }
    }
}

// 为 .gfc 文档安装语法着色（只安装一次）
void MainWindow::enableGfcSyntaxColors()
{
    if (!editor_ || !editor_->document()) return;
    if (editor_->document()->property("gfcSyntaxOn").toBool()) return;

    // 局部定义一个简易高亮器，父对象设为 document，随文档一起析构
    class GfcHighlighter final : public QSyntaxHighlighter {
    public:
        using QSyntaxHighlighter::QSyntaxHighlighter;

        void highlightBlock(const QString& text) override {
            // --- 颜色风格 ---
            static const QTextCharFormat fmtStr = [] { QTextCharFormat f; f.setForeground(QColor(0, 128, 0));    return f; }(); // 字符串
            static const QTextCharFormat fmtNum = [] { QTextCharFormat f; f.setForeground(QColor(136, 0, 136));  return f; }(); // 数字
            static const QTextCharFormat fmtId = [] { QTextCharFormat f; f.setForeground(QColor(200, 120, 0));  return f; }(); // #123=
            static const QTextCharFormat fmtClass = [] { QTextCharFormat f; f.setForeground(QColor(25, 118, 210)); f.setFontWeight(QFont::Bold); return f; }(); // 类名
            static const QTextCharFormat fmtCmt = [] { QTextCharFormat f; f.setForeground(QColor(120, 120, 120));  return f; }(); // 注释
            // ★ 新增：关键字 HEADER / DATA（红色加粗）
            static const QTextCharFormat fmtKw = [] { QTextCharFormat f; f.setForeground(QColor(220, 0, 0)); f.setFontWeight(QFont::Bold); return f; }();

            // --- 正则（按 token 类型） ---
            // 字符串（支持转义字符），不跨行
            static const QRegularExpression reDQ(R"("([^"\\]|\\.)*")");
            static const QRegularExpression reSQ(R"('([^'\\]|\\.)*')");
            // 数字：整数/小数/科学计数
            static const QRegularExpression reNum(R"(\b[+-]?(?:\d+\.\d*|\d*\.\d+|\d+)(?:[eE][+-]?\d+)?\b)");
            // #ID= 里的 #和数字（只匹配到 '=' 之前）
            static const QRegularExpression reId(R"(#\s*\d+(?=\s*=))");
            // 类名： =ClassName(   ——着色捕获组1
            static const QRegularExpression reClass(R"(=\s*([A-Za-z_][A-Za-z0-9_]*)\s*\()");
            // ★ 新增：关键字（仅匹配完整单词）
            static const QRegularExpression reKw(R"(\b(?:HEADER|DATA)\b)");
            // 行注释
            static const QRegularExpression reLineCmt(R"(//.*$)", QRegularExpression::MultilineOption);

            // 先清除 block 状态（用于多行注释）
            setCurrentBlockState(0);

            // 1) 字符串
            auto it = reDQ.globalMatch(text);
            while (it.hasNext()) { const auto m = it.next(); setFormat(m.capturedStart(), m.capturedLength(), fmtStr); }
            it = reSQ.globalMatch(text);
            while (it.hasNext()) { const auto m = it.next(); setFormat(m.capturedStart(), m.capturedLength(), fmtStr); }

            // 2) 数字
            it = reNum.globalMatch(text);
            while (it.hasNext()) { const auto m = it.next(); setFormat(m.capturedStart(), m.capturedLength(), fmtNum); }

            // 3) #ID=
            it = reId.globalMatch(text);
            while (it.hasNext()) { const auto m = it.next(); setFormat(m.capturedStart(), m.capturedLength(), fmtId); }

            // 4) 类名（只给 group(1) 上色）
            it = reClass.globalMatch(text);
            while (it.hasNext()) {
                const auto m = it.next();
                const int s1 = m.capturedStart(1);
                const int l1 = m.capturedLength(1);
                if (s1 >= 0 && l1 > 0) setFormat(s1, l1, fmtClass);
            }

            // ★ 5) 关键字 HEADER / DATA（注意放在注释着色之前，后者会覆盖前者）
            it = reKw.globalMatch(text);
            while (it.hasNext()) { const auto m = it.next(); setFormat(m.capturedStart(), m.capturedLength(), fmtKw); }

            // 6) 行注释
            it = reLineCmt.globalMatch(text);
            while (it.hasNext()) { const auto m = it.next(); setFormat(m.capturedStart(), m.capturedLength(), fmtCmt); }

            // 7) 多行注释 /* ... */
            const QString startTok = "/*";
            const QString endTok = "*/";
            int start = 0;

            if (previousBlockState() == 1) {
                // 上一行在注释中，先找结束
                int end = text.indexOf(endTok, 0);
                if (end == -1) {
                    setFormat(0, text.length(), fmtCmt);
                    setCurrentBlockState(1);
                    return;
                }
                else {
                    setFormat(0, end + 2, fmtCmt);
                    start = end + 2;
                }
            }

            while (true) {
                int s = text.indexOf(startTok, start);
                if (s == -1) break;
                int e = text.indexOf(endTok, s + 2);
                if (e == -1) {
                    setFormat(s, text.length() - s, fmtCmt);
                    setCurrentBlockState(1);
                    break;
                }
                else {
                    setFormat(s, (e + 2) - s, fmtCmt);
                    start = e + 2;
                }
            }
        }
    };

    new GfcHighlighter(editor_->document());
    editor_->document()->setProperty("gfcSyntaxOn", true);
}


// ================== 视图可见性 ==================
void MainWindow::toggleClassDock(bool checked)
{
    if (auto dock = findChild<QDockWidget*>("dockClassView")) dock->setVisible(checked);
}
void MainWindow::togglePropDock(bool checked)
{
    if (auto dock = findChild<QDockWidget*>("dockProp")) dock->setVisible(checked);
}
void MainWindow::toggleToolbar(bool checked)
{
    if (auto tb = findChild<QToolBar*>()) tb->setVisible(checked);
}
void MainWindow::toggleStatusbar(bool checked)
{
    statusBar()->setVisible(checked);
}

// ================== 导航相关（原有） ==================
void MainWindow::updateNavActions()
{
    if (actBack_)    actBack_->setEnabled(!navBackStack_.isEmpty());
    if (actForward_) actForward_->setEnabled(!navFwdStack_.isEmpty());
}

int MainWindow::findInstancePosition(int id) const
{
    const QString text = editor_->toPlainText();
    const QRegularExpression re(QString("^#%1\\b").arg(id), QRegularExpression::MultilineOption);
    const auto m = re.match(text);
    if (m.hasMatch()) return m.capturedStart(0);
    return -1;
}

void MainWindow::navigateTo(int pos, bool fromBackOrForward)
{
    if (pos < 0) return;
    QTextCursor c = editor_->textCursor();
    if (!fromBackOrForward) {
        navBackStack_.append(c.position());
        navFwdStack_.clear();
    }
    c.setPosition(pos);
    c.movePosition(QTextCursor::StartOfBlock);
    editor_->setTextCursor(c);
    editor_->centerCursor();
    updateNavActions();
}

void MainWindow::goBack()
{
    if (navBackStack_.isEmpty()) return;
    QTextCursor c = editor_->textCursor();
    navFwdStack_.append(c.position());
    int pos = navBackStack_.takeLast();
    QTextCursor t(editor_->document());
    t.setPosition(pos);
    editor_->setTextCursor(t);
    editor_->centerCursor();
    updateNavActions();
}

void MainWindow::goForward()
{
    if (navFwdStack_.isEmpty()) return;
    QTextCursor c = editor_->textCursor();
    navBackStack_.append(c.position());
    int pos = navFwdStack_.takeLast();
    QTextCursor t(editor_->document());
    t.setPosition(pos);
    editor_->setTextCursor(t);
    editor_->centerCursor();
    updateNavActions();
}

void MainWindow::locateAtCursor()
{
    QTextCursor c = editor_->textCursor();
    const QString line = c.block().text();
    const int column = c.position() - c.block().position();

    int left = column - 1;
    while (left >= 0 && line[left].isDigit()) --left;

    if (left >= 0 && line[left] == '#') {
        // ok
    }
    else if (left + 1 < line.size() && line[left + 1] == '#') {
        left = left + 1;
    }
    else {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请把光标放在引用实例的序号上，例如 #13"));
        return;
    }

    int i = left + 1;
    QString digits;
    while (i < line.size() && line[i].isDigit()) { digits += line[i]; ++i; }
    if (digits.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("未检测到实例序号"));
        return;
    }

    const int id = digits.toInt();
    const int pos = findInstancePosition(id);
    if (pos < 0) {
        QMessageBox::warning(this, QStringLiteral("定位失败"),
            QStringLiteral("未找到实例 #%1").arg(id));
        return;
    }

    navBackStack_.append(c.position());
    navFwdStack_.clear();
    QTextCursor t(editor_->document());
    t.setPosition(pos);
    editor_->setTextCursor(t);
    editor_->centerCursor();
    updateNavActions();
}

// ================== （新增）把全文匹配结果填充至底部列表 ==================
void MainWindow::runFindAll(const QString& pattern, QTextDocument::FindFlags flags)
{
    if (!findResults_) return;

    findResults_->setRowCount(0);
    if (pattern.trimmed().isEmpty()) return;

    QTextCursor start(editor_->document());
    QTextCursor c = editor_->document()->find(pattern, start, flags);

    int row = 0;
    while (!c.isNull()) {
        const int selStart = c.selectionStart();
        const int selLen = c.selectedText().length();

        QTextBlock block = editor_->document()->findBlock(selStart);
        const int line = block.blockNumber() + 1;
        const int col = selStart - block.position() + 1;
        QString context = block.text();
        if (context.size() > 200) context = context.left(200) + QStringLiteral("...");

        findResults_->insertRow(row);

        auto* itLine = new QTableWidgetItem(QString::number(line));
        auto* itCol = new QTableWidgetItem(QString::number(col));
        auto* itCtx = new QTableWidgetItem(context);

        // 把定位信息放在第一列（UserRole）
        itLine->setData(Qt::UserRole, selStart);
        itLine->setData(Qt::UserRole + 1, selLen);

        itLine->setFlags(itLine->flags() & ~Qt::ItemIsEditable);
        itCol->setFlags(itCol->flags() & ~Qt::ItemIsEditable);
        itCtx->setFlags(itCtx->flags() & ~Qt::ItemIsEditable);

        findResults_->setItem(row, 0, itLine);
        findResults_->setItem(row, 1, itCol);
        findResults_->setItem(row, 2, itCtx);

        // 继续查找
        QTextCursor next(editor_->document());
        next.setPosition(c.selectionEnd());
        c = editor_->document()->find(pattern, next, flags);
        ++row;
    }

    if (auto dock = findChild<QDockWidget*>("dockFindResults")) {
        dock->setVisible(true);
        if (row > 0) dock->raise();
    }
    statusBar()->showMessage(QStringLiteral("共找到 %1 处。").arg(row), 3000);
}

// ================== （新增）双击结果行进行定位 ==================
void MainWindow::onFindResultActivated(int row, int col)
{
    Q_UNUSED(col);
    if (!findResults_) return;
    auto* it = findResults_->item(row, 0);
    if (!it) return;

    const int pos = it->data(Qt::UserRole).toInt();
    const int len = it->data(Qt::UserRole + 1).toInt();

    QTextCursor c(editor_->document());
    c.setPosition(pos);
    if (len > 0) {
        c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, len);
    }
    editor_->setTextCursor(c);
    editor_->ensureCursorVisible();
}

void MainWindow::updateRecentFilesMenu() {
    // 确保直接使用 mRecentFilesMenu_ 操作菜单
    if (!mRecentFilesMenu_) {
        qWarning() << "最近文件菜单未正确初始化";
        return;
    }

    // 清空菜单项
    mRecentFilesMenu_->clear();

    // 输出最近打开文件的数量
    //qDebug() << "最近打开的文件数量:" << recentFiles_.size();

    // 若没有文件，则禁用菜单
    if (recentFiles_.isEmpty()) {
        mRecentFilesMenu_->setEnabled(false);  // 没有最近文件时禁用菜单
        return;  // 如果没有文件，直接退出
    }

    // 启用菜单
    mRecentFilesMenu_->setEnabled(true);

    // 为每个最近打开的文件添加菜单项
    for (int i = 0; i < recentFiles_.size(); ++i) {
        const QString& filePath = recentFiles_[i];
        QAction* act = mRecentFilesMenu_->addAction(filePath);  // 添加菜单项
        connect(act, &QAction::triggered, this, [this, filePath] { openRecentFile(filePath); });  // 连接点击信号
    }
}



void MainWindow::openRecentFile(const QString& filePath) {
    if (loadGfcFromFile(filePath)) {  // 假设loadGfcFromFile函数已正确实现
        // 加载成功后，更新菜单（避免重复添加）
        recentFiles_.removeAll(filePath);
        recentFiles_.prepend(filePath);
        if (recentFiles_.size() > 5) {
            recentFiles_.removeLast();
        }
        updateRecentFilesMenu();  // 刷新最近文件菜单
    }
}


// ================== Ctrl+点击 #id 跳转 ==================

bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == editor_->viewport()) {

        // 鼠标移动：按住 Ctrl 且位于 #id 上时显示手形光标
        if (ev->type() == QEvent::MouseMove) {
            auto* me = static_cast<QMouseEvent*>(ev);
            Qt::CursorShape shape = Qt::IBeamCursor;
            if (QApplication::keyboardModifiers() & Qt::ControlModifier) {
                // 粗判：当前位置是否落在 #数字 上
                QTextCursor c = editor_->cursorForPosition(me->pos());
                int dummy = -1;
                if (extractHashIdAtCursor(c, &dummy)) {
                    shape = Qt::PointingHandCursor;
                }
            }
            editor_->viewport()->setCursor(shape);
        }

        // Ctrl + 左键：跳转
        if (ev->type() == QEvent::MouseButtonRelease) {
            auto* me = static_cast<QMouseEvent*>(ev);
            if ((me->button() == Qt::LeftButton) &&
                (QApplication::keyboardModifiers() & Qt::ControlModifier)) {
                if (ctrlClickJumpToInstance(me->pos())) {
                    return true; // 事件已处理
                }
            }
        }
        // 普通左键：记录“点击前”的光标位置，作为后退/前进历史
        if (ev->type() == QEvent::MouseButtonPress || ev->type() == QEvent::MouseButtonDblClick) {
            auto* me = static_cast<QMouseEvent*>(ev);

            // 仅记录左键，且不按 Ctrl（Ctrl+左键的跳转在 ctrlClickJumpToInstance() 里已经做了入栈）
            if (me->button() == Qt::LeftButton &&
                !(QApplication::keyboardModifiers() & Qt::ControlModifier)) {

                const int beforePos = editor_->textCursor().position();

                // 去重：与后退栈顶相同则不重复压栈
                if (navBackStack_.isEmpty() || navBackStack_.last() != beforePos) {
                    navBackStack_.append(beforePos);
                    navFwdStack_.clear();
                    updateNavActions();
                }
                // 注意：这里不改变光标，由 Qt 自己处理鼠标点击后的移动
            }
        }

    }
    return QMainWindow::eventFilter(obj, ev);
}

bool MainWindow::ctrlClickJumpToInstance(const QPoint& viewPos)
{
    // 1) 把点击位置换算成文本光标
    QTextCursor cur = editor_->cursorForPosition(viewPos);

    // 2) 提取 #数字
    int id = -1;
    if (!extractHashIdAtCursor(cur, &id) || id < 0) {
        return false;
    }

    // 3) 找到对应定义行 "#id=CLASS(...)" 的起始 offset（行首的 '#' 位置）
    const int defPos = findInstancePosition(id);   // 你项目里已实现的方法
    if (defPos < 0) {
        QMessageBox::warning(this, QStringLiteral("定位失败"),
            QStringLiteral("未找到实例 #%1").arg(id));
        return true; // 事件已处理
    }

    // 4) 入栈（支持后退/前进）
    QTextCursor before = editor_->textCursor();
    navBackStack_.append(before.position());
    navFwdStack_.clear();
    updateNavActions();

    // 5) 选中并高亮 "#id=" ，并居中显示
    highlightIdTokenAt(defPos, id);
    editor_->centerCursor();
    return true;
}

bool MainWindow::extractHashIdAtCursor(const QTextCursor& cur, int* outId) const
{
    // 仅在当前行内做词法扩展，允许在括号中，如 (...,#5,...) 或 (#12)
    const QTextBlock blk = cur.block();
    const QString line = blk.text();
    const int posInBlock = cur.position() - blk.position();

    if (line.isEmpty()) return false;

    // 先向左找到 '#'
    int left = posInBlock;
    if (left >= line.size()) left = line.size() - 1;

    // 若正好点在字符右侧，优先看右侧
    if (left + 1 < line.size() && line[left + 1] == '#') left = left + 1;

    // 向左退，跳过数字，停在非数字
    while (left >= 0 && line[left].isDigit()) --left;

    if (left < 0 || line[left] != '#') {
        // 也可能光标正好落在 '#' 上
        if (posInBlock < line.size() && line[posInBlock] == '#') {
            left = posInBlock;
        }
        else {
            return false;
        }
    }

    // 向右收集数字
    int i = left + 1;
    QString digits;
    while (i < line.size() && line[i].isDigit()) { digits += line[i]; ++i; }
    if (digits.isEmpty()) return false;

    // （可选）判断是否“在括号内”
    // 若严格要求括号内，可打开下列判断：
    // int lparen = line.lastIndexOf('(', left);
    // int rparen = line.indexOf(')', i);
    // if (!(lparen >= 0 && rparen >= 0 && lparen < left && rparen > i)) return false;

    if (outId) *outId = digits.toInt();
    return true;
}

void MainWindow::highlightIdTokenAt(int pos, int id)
{
    // 选中 "#id=" 这个 token；若没有 '='，则只选到数字末尾
    QTextCursor c(editor_->document());
    c.setPosition(pos);

    const QString sid = QString::number(id);
    int len = 1 + sid.size(); // "#"+digits

    // 如果紧跟着是 '=' 则一起选中
    if (editor_->document()->characterAt(pos + len) == QChar('=')) {
        ++len;
    }

    // 设置选中范围
    c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, len);
    editor_->setTextCursor(c);

    // 用 ExtraSelection 给一点底色
    QTextEdit::ExtraSelection sel;
    sel.cursor = c;
    QTextCharFormat fmt;
    fmt.setBackground(QColor(255, 236, 179)); // 柔和黄
    fmt.setFontWeight(QFont::Bold);
    sel.format = fmt;
    editor_->setExtraSelections({ sel });
}

QString MainWindow::findSchemaExpNearCMake()
{
    // 1) 从可执行文件目录开始，向上找到含 CMakeLists.txt 的“项目根”
    QDir start(QCoreApplication::applicationDirPath());
    QDir dir = start;
    int guard = 0;
    while (!dir.exists("CMakeLists.txt")) {
        if (!dir.cdUp() || ++guard > 12) break; // 最多向上 12 层
    }

    // 2) 如果找到了项目根，在 <项目根>/resource/ 里找 .exp
    if (dir.exists("CMakeLists.txt")) {
        QDir res(dir.filePath("resource"));
        if (res.exists()) {
            // 优先 GFC*.exp，其次 *.exp；按时间排序（最新优先）
            QFileInfoList picks = res.entryInfoList(QStringList() << "GFC*.exp",
                QDir::Files, QDir::Time);
            if (picks.isEmpty()) {
                picks = res.entryInfoList(QStringList() << "*.exp",
                    QDir::Files, QDir::Time);
            }
            if (!picks.isEmpty())
                return picks.first().absoluteFilePath();
        }
        // 项目根存在但 resource/ 里没有 .exp
        return QString();
    }

    // 3) 没找到项目根（例如从奇怪路径启动），退回到 exe 同级的 resource/ 里再找一次
    {
        QDir res(start.filePath("resource"));
        if (res.exists()) {
            QFileInfoList picks = res.entryInfoList(QStringList() << "GFC*.exp",
                QDir::Files, QDir::Time);
            if (picks.isEmpty()) {
                picks = res.entryInfoList(QStringList() << "*.exp",
                    QDir::Files, QDir::Time);
            }
            if (!picks.isEmpty())
                return picks.first().absoluteFilePath();
        }
    }

    // 4) 两处都没有
    return QString();
}


// 程序启动时自动加载 .exp
void MainWindow::autoLoadSchemaOnStartup()
{
    const QString path = findSchemaExpNearCMake();
    if (path.isEmpty()) {
        statusBar()->showMessage(QStringLiteral("未找到 Schema(.exp)。请将 .exp 放到含 CMakeLists.txt 的目录。"), 4000);
        rebuildClassTree(); // 仍然构建一棵“未加载”提示树
        return;
    }

    QString err;
    if (!schema_.parseFile(path, &err)) {
        QMessageBox::warning(this, QStringLiteral("解析 Schema 失败"),
            QStringLiteral("文件：%1\n错误：%2").arg(path, err));
        rebuildClassTree();
        return;
    }

    currentSchemaPath_ = path;
    children_ = schema_.buildChildrenMap();
    prepareSchemaIndex();    // 你已有的大小写映射（lower -> CamelCase）
    rebuildClassTree();

    updateWindowTitle();
    statusBar()->showMessage(QStringLiteral("已自动加载 Schema：%1")
        .arg(QFileInfo(path).fileName()), 3000);
}
