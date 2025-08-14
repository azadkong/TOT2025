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
#include <QInputDialog>
#include <QCheckBox>
#include "gfcparser.h"
#include <QVBoxLayout>
#include <QDebug>
#include <QPushButton>
// ---------- 工具：根据平台设置UTF-8 ----------
static void setUtf8(QTextStream& ts) {
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    ts.setEncoding(QStringConverter::Utf8);
#else
    ts.setCodec("utf-8");
#endif
}

MainWindow::MainWindow(QWidget *parent)
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
    connect(editor_, &QPlainTextEdit::cursorPositionChanged, this, &MainWindow::onCursorPosChanged);
    connect(editor_, &QPlainTextEdit::textChanged, this, [this]{
        // 文本变动时更新大小显示
        const int bytes = editor_->toPlainText().toUtf8().size();
        lblSize_->setText(QStringLiteral("大小: %1 KB").arg(QString::number(bytes/1024.0, 'f', 2)));
    });

    updateWindowTitle();
}

// ================== UI 构建 ==================
void MainWindow::buildMenusAndToolbar()
{
    // 文件
    auto mFile = menuBar()->addMenu(QStringLiteral("文件"));
    auto actNew  = mFile->addAction(QStringLiteral("新建 (&N)"));
    actNew->setShortcut(QKeySequence::New);
    connect(actNew, &QAction::triggered, this, &MainWindow::newFile);

    auto actOpen = mFile->addAction(QStringLiteral("打开GFC (&O) ..."));
    actOpen->setShortcut(QKeySequence::Open);
    connect(actOpen, &QAction::triggered, this, &MainWindow::openGfc);

    auto actSave = mFile->addAction(QStringLiteral("保存 (&S)"));
    actSave->setShortcut(QKeySequence::Save);
    connect(actSave, &QAction::triggered, this, &MainWindow::saveGfc);

    auto actSaveAs = mFile->addAction(QStringLiteral("另存为 (&A) ..."));
    connect(actSaveAs, &QAction::triggered, this, &MainWindow::saveGfcAs);

    mFile->addSeparator();
    auto actOpenExp = mFile->addAction(QStringLiteral("打开Schema(.exp) ..."));
    connect(actOpenExp, &QAction::triggered, this, &MainWindow::openSchemaExp);

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

    //帮助
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
//新
void MainWindow::showHelpDocument() {
    // 假设帮助文档是一个简单的文本或 HTML
    QString helpContent = "欢迎使用本程序！\n\n"
        "功能简介：\n"
        "1. 文件菜单：打开、保存、另存为等功能。\n"
        "2. 编辑菜单：查找、替换等功能。\n"
        "3. 视图菜单：切换不同的视图模式。\n"
        "4. 工具菜单：设置文本颜色、帮助等功能。\n\n"
        "更多帮助请参考官方文档。";

    // 使用消息框显示帮助内容
    QMessageBox::information(this, QStringLiteral("帮助文档"), helpContent);
}
void MainWindow::showAboutDialog() {
    // 创建并设置关于对话框的内容
    QMessageBox::about(this, QStringLiteral("关于"),
        QStringLiteral("应用名称: GFC 编辑器\n")
        + QStringLiteral("版本: 1.0.0\n")
        + QStringLiteral("作者: 你的名字\n")
        + QStringLiteral("说明: 这是一个用于编辑 GFC 文件的应用。"));
}
//新

void MainWindow::buildDocks()
{
    // 视图区（类继承树）
    classTree_ = new QTreeView(this);
    classTree_->setHeaderHidden(true);
    classModel_ = new QStandardItemModel(this);
    classModel_->setHorizontalHeaderLabels({QStringLiteral("类/统计")});
    classTree_->setModel(classModel_);
    connect(classTree_, &QTreeView::clicked, this, &MainWindow::onClassTreeClicked);

    auto dockClass = new QDockWidget(QStringLiteral("视图区 - 类继承"), this);
    dockClass->setObjectName("dockClassView");
    dockClass->setWidget(classTree_);
    addDockWidget(Qt::LeftDockWidgetArea, dockClass);

    // 属性区（显示 Schema 中该类属性定义；后续可切换为实例属性）
    propTable_ = new QTableWidget(this);
    propTable_->setColumnCount(2);
    propTable_->setHorizontalHeaderLabels({QStringLiteral("属性名/定义"), QStringLiteral("类型/备注")});
    propTable_->horizontalHeader()->setStretchLastSection(true);

    auto dockProp = new QDockWidget(QStringLiteral("属性区"), this);
    dockProp->setObjectName("dockProp");
    dockProp->setWidget(propTable_);
    addDockWidget(Qt::RightDockWidgetArea, dockProp);

    // 允许浮动/停靠
    dockClass->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
    dockProp->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
}

void MainWindow::buildStatusBar()
{
    statusBar()->addPermanentWidget(lblPos_);
    statusBar()->addPermanentWidget(lblSize_);
    onCursorPosChanged();
    const int bytes = editor_->toPlainText().toUtf8().size();
    lblSize_->setText(QStringLiteral("大小: %1 KB").arg(QString::number(bytes/1024.0, 'f', 2)));
}

//// ================== 文件相关 ==================旧
//void MainWindow::newFile()
//{
//    editor_->clear();
//    currentFilePath_.clear();
//    classCounts_.clear();
//    rebuildClassTree(); // 清空统计
//    updateWindowTitle();
//}
// ================== 文件相关 ==================新
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
        "FILE_NAME('D:\\\\.gfc');\n"          // 注意反斜杠要转义成 \\\\
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
    updateWindowTitle();
}

