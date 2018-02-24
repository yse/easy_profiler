

#include <QPainter>
#include <QPoint>
#include <QTreeWidget>
#include "treeview_first_column_delegate.h"
#include "globals.h"

TreeViewFirstColumnItemDelegate::TreeViewFirstColumnItemDelegate(QTreeWidget* parent) : QStyledItemDelegate(parent)
{

}

TreeViewFirstColumnItemDelegate::~TreeViewFirstColumnItemDelegate()
{

}

void TreeViewFirstColumnItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // Draw item as usual
    QStyledItemDelegate::paint(painter, option, index);

    // Draw line under tree indicator
    const auto bottomLeft = option.rect.bottomLeft();
    if (bottomLeft.x() > 0)
    {
        painter->save();

        if (static_cast<const QTreeWidget*>(parent())->currentIndex() == index)
        {
            // Draw selection background for current item
            painter->setBrush(QColor::fromRgba(0xCC98DE98));
            painter->setPen(Qt::NoPen);
            painter->drawRect(QRectF(0, option.rect.top(), bottomLeft.x(), option.rect.height()));
        }

        painter->setBrush(Qt::NoBrush);
        painter->setPen(::profiler_gui::SYSTEM_BORDER_COLOR);
        painter->drawLine(QPoint(0, bottomLeft.y()), bottomLeft);

        painter->restore();
    }
}