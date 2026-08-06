#pragma once

#include <QColor>
#include <QJsonObject>
#include <QString>
#include <QVariant>

#include "dashboard/DashboardBaseItem.h"
#include "dashboard/runtime/ButtonAction.h"

// 按钮组件 (Task DASH-07)：paint 渲染按钮矩形，运行模式下支持五种视觉
// 状态并在点击释放时触发 ButtonAction。
//
// 业务属性在 config：
//   config["tagId"]   - 绑定的变量 id
//   config["text"]    - 按钮文字（默认 "按钮"）
//   config["action"]  - ButtonAction 序列化（type/paramA/paramB/...）
// 表现层属性全部在 commonStyle：
//   normalBg / hoverBg / pressedBg / disabledBg / waitingBg - 各状态背景色
//   textColor / fontSize / borderRadius
//
// 编辑模式：只渲染，不接受悬停/点击（交互交给基类选中/拖动）。
// 运行模式：hover 进入/离开切换 Hover 态；按下切 Pressed 态；释放时执行
// 动作并 emit actionTriggered(action, tagId)。Disabled/Waiting 态由外部
// 注入（setButtonEnabled / setWaiting），如 DASH-08 依据连接质量/点动进行
// 中设置，不随鼠标事件变化。
enum class ButtonVisualState : int {
    Normal = 0,
    Hover = 1,
    Pressed = 2,
    Disabled = 3,
    Waiting = 4
};

class ButtonItem : public DashboardBaseItem {
    Q_OBJECT
public:
    explicit ButtonItem(QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 业务属性便捷访问（setter 同步写入 config）。
    QString text() const;
    void setText(const QString& text);
    int tagId() const;
    void setTagId(int tagId);
    ButtonAction action() const;
    void setAction(const ButtonAction& action);

    // 运行态注入：Disabled（外部按可用性判定注入）与 Waiting（点动进行中等）。
    void setButtonEnabled(bool enabled);
    bool isButtonEnabled() const;
    void setWaiting(bool waiting);
    bool isWaiting() const;

    // 当前视觉状态（测试与调试用）。
    ButtonVisualState visualState() const;

    QJsonObject serialize() const override;
    void deserialize(const QJsonObject& config) override;

signals:
    // 运行模式点击释放后发出：动作与绑定的 tagId（DASH-08 executor 消费）。
    void actionTriggered(const ButtonAction& action, int tagId);

protected:
    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override;
    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

private:
    // 依据当前视觉状态取背景色（commonStyle 键 + 默认值）。
    QColor stateBackgroundColor() const;

    QString m_text = QStringLiteral("按钮");
    int m_tagId = -1;
    ButtonAction m_action;

    // 运行态视觉标志（不持久化）。
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_buttonEnabled = true;
    bool m_waiting = false;
};