void MainWindow::openGfc()
{
    QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开 GFC 文件"), QString(), "GFC (*.gfc)");
    if (path.isEmpty()) return;
    if (loadGfcFromFile(path)) {
        currentFilePath_ = path;
        updateWindowTitle();
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
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("另存为 GFC 文件"), currentFilePath_.isEmpty()? QString() : currentFilePath_, "GFC (*.gfc)");
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
    prepareSchemaIndex();                   // ★ 新增：建立 lower -> CamelCase 映射
    // 清空旧计数（可选）
    directCountCamel_.clear();
    inclusiveCountCamel_.clear();
    instancesByCamel_.clear();
    //rebuildClassTree();
    statusBar()->showMessage(QStringLiteral("已加载 Schema：%1").arg(path), 3000);
}

bool MainWindow::loadGfcFromFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"),
            QStringLiteral("无法打开文件：%1").arg(path));
        return false;
    }

    QTextStream in(&f);
    setUtf8(in);                          // 你工程里已有这个小工具函数
    const QString text = in.readAll();
    editor_->setPlainText(text);

    // —— 1) 扫描 .gfc 实例（保留原始大写类名与位置） ——
    instanceRefs_.clear();
    QVector<GfcInstanceRef> refs;
    GfcParser::countClasses(text, &refs); // 仅提取 #idx / CLASS / pos

    // —— 2) 准备统计容器（以 CamelCase 为唯一键） ——
    directCountCamel_.clear();
    inclusiveCountCamel_.clear();
    instancesByCamel_.clear();

    // 若还未加载 .exp，就没法映射；保留空计数但仍显示文本
    if (schema_.classes().isEmpty()) {
        statusBar()->showMessage(QStringLiteral("提示：未加载 Schema(.exp)，只能显示文本，视图区计数为0。"), 4000);
        rebuildClassTree();
        return true;
    }
    if (lowerToCamel_.isEmpty()) {
        prepareSchemaIndex(); // 根据 schema_ 填充 lower -> Camel 对照表
    }

    // —— 3) 统计“直接实例数”，并把实例放到对应 Camel 类下 ——
    int unknown = 0;
    for (const auto& r : refs) {
        const QString camel = lowerToCamel_.value(r.cls.toLower()); // GFCxxx -> GfcXxx
        if (camel.isEmpty()) {                  // .gfc 中有 schema 里没有的类
            ++unknown;
            continue;
        }
        directCountCamel_[camel] += 1;
        instancesByCamel_[camel].push_back(r);  // 实例节点只挂在“本类”下
    }

    // —— 4) 自下而上把直接数累加到所有祖先，得到含子类总数 ——
    inclusiveCountCamel_ = directCountCamel_;   // 先拷贝一份
    for (auto it = directCountCamel_.cbegin(); it != directCountCamel_.cend(); ++it) {
        QString p = schema_.classes().value(it.key()).parent; // 父类是 CamelCase
        while (!p.isEmpty()) {
            inclusiveCountCamel_[p] += it.value();
            p = schema_.classes().value(p).parent;
        }
    }

    // —— 5) 刷新类视图（使用 schema 的驼峰大小写展示） ——
    currentFilePath_ = path;
    updateWindowTitle();
    rebuildClassTree();

    // —— 6) 调试/提示 —— 
