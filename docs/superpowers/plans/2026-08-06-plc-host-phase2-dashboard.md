# Phase 2: 可编辑看板

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 实现多页面自由画布看板编辑器，支持全部组件类型的添加、拖动、缩放、删除、撤销和重做，以及五种按钮动作的编辑/运行双模式隔离。

**Architecture:** QGraphicsScene/QGraphicsView 自由画布 + QUndoStack 撤销 + DashboardRepository SQLite 持久化。编辑模式严禁 PLC 写入，运行模式锁定布局只允许按钮操作。

**Tech Stack:** C++20, Qt 6.8 Widgets (GraphicsView, Undo), Qt SQL (SQLite), CMake

## Global Constraints

- 编辑模式绝不向 PLC 写值。验证方式: 拦截 IModbusClient 写调用，确保编辑模式下 count=0。
- 按钮只能绑定 Tag，不能直接填写裸 Modbus 地址。
- 组件配置 JSON 带 schemaVersion，损坏时占位框不阻塞其他页面。
- 看板保存前校验页面 ID、几何、绑定和动作参数。
- 点动按钮默认最大保持 3s，窗口失焦/页面切换/退出运行模式时释放。
- 每项任务: 失败测试 → 确认 → 实现 → 通过 → 全量 ctest → @reviewer → 修复。
- Luna 在 DASH-11 后对程序截图做多模态视觉审查。

## 目录结构 (Phase 2 新增)

```
src/dashboard/
├── DashboardDocument.h      # DashboardPage, DashboardItem 结构体
├── DashboardScene.h/cpp     # QGraphicsScene 子类: item 管理、选择、编辑/运行模式
├── DashboardView.h/cpp      # QGraphicsView 子类: 缩放、平移、拖拽
├── DashboardRepository.h/cpp # SQLite 读写 pages/items
├── DashboardController.h/cpp # 顶层协调: 页面切换、模式切换、保存/恢复
├── commands/
│   ├── AddItemCommand.h/cpp
│   ├── RemoveItemCommand.h/cpp
│   ├── MoveCommand.h/cpp
│   ├── ResizeCommand.h/cpp
│   └── PropertyChangeCommand.h/cpp
├── items/
│   ├── DashboardBaseItem.h   # QGraphicsObject 基类: id, itemType, commonStyle, config, schemaVersion, serialization
│   ├── TextItem.h/cpp
│   ├── RectItem.h/cpp
│   ├── ImageItem.h/cpp
│   ├── ValueItem.h/cpp
│   ├── ValueInputItem.h/cpp
│   ├── LedItem.h/cpp
│   ├── SwitchItem.h/cpp
│   ├── ProgressBarItem.h/cpp
│   ├── GaugeItem.h/cpp
│   ├── TrendItem.h/cpp
│   └── ButtonItem.h/cpp
└── runtime/
    ├── ButtonAction.h        # ButtonActionType 枚举, ButtonAction 结构体
    └── ButtonActionExecutor.h/cpp # UI 线程: 点动/切换/固定值/输入/跳转
```

---

### Task DASH-01: 看板页面与组件模型 + 仓储

**Agent:** `@coder`
**Depends on:** CORE-02 (数据库迁移已就绪)

**Files to create:**
- `src/dashboard/DashboardDocument.h`
- `src/dashboard/DashboardRepository.h`
- `src/dashboard/DashboardRepository.cpp`
- `tests/unit/tst_DashboardRepository.cpp`

**DashboardDocument.h** (按 DOC-03 定义):
```cpp
struct DashboardPage { int id=-1; QString name; int width=1920; int height=1080; QString background; int sortOrder=0; };
struct DashboardItem { int id=-1; int pageId=-1; QString itemType; qreal x=0,y=0,width=100,height=100,zOrder=0; QJsonObject commonStyle; QJsonObject config; int schemaVersion=1; };
```

**DashboardRepository 接口:**
- `loadPages() -> QList<DashboardPage>`
- `savePage(DashboardPage) -> bool` (id=-1 时 INSERT 并回填 id)
- `deletePage(int pageId) -> bool`
- `loadItems(int pageId) -> QList<DashboardItem>`
- `saveItems(int pageId, QList<DashboardItem>) -> bool` (事务内 DELETE + INSERT)
- 构造参数: 数据库连接名

**tst_DashboardRepository.cpp 测试:**
- 临时 SQLite 验证 CRUD
- savePage + loadPages 往返
- saveItems 后 loadItems 一致
- deletePage 级联删除 items
- zOrder 保持往返一致

**Commit:** `feat: DashboardPage/DashboardItem model and SQLite repository`

---

### Task DASH-02: DashboardScene 自由画布

