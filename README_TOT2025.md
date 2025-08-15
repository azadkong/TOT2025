# GFC 编辑器（TOT2025）— 项目说明

> 版本：2025-08-15  
> 语言/框架：C++17 · Qt Widgets（优先 Qt6，自动回退 Qt5） · CMake

## 1. 项目概览
本项目是一个**GFC 文本编辑器**与**类结构浏览/定位工具**：
- 读取 **EXP（Express）模式文件**，解析出实体（ENTITY）间的**继承关系**与**属性顺序**；
- 读取 **GFC 数据文件**（示例：`resource/圆柱体【拉伸体】.gfc`），在左侧**类继承树**中展示**实例计数**（直接计数与沿父链汇总计数）；
- 文本区与视图区、属性区**联动**：
  - 点击类树中的**实例节点**，文本自动**定位并高亮**该实例；
  - 在文本区点击一行（或 **Ctrl+单击 #id**），右侧属性表显示该实例的**参数**，并按 .exp 中的属性顺序**对齐显示**；
- 内置**查找/替换**，并提供底部**查找结果定位列表**；
- 支持 **GFC 语法高亮**（字符串、数字、`#id=`、类名、注释，以及 `HEADER`/`DATA` 关键字）。

## 2. 目录结构
```
TOT2025/
  新建文件夹/
    CMakeLists.txt
    resource/
      GFC3X4.exp
      圆柱体【拉伸体】.gfc
    src/
      expressparser.h/.cpp
      gfcparser.h/.cpp
      main.cpp
      mainwindow.h/.cpp
```
- **resource/GFC3X4.exp**：示例 Schema，定义实体、继承与属性。  
- **resource/圆柱体【拉伸体】.gfc**：示例 GFC 数据文件（包含多种实体实例）。

## 3. 构建与运行
### 依赖
- CMake ≥ 3.16  
- Qt ≥ 6（若未安装，自动回退到 Qt5 Widgets）  
- MSVC/Clang/GCC 任一 C++17 编译器

### 生成
```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="<Qt 安装路径>"
cmake --build build --config Release
```
> Windows/VS 用户可直接使用 **“打开文件夹”** 或 CMake GUI。`CMAKE_PREFIX_PATH` 指向 Qt 的安装前缀。

### 运行
- 启动可执行程序 **GFCEditor**。
- 依次在菜单 **文件 → 打开 .exp**、**文件 → 打开 .gfc**。

## 4. 主要功能
- **文件**
  - 打开/保存 GFC；最近文件菜单（最多 5 个）。
  - 打开 .exp（Express）建立 Schema：继承关系与属性列表。

- **编辑/查看**
  - GFC 语法高亮：字符串、数字、`#id=`、类名、注释、`HEADER`/`DATA`。
  - 状态栏显示：光标行列、文本大小（KB）。
  - 后退/前进定位导航。

- **视图区（类继承树）**
  - 根为无父类的实体；子节点为派生类。
  - 计数：直接计数（类自身实例数）+ 汇总计数（含子类实例总数）。
  - 展开类可见该类的**实例节点**（形如 `#12 ClassName`），双击/点击可在文本中定位并高亮。

- **属性区**
  - 在文本区点击某实例行，或在类树中选择实例节点，即可在右侧按 .exp 属性顺序显示参数。
  - 当类未在 .exp 映射时，按位置顺序显示 `<extra #n>` 名称。

- **查找/替换 + 定位列表**
  - 弹窗内支持：区分大小写、全字匹配。
  - “查找”同时**填充底部列表**：每一处命中显示**行/列/上下文**，双击即可跳转。

- **交互增强**
  - 按住 **Ctrl** 在 `#数字` 上移动变为**手形**，**Ctrl+左键**可跳转到该实例定义位置（并高亮 `#id=`）。
  - 文本区点击实例定义行：仅**高亮**与**属性区更新**，**不移动光标**，便于继续编辑。

