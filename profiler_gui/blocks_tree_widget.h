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
*                   :       and renamed classes from My* to Prof*.
*                   :
*                   : * 2016/06/27 Victor Zarubkin: Added possibility to colorize rows
*                   :       with profiler blocks' colors.
*                   :
*                   : * 2016/06/29 Victor Zarubkin: Added clearSilent() method.
*                   :
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
#include <vector>
#include "profiler/reader.h"
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////

namespace btw {

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

} // END of namespace btw.

//////////////////////////////////////////////////////////////////////////

class ProfTreeWidgetItem : public QTreeWidgetItem
{
    typedef QTreeWidgetItem    Parent;
    typedef ProfTreeWidgetItem   This;

    const BlocksTree* m_block;
    QBrush    m_customBGColor;
    QBrush  m_customTextColor;

public:

    using Parent::setBackgroundColor;
    using Parent::setTextColor;

    ProfTreeWidgetItem(const BlocksTree* _treeBlock, Parent* _parent = nullptr);
    virtual ~ProfTreeWidgetItem();

    bool operator < (const Parent& _other) const override;

    const BlocksTree* block() const;

    void setTimeSmart(int _column, const ::profiler::timestamp_t& _time);

    void setTimeMs(int _column, const ::profiler::timestamp_t& _time);

    void setBackgroundColor(const QColor& _color);

    void setTextColor(const QColor& _color);

    void colorize(bool _colorize);

    void collapseAll();

    void expandAll();

}; // END of class ProfTreeWidgetItem.

//////////////////////////////////////////////////////////////////////////

class ProfItemAction : public QAction
{
    Q_OBJECT

private:

    ProfTreeWidgetItem* m_item;

public:

    ProfItemAction(const char* _label, ProfTreeWidgetItem* _item) : QAction(_label, nullptr), m_item(_item)
    {
        connect(this, &QAction::triggered, this, &ProfItemAction::onToggle);
    }

    virtual ~ProfItemAction()
    {
    }

private:

    void onToggle(bool)
    {
        emit clicked(m_item);
    }

signals:

    void clicked(ProfTreeWidgetItem* _item);
};

//////////////////////////////////////////////////////////////////////////

class ProfTreeWidget : public QTreeWidget
{
    Q_OBJECT

    typedef QTreeWidget    Parent;
    typedef ProfTreeWidget   This;

protected:

    typedef ::std::vector<ProfTreeWidgetItem*> Items;
    typedef ::std::unordered_map<const ::profiler::SerilizedBlock*, ProfTreeWidgetItem*, ::btw::do_no_hash<const ::profiler::SerilizedBlock*>::hasher_t> BlockItemMap;

    Items                        m_items;
    BlockItemMap            m_itemblocks;
    ::profiler::timestamp_t  m_beginTime;
    bool                    m_bColorRows;

public:

    ProfTreeWidget(QWidget* _parent = nullptr);
    ProfTreeWidget(const unsigned int _blocksNumber, const thread_blocks_tree_t& _blocksTree, QWidget* _parent = nullptr);
    virtual ~ProfTreeWidget();

    void clearSilent();

public slots:

    void setTree(const unsigned int _blocksNumber, const thread_blocks_tree_t& _blocksTree);

    void setTreeBlocks(const TreeBlocks& _blocks, ::profiler::timestamp_t _begin_time);

protected:

    void setTreeInternal(const unsigned int _blocksNumber, const thread_blocks_tree_t& _blocksTree);

    void setTreeInternal(const TreeBlocks& _blocks);

    void setTreeInternal(const BlocksTree::children_t& _children, ProfTreeWidgetItem* _parent);

    void contextMenuEvent(QContextMenuEvent* _event) override;

private slots:

    void onJumpToMinItemClicked(ProfTreeWidgetItem* _item);

    void onJumpToMaxItemClicked(ProfTreeWidgetItem* _item);

    void onCollapseAllClicked(bool);

    void onExpandAllClicked(bool);

    void onCollapseAllChildrenClicked(bool);

    void onExpandAllChildrenClicked(bool);

    void onItemExpand(QTreeWidgetItem*);

    void onColorizeRowsTriggered(bool _colorize);

}; // END of class ProfTreeWidget.

//////////////////////////////////////////////////////////////////////////

#endif // MY____TREE___VIEW_H
