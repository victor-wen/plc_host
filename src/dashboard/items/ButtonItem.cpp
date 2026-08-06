#include "dashboard/items/ButtonItem.h"

#include <QColor>
#include <QFont>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QStyleOptionGraphicsItem>

ButtonItem::ButtonItem(QGraphicsItem* parent)
    : DashboardBaseItem(parent)
{
    itemType = QStringLiteral("button");
    // 运行模式接收左键点击触发动作；接受悬停事件以渲染 Hover 态。
    // 编辑模式的移动/选择由基类处理。
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptHoverEvents(true);
}

QRectF ButtonItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

QString ButtonItem::text() const
{
    return m_text;
}

void ButtonItem::setText(const QString& text)
{
    if (m_text == text)
        return;
    m_text = text;
    config.insert(QStringLiteral("text"), text);
    update();
}

int ButtonItem::tagId() const
{
    return m_tagId;
}

void ButtonItem::setTagId(int tagId)
{
    if (m_tagId == tagId)
        return;
    m_tagId = tagId;
    config.insert(QStringLiteral("tagId"), tagId);
}

ButtonAction ButtonItem::action() const
{
    return m_action;
}

void ButtonItem::setAction(const ButtonAction& action)
{
    m_action = action;
    config.insert(QStringLiteral("action"), action.toJson());
    update();
}

void ButtonItem::setButtonEnabled(bool enabled)
{
    if (m_buttonEnabled == enabled)
        return;
    m_buttonEnabled = enabled;
    update();
}

bool ButtonItem::isButtonEnabled() const
{
    return m_buttonEnabled;
}

void ButtonItem::setWaiting(bool waiting)
{
    if (m_waiting == waiting)
        return;
    m_waiting = waiting;
    update();
}

bool ButtonItem::isWaiting() const
{
    return m_waiting;
}

ButtonVisualState ButtonItem::visualState() const
{
    if (!m_buttonEnabled)
        return ButtonVisualState::Disabled;
    if (m_waiting)
        return ButtonVisualState::Waiting;
    if (m_pressed)
        return ButtonVisualState::Pressed;
    if (m_hovered)
        return ButtonVisualState::Hover;
    return ButtonVisualState::Normal;
}

QColor ButtonItem::stateBackgroundColor() const
{
    const char* key = nullptr;
    QColor fallback;
    switch (visualState()) {
    case ButtonVisualState::Normal:
        key = "normalBg";
        fallback = QColor(0x2A, 0x6D, 0xF4);
        break;
    case ButtonVisualState::Hover:
        key = "hoverBg";
        fallback = QColor(0x3D, 0x7B, 0xF5);
        break;
    case ButtonVisualState::Pressed:
        key = "pressedBg";
        fallback = QColor(0x1F, 0x56, 0xD0);
        break;
    case ButtonVisualState::Disabled:
        key = "disabledBg";
        fallback = QColor(0x3A, 0x48, 0x54);
        break;
    case ButtonVisualState::Waiting:
        key = "waitingBg";
        fallback = QColor(0xD9, 0xA2, 0x1B);
        break;
    }
    return QColor(commonStyle.value(QLatin1String(key))
                      .toString(fallback.name()));
}

void ButtonItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
                       QWidget* widget)
{
    Q_UNUSED(option);
    Q_UNUSED(widget);

    const QRectF rect = boundingRect();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 按下态整体下移 1px，模拟物理按压反馈。
    QRectF body = rect;
    if (visualState() == ButtonVisualState::Pressed)
        body.translate(0, 1);

    painter->setBrush(stateBackgroundColor());
    painter->setPen(QPen(QColor(QStringLiteral("#17212B")), 1));
    const qreal radius = commonStyle.value(QStringLiteral("borderRadius")).toDouble(6.0);
    painter->drawRoundedRect(body, radius, radius);

    // 居中按钮文字；字体与颜色来自 commonStyle。
    const qreal fontSize = commonStyle.value(QStringLiteral("fontSize")).toDouble(14.0);
    QFont font;
    font.setPixelSize(qMax(1, qRound(fontSize)));
    painter->setFont(font);
    const QString textColor =
        commonStyle.value(QStringLiteral("textColor")).toString(QStringLiteral("#FFFFFF"));
    painter->setPen(QColor(textColor));
    painter->drawText(body, Qt::AlignCenter, m_text);
}

QJsonObject ButtonItem::serialize() const
{
    QJsonObject cfg = config;
    cfg.insert(QStringLiteral("tagId"), m_tagId);
    cfg.insert(QStringLiteral("text"), m_text);
    cfg.insert(QStringLiteral("action"), m_action.toJson());
    return cfg;
}

void ButtonItem::deserialize(const QJsonObject& cfg)
{
    config = cfg;
    m_tagId = cfg.value(QStringLiteral("tagId")).toInt(-1);
    m_text = cfg.value(QStringLiteral("text")).toString(QStringLiteral("按钮"));
    m_action = ButtonAction::fromJson(cfg.value(QStringLiteral("action")).toObject());
}

void ButtonItem::hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event);
    if (!m_editMode) {
        m_hovered = true;
        update();
    }
}

void ButtonItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
    Q_UNUSED(event);
    if (!m_editMode) {
        m_hovered = false;
        update();
    }
}

void ButtonItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_editMode) {
        // 编辑模式：交给基类（选中/拖动），不改变视觉。
        QGraphicsObject::mousePressEvent(event);
        return;
    }
    event->accept();
    if (!m_buttonEnabled)
        return;
    m_pressed = true;
    update();
}

void ButtonItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    if (m_editMode) {
        QGraphicsObject::mouseReleaseEvent(event);
        return;
    }
    m_pressed = false;
    update();
    if (!m_buttonEnabled)
        return;
    if (event->button() == Qt::LeftButton
        && boundingRect().contains(event->pos())) {
        emit actionTriggered(m_action, m_tagId);
        event->accept();
        return;
    }
    event->ignore();
}
