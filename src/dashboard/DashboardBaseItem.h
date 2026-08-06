#pragma once

#include <QGraphicsObject>
#include <QJsonObject>
#include <QString>

// 所有看板组件的 QGraphicsObject 基类 (docs/architecture/interfaces.md §9, Phase 2 DASH-02)
// 持有组件的元数据（itemId/itemType/commonStyle/config/schemaVersion）以及编辑/运行
// 模式状态：编辑模式可移动/可选中，运行模式锁定。位置由 setPos() 表示左上角，
// 尺寸由 boundingRect() 提供（DASH-03 缩放手柄会修改 m_width/m_height）。
class DashboardBaseItem : public QGraphicsObject {
    Q_OBJECT
public:
    explicit DashboardBaseItem(QGraphicsItem* parent = nullptr);

    int itemId = -1;
    QString itemType;
    QJsonObject commonStyle;
    QJsonObject config;
    int schemaVersion = 1;

    // 切换编辑/运行模式。编辑模式下启用 ItemIsMovable/ItemIsSelectable；
    // 运行模式下两者强制关闭。
    virtual void setEditMode(bool editing);
    bool isEditMode() const;

    QRectF boundingRect() const override;

    // 修改组件尺寸（内部先 prepareGeometryChange）。供场景工厂与 DASH-03
    // 缩放手柄使用。
    void setSize(qreal width, qreal height);

protected:
    bool m_editMode = true;

    // 组件尺寸（项坐标，左上角 = (0,0)，位置由 setPos 决定）。
    qreal m_width = 100;
    qreal m_height = 100;

    // 运行模式防御：外部误设交互 flags 时强制剥离可移动/可选中。
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
};

// ---- inline 实现（组件基类为纯接口性质，见 DASH-05+ 具体组件） ----

inline DashboardBaseItem::DashboardBaseItem(QGraphicsItem* parent)
    : QGraphicsObject(parent)
{
    // 默认处于编辑模式：可移动、可选中；几何变化通知用于网格吸附等（DASH-03）。
    setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
}

inline void DashboardBaseItem::setEditMode(bool editing)
{
    if (m_editMode == editing)
        return;
    m_editMode = editing;
    setFlag(ItemIsMovable, editing);
    setFlag(ItemIsSelectable, editing);
}

inline bool DashboardBaseItem::isEditMode() const
{
    return m_editMode;
}

inline QRectF DashboardBaseItem::boundingRect() const
{
    return QRectF(0, 0, m_width, m_height);
}

inline void DashboardBaseItem::setSize(qreal width, qreal height)
{
    prepareGeometryChange();
    m_width = width;
    m_height = height;
}

inline QVariant DashboardBaseItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemFlagsChange && !m_editMode) {
        // 运行模式下强制禁止交互，防止外部误调 setFlags 破坏锁定。
        const Qt::ItemFlags flags = value.value<Qt::ItemFlags>();
        return QVariant::fromValue(flags & ~(ItemIsMovable | ItemIsSelectable));
    }
    return QGraphicsObject::itemChange(change, value);
}
