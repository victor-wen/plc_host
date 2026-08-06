# Phase 3.5: Luna 视觉设计

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 由 @frontend (opencode-go/gpt-5.6-luna) 完成全部 Qt Widgets 界面视觉与交互设计，输出可直接交付给 @coder 实现的设计规范。

**Architecture:** Luna 只负责视觉设计和规范文档，不写 C++ 代码。所有 Qt 实现由 @coder 根据此规范完成。

**Tech Stack:** Luna 多模态能力、浏览器截图审查

## Global Constraints

- 遵循工业控制清晰可靠风格，避免通用后台模板和花哨渐变。
- 所有设计覆盖 100%/125%/150%/200% Windows DPI。
- 目标分辨率: 1366x768, 1920x1080, 2560x1440。
- 最终由 Luna 对程序截图做多模态审查。

---

### Task UI-01: 视觉方向提案

**Agent:** `@frontend` (Luna)
**Depends on:** DOC-01 (Qt 文档完成)
**Parallel:** 可与 CORE-01 到 CORE-08 并行

**产出:**
- `docs/design/concepts/light.md` - 工业浅色方案
- `docs/design/concepts/dark.md` - 工业深色方案
- `docs/design/concepts/mixed.md` - 深浅混合方案 (编辑器浅色, 运行画面深色)
- `docs/design/concept-comparison.md` - 三种方案对比和推荐

**每个方案需描述:**

1. 主窗口结构: 侧栏、顶部工具栏、工作区、状态栏的视觉布局
2. 色板: 主色、背景色、文字色、边框色、强调色 (HEX 值)
3. 看板编辑器视觉: 画布背景 (网格/纯色)、选中态 (蓝色边框+手柄)、对齐参考线
4. 运行模式视觉: 与编辑模式的区分 (边框、背景、工具栏变化)
5. 连接状态指示: Good(绿)/Stale(黄)/Bad(红)/Disconnected(灰) 的具体样式
6. 按钮组件: 正常/悬停/按下/禁用/等待应答 五种状态的上色和文字
7. 趋势和报警页面配色

**Commit:** `docs: Luna visual direction proposals (light, dark, mixed) with comparison`

---

### Task UI-02: 视觉方向选定

**用户从 UI-01 三种方案中选择一套。** 选中后进入详细设计。

**Agent:** `@frontend` (Luna)
**Depends on:** UI-01 用户选择

---

### Task UI-03: 设计 Token 输出

**Agent:** `@frontend` (Luna)
**Depends on:** UI-02

**产出:** `docs/design/design-tokens.md`

必须输出的 token:

**颜色:**
```yaml
colors:
  background:
    primary: "#XXXXXX"
    secondary: "#XXXXXX"
    editor_canvas: "#XXXXXX"
    runtime_canvas: "#XXXXXX"
  text:
    primary: "#XXXXXX"
    secondary: "#XXXXXX"
    disabled: "#XXXXXX"
  border:
    default: "#XXXXXX"
    focus: "#XXXXXX"
    hover: "#XXXXXX"
  state:
    good: "#XXXXXX"
    stale: "#XXXXXX"
    bad: "#XXXXXX"
    disconnected: "#XXXXXX"
  alarm:
    critical: "#XXXXXX"
    warning: "#XXXXXX"
    info: "#XXXXXX"
  button:
    normal/bg: "#XXXXXX"
    normal/text: "#XXXXXX"
    hover/bg: "#XXXXXX"
    pressed/bg: "#XXXXXX"
    disabled/bg: "#XXXXXX"
    waiting/bg: "#XXXXXX"
```

**字体:**
```yaml
fonts:
  family: "Segoe UI"
  sizes:
    h1: 18px
    h2: 14px
    body: 12px
    small: 10px
    value_display: 20px  # 看板数值显示
    gauge_label: 11px
```

**间距与尺寸:**
```yaml
spacing:
  component_min_width: 60px
  component_min_height: 40px
  grid_snap: 10px
  panel_padding: 8px
  item_margin: 4px
  button_min_width: 80px
  button_min_height: 36px
```

**圆角与阴影:**
```yaml
corners:
  button_radius: 4px
  panel_radius: 6px
  item_radius: 4px
  gauge_radius: 50%  # 圆形仪表

shadows:
  item_selected: "0 0 0 2px #XXXXXX"
  panel: "0 1px 4px rgba(0,0,0,0.1)"
```

**Commit:** `docs: Luna design tokens - colors, fonts, spacing, corners, shadows`

---

### Task UI-04: 信息架构

**Agent:** `@frontend` (Luna)
**Depends on:** UI-02

**产出:** `docs/design/information-architecture.md`

**必须定义:**

1. 主窗口布局:
   ```
   ┌──────────────────────────────────────────────┐
   │ [连接状态] 设备名称  ●Online      [设置] [?] │  ← 顶部工具栏
   ├────────┬─────────────────────────────────────┤
   │ PLC配置│                                      │
   │ Tag编辑│         工作区                        │
   │ 看板   │    (编辑器/运行画面/趋势/等)          │
   │ 趋势   │                                      │
   │ 报警   │                                      │
   │ 历史   │                                      │
   │ 配方   │                                      │
   │ 日志   │                                      │
   ├────────┴─────────────────────────────────────┤
   │ 状态栏: 采集状态 | Tag数 | 最后更新           │  ← 底部状态栏
   └──────────────────────────────────────────────┘
   ```

2. 导航: 左侧树形/列表导航，带图标和活动态高亮。

