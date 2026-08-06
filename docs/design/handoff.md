# UI-07 Luna 设计交付

> 交付对象：`@coder`
> 设计来源：UI-03 `design-tokens.md`、UI-04 `information-architecture.md`、UI-05 `interaction-states.md`、UI-06 `component-spec.md`
> 本文是视觉实现和验收的最终清单；所有尺寸均为 Qt 逻辑 px。

## 1. 文件映射表

下表把每份设计文档映射到当前仓库的 C++ 实现落点。实现时优先复用这些类的现有职责；颜色、状态和尺寸应集中遵循设计 token，不在单个组件中重新定义语义。

> **UI-07 颜色冻结说明：** 质量状态的最终视觉验收值以本文第 2 节为准（Good `#34C759`、Stale `#FF9500`、Bad `#FF3B30`、Disconnected `#8E8E93`）。若现有 token 或实现仍使用其他质量色，应在统一 token 映射处调整，不要在组件内散落覆盖。

| 设计文档 | 主要实现职责 | 对应 C++ 源文件 |
|---|---|---|
| UI-03 `docs/design/design-tokens.md` | 应用背景、导航/焦点/选中强调色、画布/网格、字体、间距、按钮状态、质量/报警状态、DPI 几何 | `src/app/MainWindow.h`, `src/app/MainWindow.cpp`<br>`src/dashboard/DashboardView.h`, `src/dashboard/DashboardView.cpp`<br>`src/dashboard/DashboardScene.h`, `src/dashboard/DashboardScene.cpp`<br>`src/dashboard/DashboardBaseItem.h`<br>`src/ui/PlcConfigWidget.h`, `src/ui/PlcConfigWidget.cpp`<br>`src/ui/TagEditorWidget.h`, `src/ui/TagEditorWidget.cpp`<br>`src/ui/TagMonitorWidget.h`, `src/ui/TagMonitorWidget.cpp`<br>看板组件绘制：`src/dashboard/items/TextItem.h/.cpp`, `RectItem.h/.cpp`, `ImageItem.h/.cpp`, `ValueItem.h/.cpp`, `ValueInputItem.h/.cpp`, `LedItem.h/.cpp`, `SwitchItem.h/.cpp`, `ProgressBarItem.h/.cpp`, `GaugeItem.h/.cpp`, `TrendItem.h/.cpp`, `ButtonItem.h/.cpp` |
| UI-04 `docs/design/information-architecture.md` | 主窗口四区骨架、导航和工作区切换、看板页/编辑运行模式、页面持久化、空状态入口 | `src/app/MainWindow.h`, `src/app/MainWindow.cpp`<br>`src/dashboard/DashboardController.h`, `src/dashboard/DashboardController.cpp`<br>`src/dashboard/DashboardView.h`, `src/dashboard/DashboardView.cpp`<br>`src/dashboard/DashboardScene.h`, `src/dashboard/DashboardScene.cpp`<br>`src/dashboard/DashboardRepository.h`, `src/dashboard/DashboardRepository.cpp`<br>`src/dashboard/DashboardDocument.h`<br>`src/ui/PlcConfigWidget.h/.cpp`, `src/ui/TagEditorWidget.h/.cpp`, `src/ui/TagMonitorWidget.h/.cpp`<br>趋势、报警、历史、配方、日志目前没有独立页面 C++ 源文件；集成入口为 `MainWindow` 的导航/工作区。 |
| UI-05 `docs/design/interaction-states.md` | 选中/多选、拖拽、吸附、缩放、撤销、编辑/运行锁定、按钮五态、点动释放、空/错误状态 | `src/dashboard/DashboardBaseItem.h`<br>`src/dashboard/DashboardScene.h`, `src/dashboard/DashboardScene.cpp`<br>`src/dashboard/DashboardView.h`, `src/dashboard/DashboardView.cpp`<br>`src/dashboard/commands/AddItemCommand.h/.cpp`, `RemoveItemCommand.h/.cpp`, `MoveCommand.h/.cpp`, `ResizeCommand.h/.cpp`, `PropertyChangeCommand.h/.cpp`<br>`src/dashboard/items/ButtonItem.h`, `src/dashboard/items/ButtonItem.cpp`<br>`src/dashboard/runtime/ButtonAction.h`<br>`src/dashboard/runtime/ButtonActionExecutor.h`, `src/dashboard/runtime/ButtonActionExecutor.cpp`<br>`src/dashboard/DashboardController.h`, `src/dashboard/DashboardController.cpp`<br>`src/runtime/AcquisitionEngine.h`, `src/runtime/AcquisitionEngine.cpp`<br>`src/runtime/TagCache.h`, `src/runtime/TagCache.cpp`<br>`src/domain/TagValue.h` |
| UI-06 `docs/design/component-spec.md` | 11 种 Dashboard 组件的默认/最小/最大尺寸、属性、序列化、绘制、工具箱和运行差异 | `src/dashboard/DashboardDocument.h`<br>`src/dashboard/DashboardBaseItem.h`<br>`src/dashboard/DashboardScene.h`, `src/dashboard/DashboardScene.cpp`<br>`src/dashboard/DashboardRepository.h`, `src/dashboard/DashboardRepository.cpp`<br>`src/dashboard/items/TextItem.h/.cpp`, `RectItem.h/.cpp`, `ImageItem.h/.cpp`, `ValueItem.h/.cpp`, `ValueInputItem.h/.cpp`, `LedItem.h/.cpp`, `SwitchItem.h/.cpp`, `ProgressBarItem.h/.cpp`, `GaugeItem.h/.cpp`, `TrendItem.h/.cpp`, `ButtonItem.h/.cpp`<br>`src/dashboard/runtime/ButtonAction.h`<br>`src/dashboard/runtime/ButtonActionExecutor.h`, `src/dashboard/runtime/ButtonActionExecutor.cpp` |