#ifdef QT_DEBUG
    qDebug() << "==== loadGfcFromFile ====";
    qDebug() << "parsed instances:" << refs.size() << "unknown classes:" << unknown;
    for (auto it = directCountCamel_.cbegin(); it != directCountCamel_.cend(); ++it) {
        const QString c = it.key();
        qDebug() << c
            << "direct=" << it.value()
            << "inclusive=" << inclusiveCountCamel_.value(c, it.value())
            << "insts=" << instancesByCamel_.value(c).size();
    }
#endif
    statusBar()->showMessage(
        QStringLiteral("已加载 GFC：解析到 %1 个实例，映射到 %2 个类，忽略未知类 %3。")
        .arg(refs.size()).arg(directCountCamel_.size()).arg(unknown),
        4000);

    return true;
}


bool MainWindow::saveGfcToFile(const QString &path)
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
int MainWindow::computeInclusiveCount(const QString &cls) const
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

    // —— 是否启用“(0/0) 隐藏”：
    // 只有解析到至少一个实例（inclusive 有任意 >0）才隐藏，否则保留整棵 Schema 便于浏览
    bool hasAnyInstance = false;
    for (auto it = inclusiveCountCamel_.cbegin(); it != inclusiveCountCamel_.cend(); ++it) {
        if (it.value() > 0) { hasAnyInstance = true; break; }
    }

    // 找所有根（父类为空），键与 schema 保持 CamelCase
    QSet<QString> roots;
    for (auto it = schema_.classes().cbegin(); it != schema_.classes().cend(); ++it) {
        if (it->parent.isEmpty()) roots.insert(it.key());
    }

    // 递归建树：若启用隐藏且 inclusive==0，则直接裁剪该节点
    std::function<QStandardItem* (const QString&)> makeNode =
        [&](const QString& camel) -> QStandardItem*
        {
            const int direct = directCountCamel_.value(camel, 0);
            const int incl = inclusiveCountCamel_.value(camel, 0);

            if (hasAnyInstance && incl == 0) {
                return nullptr; // 裁剪 (0/0) 节点
            }

            auto* item = new QStandardItem(
                QStringLiteral("%1 (%2/%3)").arg(camel).arg(direct).arg(incl));
            item->setEditable(false);
            item->setData(camel, RoleClassName);
            item->setData(NodeClass, RoleNodeType);

            // 子类
            for (const auto& ch : children_.value(camel)) {
                if (auto* childItem = makeNode(ch)) {
                    item->appendRow(childItem);
                }
            }

            // 直接实例
            const auto insts = instancesByCamel_.value(camel);
            for (const auto& ref : insts) {
                auto* inst = new QStandardItem(
                    QString("#%1 %2").arg(ref.index).arg(ref.cls)); // 保留 .gfc 原始大写类名
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

    // 若全部被裁剪，给个提示项
    if (added == 0) {
        auto* tip = new QStandardItem(
            hasAnyInstance
            ? QStringLiteral("所有 (0/0) 类已隐藏，无可显示节点。")
            : QStringLiteral("当前无实例，未启用隐藏规则（请加载 .gfc）。"));
        tip->setEditable(false);
        classModel_->appendRow(tip);
    }

#ifdef QT_DEBUG
    qDebug() << "==== Rebuilt with pruning ====" << "hasAnyInstance=" << hasAnyInstance;
    for (auto it = directCountCamel_.cbegin(); it != directCountCamel_.cend(); ++it) {
        const QString c = it.key();
        qDebug() << c << "direct=" << it.value()
            << "inclusive=" << inclusiveCountCamel_.value(c, it.value())
            << "insts=" << instancesByCamel_.value(c).size();
    }
#endif
}


void MainWindow::updateParentInstances(const QString& cls)
{
    // 获取当前类的父类
    const auto& ci = schema_.classes().value(cls);
    if (!ci.parent.isEmpty()) {
        // 如果当前类有父类，增加父类的实例数
        classCounts_[ci.parent] += 1;
        instancesByClass_[ci.parent].push_back({ -1, ci.parent, -1 });  // 在父类下创建一个实例（实例编号暂时为-1）

        // 递归更新父类的父类
        updateParentInstances(ci.parent);
    }
}

// ================== 高亮辅助 ==================
void MainWindow::clearHighlights()
{
    // 通过设置空的 ExtraSelections 清空
    editor_->setExtraSelections({});
}

void MainWindow::highlightOccurrences(const QString& token)
{
    clearHighlights();
    if (token.trimmed().isEmpty()) return;

    QList<QTextEdit::ExtraSelection> sels;

    // 高亮策略：查找 "TOKEN(" 或完全匹配 token
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
        fmt.setBackground(QColor(255, 255, 0, 120)); // 半透明黄
        sel.format = fmt;
        sels << sel;
    }

    editor_->setExtraSelections(sels);
}

