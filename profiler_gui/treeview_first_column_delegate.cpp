

#include <QPainter>
#include <QPoint>
#include "treeview_first_column_delegate.h"
#include "globals.h"

EasyTreeViewFirstColumnItemDelegate::EasyTreeViewFirstColumnItemDelegate(QObject* parent) : QStyledItemDelegate(parent)
{

}

EasyTreeViewFirstColumnItemDelegate::~EasyTreeViewFirstColumnItemDelegate()
{

}

void EasyTreeViewFirstColumnItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // Draw item as usual
    QStyledItemDelegate::paint(painter, option, index);

    // Draw line under tree indicator
    const auto bottomLeft = option.rect.bottomLeft();
    if (bottomLeft.x() > 0)
    {
        painter->save();
        painter->setBrush(Qt::NoBrush);
        painter->setPen(::profiler_gui::SYSTEM_BORDER_COLOR);
        painter->drawLine(QPoint(0, bottomLeft.y()), bottomLeft);
        painter->restore();
    }
}