### 1.1 对应的 C++ 验证入口

视觉实现完成后，优先沿用现有 UI/单元测试并补充缺口：

- `tests/ui/tst_UiSmoke.cpp`
- `tests/ui/tst_DashboardScene.cpp`
- `tests/ui/tst_DashboardGeometry.cpp`
- `tests/ui/tst_DashboardItems.cpp`
- `tests/ui/tst_DashboardRuntime.cpp`
- `tests/unit/tst_ButtonActionModel.cpp`
- `tests/unit/tst_ButtonActionExecutor.cpp`
- `tests/unit/tst_UndoCommands.cpp`

## 2. 视觉验收清单

### 2.1 颜色、画布和状态

- [ ] 全局背景色与设计 token 一致：编辑主背景 `#F2F5F7`、编辑画布 `#F8FAFC`；运行主背景 `#151E26`、运行画布 `#111820`。
- [ ] 左侧导航高亮色正确：编辑模式使用 `accent #0078D4`，运行模式使用 `#5BB9F5`；同时有活动背景/左侧标线/文字或图标语义，不能只靠颜色。
- [ ] 看板编辑器网格线颜色和间距正确：`grid #E2EAF0`，10px 网格/吸附步长；运行模式隐藏网格。
- [ ] 选中组件蓝色边框 2px + 8 个缩放手柄：边框 `#007AFF`；手柄为 8px × 8px 白色方块、蓝色边框，位于四角和四边中点；运行模式全部隐藏。
- [ ] 吸附参考线绿色虚线：`snap_guide #00FF00`，仅在拖拽/对齐期间显示，释放后移除且不持久化。
- [ ] 按钮 5 种状态颜色与 token 一致：Normal、Hover、Pressed、Disabled、Waiting；状态切换不改变尺寸，Disabled/Waiting 阻止重复操作并显示可读原因/等待语义。
- [ ] 质量状态颜色：`Good=#34C759`、`Stale=#FF9500`、`Bad=#FF3B30`、`Disconnected=#8E8E93`；每种状态同时显示文字、图标或提示，颜色不是唯一信息。
- [ ] 报警颜色：`Critical=#FF3B30`、`Warning=#FF9500`、`Info=#007AFF`；严重度同时显示图标和文字。
- [ ] 编辑/运行模式视觉区分明显：编辑为浅色、显示工具箱/属性面板/网格并可编辑；运行为深色、隐藏编辑层并锁定布局。

### 2.2 尺寸、字体和布局