// ================== 属性区展示 ==================
void MainWindow::showClassProperties(const QString &cls)
{
    propTable_->clearContents();
    propTable_->setRowCount(0);
    propTable_->setHorizontalHeaderLabels({QStringLiteral("属性定义"), QStringLiteral("备注")});

    if (!schema_.classes().contains(cls)) return;

    const auto& info = schema_.classes()[cls];
    propTable_->setRowCount(info.attributes.size());
    for (int i=0; i<info.attributes.size(); ++i) {
        auto* a0 = new QTableWidgetItem(info.attributes[i]);
        auto* a1 = new QTableWidgetItem(QString()); // 可作为备注/类型解析展示位
        a0->setFlags(a0->flags() & ~Qt::ItemIsEditable); // 先只读
        propTable_->setItem(i, 0, a0);
        propTable_->setItem(i, 1, a1);
    }
}

// ================== 交互 ==================
void MainWindow::onCursorPosChanged()
{
    auto c = editor_->textCursor();
    int line = c.blockNumber() + 1;
    int col  = c.positionInBlock() + 1;
    lblPos_->setText(QStringLiteral("行: %1  列: %2").arg(line).arg(col));
}

void MainWindow::onClassTreeClicked(const QModelIndex &idx)
{
    if (!idx.isValid()) return;
    QString cls = idx.data(Qt::UserRole + 1).toString();
    if (cls.isEmpty()) return;

    // 属性区显示该类的Schema属性
    showClassProperties(cls);

    // 在文本区高亮该类实例（匹配“CLS(”）
    highlightOccurrences(cls);
}