**Agent:** `@coder`
**Depends on:** DASH-01

**Files to create:**
- `src/dashboard/DashboardScene.h/cpp`
- `src/dashboard/DashboardView.h/cpp`
- `tests/ui/tst_DashboardScene.cpp`

**DashboardScene (QGraphicsScene 子类):**
- `setPage(const DashboardPage& page)` - 设置画布尺寸和背景
- `addItemToPage(DashboardItem item)` - 工厂方法创建对应类型的 QGraphicsObject
- `setEditMode(bool)` - 切换编辑/运行模式
- `selectedDashboardItems() -> QList<DashboardBaseItem*>` - 只返回 DashboardBaseItem*
- `snapToGrid(QPointF, int gridSize=10) -> QPointF`
- `alignSelected(Qt::Alignment)`
- `bringToFront()/sendToBack()`

**DashboardView (QGraphicsView 子类):**
- 鼠标滚轮缩放, 中键/右键拖拽平移
- `fitToScreen()` 自适应窗口

**tst_DashboardScene.cpp Qt Test:**
- 添加组件后 scene 包含正确数量
- setEditMode(false) 后 ItemIsMovable/Selectable 全部清除
- snapToGrid 正确舍入
- selectedDashboardItems 过滤非 DashboardBaseItem

**Commit:** `feat: DashboardScene with item factory, edit/run mode, snapToGrid, align`

---

### Task DASH-03: 组件几何操作

**Agent:** `@coder`
**Depends on:** DASH-02

**Files to create:**
- `src/dashboard/DashboardScene.cpp` (扩展)
- `tests/ui/tst_DashboardGeometry.cpp`

**添加到 DASH-02 的场景:**
- 多选: RubberBandDrag + SHIFT 选择
- 拖动: ItemIsMovable + 网格吸附
- 缩放: 选中项四角/四边拖拽手柄 (QGraphicsObject 子项实现)
- 层级: bringToFront/sendToBack/stepForward/stepBackward 修改 zValue
- 复制粘贴: Ctrl+C/V 复制选中项 (JSON 序列化中转)
- 删除: Delete 键

**tst_DashboardGeometry.cpp 测试:**
- 移动后位置正确(考虑吸附)
- 缩放后 width/height >= 最小尺寸
- bringToFront 后 zValue 最大
- delete 后 item 从 scene 移除

**Commit:** `feat: item move, resize, z-order, copy/paste, delete geometry operations`

---

### Task DASH-04: 撤销与重做

**Agent:** `@coder`
**Depends on:** DASH-02

**Files to create:**
- `src/dashboard/commands/AddItemCommand.h/cpp`
- `src/dashboard/commands/RemoveItemCommand.h/cpp`
- `src/dashboard/commands/MoveCommand.h/cpp`
- `src/dashboard/commands/ResizeCommand.h/cpp`
- `src/dashboard/commands/PropertyChangeCommand.h/cpp`
- `tests/unit/tst_UndoCommands.cpp`

**AddItemCommand:**
- 构造: DashboardScene*, DashboardItem
- redo: addItemToPage, undo: 从 scene 移除

**MoveCommand:**
- 构造: item*, oldPos, newPos
- redo: setPos(newPos), undo: setPos(oldPos)
- `mergeWith`: 连续移动同对象时返回 true

**ResizeCommand:**
- 构造: item*, oldRect, newRect
- redo/undo 同理 MoveCommand
- mergeWith 连续缩放同对象

**PropertyChangeCommand:**
- 构造: item*, propertyName, oldValue, newValue (QVariant)
- redo/undo 修改对应属性

**tst_UndoCommands.cpp 测试:**
- AddCommand undo 后 item 从 scene 移除
- MoveCommand mergeWith 连续移动合并
- 多次移动 undo 回到初始位置
- PropertyChange undo 恢复旧值

**Commit:** `feat: QUndoCommand subclasses for add, remove, move, resize, property change`

---

### Task DASH-05: 基础组件

**Agent:** `@coder`
**Depends on:** DASH-02

**Files to create:**
- `src/dashboard/items/DashboardBaseItem.h`
- `src/dashboard/items/TextItem.h/cpp`
- `src/dashboard/items/RectItem.h/cpp`
- `src/dashboard/items/ImageItem.h/cpp`
- `src/dashboard/items/ValueItem.h/cpp`
- `src/dashboard/items/LedItem.h/cpp`
- `src/dashboard/items/SwitchItem.h/cpp`
- `tests/ui/tst_DashboardItems.cpp`

