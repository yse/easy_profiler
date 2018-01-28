




#ifndef EASY_PROFILER_GUI_TREEVIEW_FIRST_COLUMN_DELEGATE_H
#define EASY_PROFILER_GUI_TREEVIEW_FIRST_COLUMN_DELEGATE_H

#include <QStyledItemDelegate>

class TreeViewFirstColumnItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:

    explicit TreeViewFirstColumnItemDelegate(QObject* parent = nullptr);
    ~TreeViewFirstColumnItemDelegate() override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;

}; // END of class TreeViewFirstColumnItemDelegate.

#endif // EASY_PROFILER_GUI_TREEVIEW_FIRST_COLUMN_DELEGATE_H
