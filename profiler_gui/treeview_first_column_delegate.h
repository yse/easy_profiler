




#ifndef EASY_PROFILER_GUI_TREEVIEW_FIRST_COLUMN_DELEGATE_H
#define EASY_PROFILER_GUI_TREEVIEW_FIRST_COLUMN_DELEGATE_H

#include <QStyledItemDelegate>

class EasyTreeViewFirstColumnItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:

    explicit EasyTreeViewFirstColumnItemDelegate(QObject* parent = nullptr);
    ~EasyTreeViewFirstColumnItemDelegate() override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

}; // END of class EasyTreeViewFirstColumnItemDelegate.

#endif // EASY_PROFILER_GUI_TREEVIEW_FIRST_COLUMN_DELEGATE_H