**DashboardBaseItem (QGraphicsObject 基类):**
```cpp
class DashboardBaseItem : public QGraphicsObject {
    Q_OBJECT
public:
    int itemId = -1;
    int pageId = -1;
    QString itemType;
    QJsonObject commonStyle;  // fillColor, borderColor, borderWidth, font
    QJsonObject config;       // tagId, min, max, unit, ...
    int schemaVersion = 1;

    virtual QJsonObject serialize() const;
    virtual void deserialize(const QJsonObject& obj);
    virtual void updateFromTagCache(const TagCache& cache) = 0;
    virtual void setEditMode(bool editing);

signals:
    void geometryChanged();
};
```

**TextItem:** paint 渲染文字, 支持 font/color/alignment 通过 commonStyle 配置。
**RectItem:** paint 渲染矩形, 支持填充色/边框色/圆角。
**ImageItem:** paint 渲染 QPixmap, 支持保持比例或拉伸。
**ValueItem:** updateFromTagCache 显示 tag 的工程值。paint 渲染数值文本。
**LedItem:** updateFromTagCache 根据 tag 值渲染绿灯/红灯/灰灯。
**SwitchItem:** 运行模式点击时 emit writeRequested, 切换 On/Off 显示。

**tst_DashboardItems.cpp 测试:**
- 每个组件 serialize/deserialize 往返
- ValueItem updateFromTagCache 显示正确值
- LedItem 根据 bool 值渲染正确颜色
- SwitchItem 运行模式点击发出信号
- 所有组件 editMode=false 时不可移动选择

**Commit:** `feat: DashboardBaseItem and basic components (text, rect, image, value, led, switch)`

---

### Task DASH-06: 高级组件

**Agent:** `@coder`
**Depends on:** DASH-05

**Files to create:**
- `src/dashboard/items/ProgressBarItem.h/cpp`
- `src/dashboard/items/GaugeItem.h/cpp`
- `src/dashboard/items/ValueInputItem.h/cpp`
- `src/dashboard/items/TrendItem.h/cpp`

**ProgressBarItem:** updateFromTagCache 按 tag 值相对 min/max 画填充进度条。文本在条内或条外。
**GaugeItem:** updateFromTagCache 画弧形仪表，标注 min/max/当前值。
**ValueInputItem:** 运行模式双击弹出 QInputDialog，校验范围后 emit writeRequested。
**TrendItem:** 内嵌 QChartView 显示 tag 的实时曲线，config 含 historySeconds/maxPoints。

**Commit:** `feat: advanced dashboard components (progress, gauge, input, trend)`

---

### Task DASH-07: 按钮属性与动作模型

**Agent:** `@coder`
**Depends on:** DASH-05

**Files to create:**
- `src/dashboard/items/ButtonItem.h/cpp`
- `src/dashboard/runtime/ButtonAction.h`
- `tests/unit/tst_ButtonActionModel.cpp`

**ButtonItem:**
- paint 渲染按钮，commonStyle 控制颜色/字体/边框/圆角
- config 含 tagId, ButtonAction (type, paramA, paramB, confirmMessage, targetPageId)
- 编辑模式: 选中后属性面板编辑动作参数
- 运行模式: 正常态/悬停态/按下态/禁用态 四种 paint 样式

**ButtonAction.h (按 DOC-03):**
```cpp
enum class ButtonActionType { Momentary, Toggle, FixedValue, InputValue, NavigatePage };
struct ButtonAction {
    ButtonActionType type = ButtonActionType::FixedValue;
    QVariant paramA;
    QVariant paramB;
    int targetPageId = -1;
    QString confirmMessage;
};
```

**tst_ButtonActionModel.cpp 测试:**
- 五种类型 serialize/deserialize 往返
- config JSON 解析验证
- 无效类型拒绝

**Commit:** `feat: ButtonItem rendering and ButtonAction model with all 5 action types`

---

### Task DASH-08: 按钮执行器

**Agent:** `@coder`
**Depends on:** DASH-07, CORE-07

**Files to create:**
- `src/dashboard/runtime/ButtonActionExecutor.h/cpp`
- `tests/unit/tst_ButtonActionExecutor.cpp`

**ButtonActionExecutor (UI 线程 QObject):**
- `execute(action, tagId)` → emit writeRequested(WriteCommand)
- `releaseMomentary(tagId)` → 发送点动释放写入
- `releaseAllMomentary()` → 遍历所有点动按钮释放
- `isButtonEnabled(tagId)`:
  - 离线 → false
  - Tag 只读 → false
  - TagValue 质量 Bad/Disconnected → false
  - 固定值/输入值/Toggle 校验类型范围失败 → false
- 点动最大保持 3s: setSingleShot QTimer
- 确认弹窗: confirmMessage 非空时 QMessageBox::question