- [ ] 1366×768 分辨率布局不错乱：顶部栏、左导航、工作区和底部状态栏不重叠；导航默认宽度 224px（允许 196–280px），顶部 44px、底部 26px。
- [ ] 125%/150%/200% DPI 缩放正确：文字、命中区、网格、选中框、手柄和状态提示不裁切、不重叠；布局几何语义不因模式切换改变。
- [ ] 所有空状态占位显示正确：加载中“正在加载”；无 Tag“暂无变量，点击添加”；空看板“拖入组件开始编辑”；历史无数据“所选时间范围无数据”；无报警“当前无报警 ✓”；空配方“点击新建创建第一个配方”；每个状态保留下一步操作。
- [ ] 字体：Segoe UI，大小与 token 一致：h1 18px、h2 14px、body 12px、small 10px、value display 20px、gauge label 11px；缺失时回退 Microsoft YaHei UI。
- [ ] 组件最小/最大尺寸限制有效：属性输入、导入、拖拽缩放使用同一套钳制规则；通用编辑命中基线至少 60px × 40px，但持久化几何遵循组件专属边界。

### 2.3 组件尺寸基线

| 组件 | 默认尺寸 | 最小尺寸 | 最大尺寸 |
|---|---:|---:|---:|
| `TextItem` | 200 × 40 | 40 × 20 | 800 × 200 |
| `RectItem` | 150 × 100 | 20 × 20 | 2000 × 2000 |
| `ImageItem` | 200 × 150 | 40 × 30 | 2000 × 2000 |
| `ValueItem` / `ValueInputItem` | 120 × 60 | 60 × 30 | 400 × 200 |
| `LedItem` | 60 × 60 | 30 × 30 | 200 × 200 |
| `SwitchItem` | 80 × 40 | 40 × 20 | 200 × 100 |
| `ProgressBarItem` | 200 × 30 | 60 × 20 | 600 × 80 |
| `GaugeItem` | 150 × 150 | 80 × 80 | 400 × 400 |
| `TrendItem` | 300 × 200 | 150 × 100 | 800 × 600 |
| `ButtonItem` | 120 × 40 | 60 × 30 | 400 × 200 |

## 3. 设计决策说明

### 3.1 为什么编辑模式用浅色、运行模式用深色

编辑模式服务于布局和配置：浅色画布能让 `#007AFF` 选中框、`#00FF00` 参考线、网格和组件边界保持清晰对比，属性编辑和空间关系更容易检查。运行模式服务于持续监控和操作：深色 `runtime_canvas #111820` 降低大面积画面的眩光和视觉噪声，把实时数值、质量、报警和控制反馈置于前景；同时隐藏网格、工具箱、属性面板和选中层，避免操作员把编辑辅助物误认为运行内容。两种模式仍共享顶部栏、导航、底部状态栏和组件几何，切换只改变视觉与权限，不改变空间记忆。

### 3.2 为什么选中边框用蓝色而不是其他颜色

蓝色是界面的活动/焦点语义，能在浅色编辑画布上形成稳定的高对比轮廓，并与状态色分工：绿色专门表示吸附参考线，橙/红表示质量或报警，灰色表示断开/不可用。因此选中不会被误解为报警或 PLC 质量。实现上选中框使用 UI-05 冻结的 `#007AFF`、2px；导航/焦点继续使用 token 的 `accent`（编辑 `#0078D4`、运行 `#5BB9F5`），两者不要混为一个语义。

### 3.3 网格线的作用

网格是编辑辅助，不是运行内容：它以低对比度 `#E2EAF0` 和 10px 间距提供稳定的空间参照，让添加、移动、缩放和对齐得到可预测的几何结果，并帮助多个组件保持整齐间距。吸附参考线在此基础上临时提示组件边缘/中心线对齐；释放鼠标后移除。网格、参考线和选中层都不写入持久化组件内容，运行模式全部隐藏。

## 4. 交付给 coder 的执行提醒

1. 先统一 token 和模式判定，再实现各组件绘制；禁止在组件中用与 token 不同的散落颜色表达同一状态。
2. 任何质量、报警、Disabled、Waiting、空数据和错误状态都必须保留文字/图标语义。
3. 编辑模式不得到达 PLC 写入执行器；模式切换/切页前释放点动状态，运行模式不得改变保存的组件几何。
4. 视觉验收至少覆盖 `1366 × 768` 和 125%/150%/200% DPI，并记录清单中的每项结果。
