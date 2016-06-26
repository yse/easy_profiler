/************************************************************************
* file name         : blocks_tree_widget.h
* ----------------- : 
* creation time     : 2016/06/26
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- : 
* description       : The file contains declaration of TreeWidget and it's auxiliary classes
*                   : for displyaing easy_profiler blocks tree.
* ----------------- : 
* change log        : * 2016/06/26 Victor Zarubkin: moved sources from tree_view.h
*                   : * 
* ----------------- : 
* license           : TODO: add license text
************************************************************************/

#ifndef MY____TREE___VIEW_H
#define MY____TREE___VIEW_H

#include <QTreeWidget>
#include <QAction>
#include <stdlib.h>
#include <unordered_map>
#include "profiler/reader.h"

//////////////////////////////////////////////////////////////////////////

template <const size_t SIZEOF_T>
struct no_hasher {
    template <class T> inline size_t operator () (const T& _data) const {
        return (size_t)_data;
    }
};

#ifdef _WIN64
template <> struct no_hasher<8> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};
#endif

template <> struct no_hasher<4> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};

template <> struct no_hasher<2> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};

template <> struct no_hasher<1> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};

template <class T>
struct do_no_hash {
    typedef no_hasher<sizeof(T)> hasher_t;
};

//////////////////////////////////////////////////////////////////////////

class MyTreeItem : public QTreeWidgetItem
{
    //Q_OBJECT

    const BlocksTree* m_block;

public:

    MyTreeItem(const BlocksTree* _block, QTreeWidgetItem* _parent = nullptr);

    const BlocksTree* block() const;

    inline bool operator < (const QTreeWidgetItem& _other) const
    {
        const auto col = treeWidget()->sortColumn();
        //switch (col)
        //{
        //    case 1:
        //    case 2:
        //    case 3:
            if (col > 0 && col < 7)
            {
#ifndef _DEBUG
                return data(col, Qt::UserRole).toULongLong() < _other.data(col, Qt::UserRole).toULongLong();
#else
                const auto selfdata = data(col, Qt::UserRole).toULongLong();
                const auto otherdata = _other.data(col, Qt::UserRole).toULongLong();
                return selfdata < otherdata;
#endif
            }
        //}

        return QTreeWidgetItem::operator < (_other);
    }

    void setTimeSmart(int _column, const ::profiler::timestamp_t& _time);

    void setTimeMs(int _column, const ::profiler::timestamp_t& _time);

}; // END of class MyTreeItem.

//////////////////////////////////////////////////////////////////////////

class ItemAction : public QAction
{
    Q_OBJECT

private:

    MyTreeItem* m_item;

public:

    ItemAction(const char* _label, MyTreeItem* _item) : QAction(_label, nullptr), m_item(_item)
    {
        connect(this, &QAction::triggered, this, &ItemAction::onToggle);
    }

private:

    void onToggle(bool)
    {
        emit clicked(m_item);
    }

signals:

    void clicked(MyTreeItem* _item);
};

//////////////////////////////////////////////////////////////////////////

class MyTreeWidget : public QTreeWidget
{
    Q_OBJECT

protected:

    typedef ::std::unordered_map<const ::profiler::SerilizedBlock*, MyTreeItem*, do_no_hash<const ::profiler::SerilizedBlock*>::hasher_t> BlockItemMap;

    BlockItemMap            m_itemblocks;
    ::profiler::timestamp_t  m_beginTime;

public:

    MyTreeWidget(const thread_blocks_tree_t& _blocksTree, QWidget* _parent = nullptr);

    void setTree(const thread_blocks_tree_t& _blocksTree);

protected:

    void setTreeInternal(const thread_blocks_tree_t& _blocksTree);

    void setTreeInternal(const BlocksTree::children_t& _children, MyTreeItem* _parent);

    void contextMenuEvent(QContextMenuEvent* _event);

private slots:

    void onJumpToMinItemClicked(MyTreeItem* _item);

    void onJumpToMaxItemClicked(MyTreeItem* _item);

    void onCollapseAllClicked(bool);

    void onExpandAllClicked(bool);

    void onItemExpand(QTreeWidgetItem*);

}; // END of class MyTreeWidget.

//////////////////////////////////////////////////////////////////////////

#endif // MY____TREE___VIEW_H