**tst_ButtonActionExecutor.cpp 测试:**
- execute Momentary 发出 writeRequested (command.isRelease=false, priority=0)
- releaseMomentary 发出 writeRequested (command.isRelease=true, priority=1)
- 点动 3s 超时自动 releaseMomentary
- releaseAllMomentary 释放所有点动状态
- isButtonEnabled 离线返回 false
- isButtonEnabled 只读返回 false
- isButtonEnabled Bad 返回 false
- FixedValue 无 confirmMessage 直接发送
- Toggle 计算下次值正确
- NavigatePage emit pageNavigationRequested

**Commit:** `feat: ButtonActionExecutor with momentary timeout, 5 action types, and safety guards`

---

### Task DASH-09: 页面跳转与编辑/运行模式隔离

**Agent:** `@coder`
**Depends on:** DASH-02, DASH-08

**Files to create:**
- `src/dashboard/DashboardController.h/cpp`
- `tests/ui/tst_DashboardRuntime.cpp`

**DashboardController:**
- 持有 DashboardScene, DashboardView, DashboardRepository, ButtonActionExecutor
- `loadPage(int pageId)` - 清空 scene, 加载 items
- `switchPage(int pageId)` - 切换到新页面, releaseAllMomentary
- `setEditMode(bool)` - 切换模式, releaseAllMomentary
- `save()` / `saveAsDraft()` / `restoreDraft()`
- `addComponent(QString itemType)` - 工厂创建默认组件

**tst_DashboardRuntime.cpp 测试:**
- 编辑模式切换到运行模式，所有 item 不可移动选择
- 编辑模式下调用 IModbusClient::sendWriteRequest 次数为 0
- 切换页面时 releaseAllMomentary 被调用
- 切换模式时 releaseAllMomentary 被调用

**Commit:** `feat: DashboardController with page switching and edit/run mode isolation`

---

### Task DASH-10: 保存、草稿恢复和损坏组件隔离

**Agent:** `@coder`
**Depends on:** DASH-01, DASH-09

**Files to create:**
- `src/dashboard/DashboardController.cpp` (扩展)
- `tests/unit/tst_DashboardRecovery.cpp`

**保存流程:**
1. 序列化所有 item 为 JSON
2. 校验: pageId 有效, item type 已知, 几何非负, config 可解析
3. 事务内 saveItems
4. setClean

**草稿:**
- 定时 (30s) 和模式切换时自动保存到 app_settings (key: "draft_<pageId>")
- 启动时检查草稿是否比已保存版本新，提示恢复
- 正常保存后删除草稿

**损坏组件:**
- deserialize 失败 → 创建 "ErrorPlaceholder" item (黄色占位框，显示错误信息)
- 不影响其他 item 加载

**tst_DashboardRecovery.cpp 测试:**
- 保存校验: 无效 pageId 被拒绝
- 保存后 isClean=true
- 草稿创建和恢复
- 损坏 config 被 ErrorPlaceholder 替代
- 损坏 item 不阻止其他 item 加载

**Commit:** `feat: dashboard save validation, draft recovery, and corrupted item isolation`

---

### Task DASH-11: 按 Luna 规范实现视觉主题

**Agent:** `@coder`
**Depends on:** DASH-05, DASH-06, DASH-07, UI-02 (Luna 视觉设计完成)

**Files to modify:**
- `src/dashboard/items/*.cpp` - 所有 paint()
- `src/dashboard/DashboardScene.cpp` - 背景渲染
- `src/dashboard/DashboardView.cpp` - 滚动条样式
- `src/app/MainWindow.cpp` - 全局样式表

**实现 Luna 视觉规范中的:**
- 设计 token: 颜色、字体、间距、圆角、阴影
- 组件状态: 正常、悬停、按下、禁用
- 质量状态颜色: Good=绿, Stale=黄, Bad=红, Disconnected=灰
- 编辑模式与运行模式视觉区别
- 编辑模式下选中/未选中/多选/缩放手柄样式

**验收:** Luna 对程序截图做多模态视觉审查，偏差由 @coder 修复。

**Commit:** `feat: apply Luna visual design tokens to all dashboard components and states`

---

## Phase 2 审查节点

| 节点 | 触发 | 审查重点 |
|---|---|---|
| RG-4 | DASH-04 完成 | undo command 对象生命周期、scene 所有权、内存泄漏 |
| RG-5 | DASH-08 完成 | 点动释放完整性、窗口失焦、超时、按钮禁用条件 |
| RG-6 | DASH-09 完成 | 编辑模式写请求=0 验证、模式切换释放点动 |
| RG-7 | DASH-11 完成 | Luna 多模态截图审查 + @reviewer 全量看板测试 |