## 5. 核心模块与关键 API
### 5.1 `ExpressParser`（极简 .exp 解析）
- 解析 **ENTITY 名称**、`SUBTYPE OF(...)` **父类**、以及**属性行**（`Name : Type;`）。
  - `bool parseFile(const QString& filePath, QString* err=nullptr)`：读取并构建 `classes()` 映射。
  - `const QHash<QString, ExpClassInfo>& classes() const`：返回 CamelCase 实体信息（包含 parent/attributes）。
  - `QHash<QString, QSet<QString>> buildChildrenMap() const`：基于父类关系生成**子类集合**。

### 5.2 `GfcParser`（GFC 文本扫描）
- 扫描 `DATA;` 段内的实例定义：`#12=GFCWALL(...);`
  - `QHash<QString,int> countClasses(...)`：按**大写类名**统计出现次数，并可同时收集实例引用（`index/classUpper/pos`）。
  - `static int parseInstanceIndex("#123")`：提取实例序号。
  - `static bool parseInstanceAt(text, startPos, ParsedInstance* out)`：在给定位置解析出**完整实例**与其参数列表。

### 5.3 `MainWindow`（应用外壳与联动逻辑）
- 菜单/工具栏/状态栏与停靠窗体（视图区、属性区、查找结果）。
  - `enableGfcSyntaxColors()`：启用语法高亮器。
  - `recomputeFromText()`：从全文重算**实例映射/计数**，并生成 `instancesByCamel_`。
  - `rebuildClassTree()`：按 Schema 建树并附上**计数**与**实例节点**。
  - `showInstanceByPos(pos, moveCaret)`：从某个文本位置解析实例并在属性区展示。
  - `ctrlClickJumpToInstance(viewPos)` / `findInstancePosition(id)` / `highlightIdTokenAt(...)`：Ctrl 点击跳转与高亮。
  - `runFindAll(pattern, flags)` / `onFindResultActivated(...)`：填充并响应**查找结果**表格。

## 6. 典型工作流
1. **加载 Schema(.exp)** → `ExpressParser::parseFile()` → `classes()`（CamelCase） → `buildChildrenMap()`（子类映射） → `prepareSchemaIndex()`（大小写无关映射）。  
2. **加载 GFC** → 读取文本 → `enableGfcSyntaxColors()` → `recomputeFromText()`（把大写类映射为 CamelCase，统计 direct & inclusive） → `rebuildClassTree()`。  
3. **联动**：
  - 文本点击实例行 → `showInstanceByPos()` → 右侧**属性表更新** + **额外高亮**该实例行；
  - 类树点击实例节点 → `showInstanceByPos(pos,true)` → 文本**定位+高亮**；
  - Ctrl+单击 #id → `ctrlClickJumpToInstance()` → 文本**跳转到定义**并**选中 `#id=`**。

## 7. 示例数据
- `resource/GFC3X4.exp`：提供了多个实体、继承层级与属性声明；
- `resource/圆柱体【拉伸体】.gfc`：包含 `GFCVECTOR3D`、`GFCEXTRUDEDBODY`、`GFCELEMENT` 等多种实体实例，可用于体验计数与联动。

## 8. 已知限制 & 后续改进
- `.exp` 解析为轻量实现，未覆盖完整 EXPRESS 语法（例如复杂约束/where 语句等）；
- `.gfc` 参数解析按**顶层逗号**切分，字符串/括号嵌套已处理，但未做跨行拼接与注释块剔除的所有边角；
- 尚未实现**撤销/重做历史导航**与**多文件会话**；
  - 类树**快速过滤/搜索**；
  - 属性表**复制导出**与**类型感知**渲染；
  - 大文件的**增量重算**与**异步解析**；
  - 更多 Schema 关键字与类型支持。

## 9. 许可证
此仓库未附带明确的开源许可证，默认**保留所有权利**。如需开源，请补充许可证文件。

---

**作者建议**：首次运行请先打开 `resource/GFC3X4.exp`，再打开 `resource/圆柱体【拉伸体】.gfc`，即可观察类树计数、点击联动与查找结果列表。