3. 每个页面的详细线框:
   - PLC 配置页: 表单布局
   - Tag 编辑页: 表格 + 工具栏
   - 看板编辑器: 左侧工具箱 + 中间画布 + 右侧属性面板
   - 看板运行: 全屏画布 + 顶部页面标签 + 底部状态栏
   - 趋势页: 左侧 Tag 选择 + 右侧图表 + 底部时间控件
   - 报警页: 两标签 (当前/历史) + 表格
   - 历史页: 筛选条件 + 表格 + 导出按钮
   - 配方页: 左侧列表 + 右侧表格 + 操作按钮

**Commit:** `docs: Luna information architecture with wireframes for all pages`

---

### Task UI-05: 交互状态规范

**Agent:** `@frontend` (Luna)
**Depends on:** UI-02

**产出:** `docs/design/interaction-states.md`

**必须覆盖:**

1. 看板编辑模式:
   - 未选中组件: 正常渲染
   - 选中组件: 蓝色边框 + 8 个缩放手柄 + 属性面板激活
   - 多选: 每个选中项独立蓝色边框
   - 拖拽中: 吸附参考线 (绿线)
   - 缩放中: 实时尺寸提示 (宽x高 px)
   - 对齐: 多选右键菜单 → 左对齐/右对齐/上对齐/下对齐/水平居中/垂直居中

2. 看板运行模式:
   - 数值组件: 正常显示值，不可交互
   - 开关: 正常/按下态，点击切换
   - 按钮: 5 种状态的明确视觉区分
   - 数值输入: 双击 → 输入框覆盖 → Enter 确认
   - 趋势小窗: 自动滚动

3. 按钮 5 种状态详细视觉:
   - Normal: 基础色 + 文字
   - Hover: 背景色加深 10%, 光标变为手型
   - Pressed: 背景色加深 20%, 轻微内阴影
   - Disabled: 灰色背景, 灰色文字, 不可点击
   - Waiting: 背景脉冲动画或旋转小图标

4. 点动按钮特殊行为:
   - 按下时视觉保持 Pressed 状态
   - 松开时恢复 Normal
   - 超时 3s 自动恢复并提示

5. 加载和空状态:
   - 连接中: 连接按钮变灰 + 旋转动画
   - 无 Tag: 表格显示 "暂无变量，点击添加"
   - 看板空页: 画布中央显示 "拖入组件开始编辑"
   - 历史无数据: "所选时间范围无数据"
   - 报警无数据: "当前无报警"
   - 配方为空: "点击新建创建第一个配方"

**Commit:** `docs: Luna interaction states - edit/run modes, button states, empty states`

---

### Task UI-06: 组件规格

**Agent:** `@frontend` (Luna)
**Depends on:** UI-02

**产出:** `docs/design/component-spec.md`

**每个 Dashboard 组件必须定义:**
- 默认尺寸 (宽 x 高)
- 最小/最大尺寸限制
- 编辑模式和运行模式的视觉差异
- 配置属性和对应 UI 控件 (属性面板)
- 渲染规则 (paint 伪代码或详细描述)

**组件清单:**

| 组件 | 编辑模式 | 运行模式 | 配置属性 |
|---|---|---|---|
| TextItem | 可编辑文字, 双击进入编辑 | 静态显示 | text, font, color, alignment |
| RectItem | 可拖拽缩放 | 静态显示 | fillColor, borderColor, borderWidth, cornerRadius |
| ImageItem | 可替换图片, 保持比例/拉伸 | 静态显示 | imagePath, fitMode |
| ValueItem | 显示占位值 0.00 | 实时显示 tag 值 | tagId, precision, fontSize, prefix, suffix |
| ValueInputItem | 显示占位输入框 | 双击输入 | tagId, min, max, precision |
| LedItem | 显示灰色 | tag bool=1 绿, 0 红, 离线灰 | tagId, onColor, offColor, label |
| SwitchItem | 不可点击 | 点击切换 emit write | tagId, onValue, offValue, onColor, label |
| ProgressBarItem | 显示 50% | 按 (value-min)/(max-min) 填充 | tagId, min, max, barColor, showValue |
| GaugeItem | 显示 0 | 指针指向当前值 | tagId, min, max, startAngle, spanAngle, tickCount |
| TrendItem | 显示静态曲线 | 实时滚动 | tagId, historySeconds, lineColor, showAxis |
| ButtonItem | 显示静态按钮 | 5 状态交互 | tagId, action, text, fontSize, colors |

**组件工具箱视觉:**
- 垂直列表或网格
- 每项显示图标 + 名称
- 拖拽到画布或双击添加
- 分类: 显示类、控制类、图表类

**Commit:** `docs: Luna component spec with default sizes, properties, and paint rules`

---

### Task UI-07: 设计交付明细

**Agent:** `@frontend` (Luna)
**Depends on:** UI-03, UI-04, UI-05, UI-06

**产出:** `docs/design/handoff.md`

**汇总清单:**
- 所有 coder 需要遵循的文件路径和引用方式
- 各文件与 C++ 源文件的对应关系
- Luna 设计决策的理由 (帮助 coder 理解背后的意图)
- 视觉验收清单: 可逐项勾选的检查项

**Commit:** `docs: Luna design handoff - file mapping and visual acceptance checklist`

---

## Luna 设计审查

Luna 不实现代码，但在以下节点参与审查:

| 节点 | 触发条件 | 审查内容 |
|---|---|---|
| DASH-11 | 所有组件已实现 | 多模态截图审查，输出偏差清单 |
| HARD-07 | Windows 部署就绪 | 最终视觉一致性审查 |
