# UI-03 Luna 设计 Token

> 视觉基线：UI-01 推荐的深浅混合方案。编辑模式使用工业浅色，运行模式使用工业深色；两种模式共用布局几何和逻辑像素。所有颜色均为 HEX，所有尺寸均为 Qt 逻辑 px。

## 1. Token 定义

顶层值是编辑模式的默认值；运行模式只覆盖确实需要改变的颜色，避免组件在模式切换时改变尺寸或状态语义。

```yaml
colors:
  background:
    primary: "#F2F5F7"       # 主窗口背景
    secondary: "#FFFFFF"     # 面板/卡片背景
    editor_canvas: "#F8FAFC" # 编辑器画布（浅色，有网格线）
    runtime_canvas: "#111820" # 运行模式画布（深色）
  text:
    primary: "#17212B"       # 主文字
    secondary: "#526170"     # 次要文字
    disabled: "#8A96A1"       # 禁用文字
    on_dark: "#F1F5F8"        # 深色背景上文字
  border:
    default: "#C5CFD8"
    focus: "#0078D4"          # 输入框聚焦
    hover: "#7F92A3"
  state:
    good: "#1B8E53"           # Good 质量
    stale: "#C4831A"          # Stale 质量
    bad: "#C73E3E"            # Bad 质量
    disconnected: "#6B7280"  # 离线
  alarm:
    critical: "#FF3B30"      # 红色
    warning: "#FF9500"       # 橙色
    info: "#007AFF"          # 蓝色
  button:
    normal_bg: "#1F5A94"
    normal_text: "#FFFFFF"
    hover_bg: "#174A7A"
    pressed_bg: "#103551"
    disabled_bg: "#DEE4E8"
    disabled_text: "#8A96A1"
    waiting_bg: "#C4831A"
  accent: "#0078D4"           # 强调色（导航高亮、选中框）
  grid: "#E2EAF0"             # 编辑器细网格线
  snap_guide: "#00FF00"       # 吸附参考线

fonts:
  family: "Segoe UI"
  fallback: "Microsoft YaHei UI"
  weights:
    regular: 400
    semibold: 600
    bold: 700
  sizes:
    h1: 18px
    h2: 14px
    body: 12px
    small: 10px
    value_display: 20px       # 看板数值显示
    gauge_label: 11px

spacing:
  component_min: "60px x 40px"
  component_min_width: 60px
  component_min_height: 40px
  grid_snap: 10px
  panel_padding: 8px
  item_margin: 4px
  button_min_width: 80px
  button_min_height: 36px
  toolbar_height: 32px
  top_status_bar_height: 44px
  bottom_status_bar_height: 26px
  navigation_width: 224px
  resize_handle_size: 8px

corners:
  button_radius: 4px
  panel_radius: 6px
  item_radius: 4px
  gauge_radius: 50%

shadows:
  item_selected: "0px 0px 0px 2px #0078D4"
  panel: "0px 1px 4px #0000001A"
```

## 2. 运行模式覆盖

运行画布保留 `colors.background.runtime_canvas`；以下覆盖用于运行工作区、导航、面板、状态和按钮。编辑模式不读取这些覆盖值。

```yaml
mode_overrides:
  runtime:
    colors:
      background:
        primary: "#151E26"
        secondary: "#1B242D"
      text:
        primary: "#F1F5F8"
        secondary: "#B4C0CA"
        disabled: "#788691"
      border:
        default: "#3A4854"
        focus: "#5BB9F5"
        hover: "#5B7080"
      state:
        good: "#32C36C"
        stale: "#E5A83B"
        bad: "#F05D5E"
        disconnected: "#7F8B96"
      button:
        normal_bg: "#2E77A8"
        normal_text: "#FFFFFF"
        hover_bg: "#3E8FC4"
        pressed_bg: "#1F587C"
        disabled_bg: "#303B45"
        disabled_text: "#788691"
        waiting_bg: "#C98A24"
      accent: "#5BB9F5"
```

## 3. 使用约束

- `accent` 只表达活动导航、焦点和选中框；`snap_guide` 只表达编辑器几何辅助线，释放鼠标后移除。
- Good、Stale、Bad、Disconnected 和报警都必须同时显示文字或图标，颜色不能作为唯一状态信息。
- `shadows.panel` 仅用于需要层级的弹出面板；常规工业面板保持扁平，运行画布不使用装饰性阴影或发光。
- `shadows.item_selected` 是选中轮廓，不是投影；编辑模式使用，运行模式不显示选中框和缩放手柄。
- 顶部状态栏、底部状态栏、导航宽度和组件命中尺寸在编辑/运行模式保持不变，由 Qt 按 100%/125%/150%/200% DPI 缩放。
- 质量状态使用状态色，报警使用 `alarm` 色；写入按钮在 Bad 或 Disconnected 时使用 disabled token，不得仅通过颜色暗示可写入能力。