void MainWindow::doFind() {
    bool ok = false;

    // 创建查找对话框，获取要查找的文本
    QDialog* findDialog = new QDialog(this);
    findDialog->setWindowTitle(QStringLiteral("查找/替换"));

    // 查找文本框
    QLineEdit* findLineEdit = new QLineEdit(findDialog);
    findLineEdit->setText(lastFindText_);

    // 替换文本框
    QLineEdit* replaceLineEdit = new QLineEdit(findDialog);
    replaceLineEdit->setText(lastReplaceText_);

    // 区分大小写复选框
    QCheckBox* caseCheckBox = new QCheckBox(QStringLiteral("区分大小写"), findDialog);
    caseCheckBox->setChecked(true);  // 默认选中

    // 全字匹配复选框
    QCheckBox* wordCheckBox = new QCheckBox(QStringLiteral("全字匹配"), findDialog);
    wordCheckBox->setChecked(false); // 默认不选中

    // 查找按钮
    QPushButton* findButton = new QPushButton(QStringLiteral("查找"), findDialog);
    QPushButton* findNextButton = new QPushButton(QStringLiteral("查找下一个"), findDialog);
    QPushButton* replaceButton = new QPushButton(QStringLiteral("替换"), findDialog);
    QPushButton* replaceAllButton = new QPushButton(QStringLiteral("替换所有"), findDialog);

    // 设置布局
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

    // 连接信号和槽
    connect(findButton, &QPushButton::clicked, this, [this, findLineEdit, caseCheckBox, wordCheckBox, findDialog] {
        QString searchText = findLineEdit->text();
        bool caseSensitive = caseCheckBox->isChecked();
        bool wholeWord = wordCheckBox->isChecked();

        if (!searchText.isEmpty()) {
            lastFindText_ = searchText;

            // 设置查找的标志
            QTextDocument::FindFlags flags;
            if (caseSensitive) {
                flags |= QTextDocument::FindCaseSensitively;  // 启用区分大小写
            }
            if (wholeWord) {
                flags |= QTextDocument::FindWholeWords;  // 启用全字匹配
            }

            // 执行查找
            QTextCursor cursor = editor_->textCursor();
            cursor = editor_->document()->find(searchText, cursor, flags);

            if (cursor.isNull()) {
                // 如果未找到，从文档开头开始查找
                QTextCursor startCursor(editor_->document());
                cursor = editor_->document()->find(searchText, startCursor, flags);
            }

            if (!cursor.isNull()) {
                editor_->setTextCursor(cursor);
                editor_->ensureCursorVisible();
            }
            else {
                QMessageBox::information(this, QStringLiteral("查找"), QStringLiteral("找不到：%1").arg(searchText));
            }
        }
        });

    connect(findNextButton, &QPushButton::clicked, this, [this, findLineEdit, caseCheckBox, wordCheckBox] {
        QString searchText = findLineEdit->text();
        bool caseSensitive = caseCheckBox->isChecked();
        bool wholeWord = wordCheckBox->isChecked();

        if (!searchText.isEmpty()) {
            lastFindText_ = searchText;

            // 设置查找的标志
            QTextDocument::FindFlags flags;
            if (caseSensitive) {
                flags |= QTextDocument::FindCaseSensitively;  // 启用区分大小写
            }
            if (wholeWord) {
                flags |= QTextDocument::FindWholeWords;  // 启用全字匹配
            }

            // 查找下一个匹配
            QTextCursor cursor = editor_->textCursor();
            cursor = editor_->document()->find(searchText, cursor, flags);

            if (!cursor.isNull()) {
                editor_->setTextCursor(cursor);
                editor_->ensureCursorVisible();
            }
            else {
                QMessageBox::information(this, QStringLiteral("查找"), QStringLiteral("已到文末。"));
            }
        }
        });

    connect(replaceButton, &QPushButton::clicked, this, [this, findLineEdit, replaceLineEdit, caseCheckBox, wordCheckBox] {
        QString searchText = findLineEdit->text();
        QString replaceText = replaceLineEdit->text();
        bool caseSensitive = caseCheckBox->isChecked();
        bool wholeWord = wordCheckBox->isChecked();

        if (!searchText.isEmpty() && !replaceText.isEmpty()) {
            lastFindText_ = searchText;
            lastReplaceText_ = replaceText;

            // 设置查找的标志
            QTextDocument::FindFlags flags;
            if (caseSensitive) {
                flags |= QTextDocument::FindCaseSensitively;  // 启用区分大小写
            }
            if (wholeWord) {
                flags |= QTextDocument::FindWholeWords;  // 启用全字匹配
            }

            // 获取当前光标位置
            QTextCursor cursor = editor_->textCursor();

            // 执行替换
            qDebug() << cursor.selectedText();
            if (cursor.selectedText() == searchText.toUpper()) {  // 如果光标选中的文本就是要查找的内容
                cursor.insertText(replaceText);  // 用替换文本替换选中的内容
                editor_->setTextCursor(cursor);  // 更新光标位置到替换后的文本
                editor_->ensureCursorVisible();  // 确保光标可见
            }
            else {
                // 如果光标没有选中文本，而是位于某个匹配的位置
                cursor = editor_->document()->find(searchText, cursor, flags);
                if (!cursor.isNull()) {
                    cursor.insertText(replaceText);  // 执行替换
                    editor_->setTextCursor(cursor);  // 更新光标位置
                    editor_->ensureCursorVisible();  // 确保光标可见
                }
            }

            // 查找下一个匹配项（光标移动到下一个匹配项，但不进行替换）
            cursor = editor_->document()->find(searchText, cursor, flags);  // 查找下一个匹配项
            if (!cursor.isNull()) {
                editor_->setTextCursor(cursor);  // 更新光标到下一个匹配项
                editor_->ensureCursorVisible();
            }
        }
        });

    connect(replaceAllButton, &QPushButton::clicked, this, [this, findLineEdit, replaceLineEdit, caseCheckBox, wordCheckBox] {
        QString searchText = findLineEdit->text();
        QString replaceText = replaceLineEdit->text();
        bool caseSensitive = caseCheckBox->isChecked();
        bool wholeWord = wordCheckBox->isChecked();

        if (!searchText.isEmpty() && !replaceText.isEmpty()) {
            lastFindText_ = searchText;
            lastReplaceText_ = replaceText;

            // 设置查找的标志
            QTextDocument::FindFlags flags;
            if (caseSensitive) {
                flags |= QTextDocument::FindCaseSensitively;  // 启用区分大小写
            }
            if (wholeWord) {
                flags |= QTextDocument::FindWholeWords;  // 启用全字匹配
            }

            // 执行替换所有
            QTextCursor cursor = editor_->textCursor();
            cursor = editor_->document()->find(searchText, cursor, flags);
            while (!cursor.isNull()) {
                cursor.insertText(replaceText);  // 替换文本
                editor_->setTextCursor(cursor);  // 更新光标位置
                editor_->ensureCursorVisible();  // 确保光标可见
                cursor = editor_->document()->find(searchText, cursor, flags);  // 查找下一个匹配项
            }
        }
        });

    // 显示查找对话框
    findDialog->exec();

    // 对话框保持打开状态，直到用户关闭
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
    bool ok=false;
    QString repl = QInputDialog::getText(this, QStringLiteral("替换"),
                                         QStringLiteral("将当前选中的/下一处匹配替换为："),
                                         QLineEdit::Normal, QString(), &ok);
    if (!ok) return;

    auto c = editor_->textCursor();
    if (c.hasSelection() && c.selectedText() == lastFindText_) {
        c.insertText(repl);
    } else {
        // 找下一处
        QTextDocument::FindFlags flags;
        c = editor_->document()->find(lastFindText_, c, flags);
        if (!c.isNull()) {
            editor_->setTextCursor(c);
            editor_->ensureCursorVisible();
            c.insertText(repl);
        } else {
            QMessageBox::information(this, QStringLiteral("替换"), QStringLiteral("没有更多匹配。"));
        }
    }
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


#include <QRegularExpression>
#include <QMessageBox>

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
    } else if (left + 1 < line.size() && line[left + 1] == '#') {
        left = left + 1;
    } else {
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

