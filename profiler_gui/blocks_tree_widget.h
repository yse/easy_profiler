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

class ProfTreeWidgetItem : public QTreeWidgetItem
{
    typedef QTreeWidgetItem    Parent;
    typedef ProfTreeWidgetItem   This;

    const ::profiler::BlocksTree*           m_block;
    QRgb                            m_customBGColor;
    QRgb                          m_customTextColor;

public:

    using Parent::setBackgroundColor;
    using Parent::setTextColor;

    ProfTreeWidgetItem(const ::profiler::BlocksTree* _treeBlock, Parent* _parent = nullptr);
    virtual ~ProfTreeWidgetItem();

    bool operator < (const Parent& _other) const override;

public:

    const ::profiler::BlocksTree* block() const;

    ::profiler::timestamp_t duration() const;
    ::profiler::timestamp_t selfDuration() const;

    void setTimeSmart(int _column, const ::profiler::timestamp_t& _time, const QString& _prefix = "");

    void setTimeMs(int _column, const ::profiler::timestamp_t& _time);
    void setTimeMs(int _column, const ::profiler::timestamp_t& _time, const QString& _prefix);

    void setBackgroundColor(QRgb _color);

    void setTextColor(QRgb _color);

    void colorize(bool _colorize);

    void collapseAll();

    void expandAll();

}; // END of class ProfTreeWidgetItem.

//////////////////////////////////////////////////////////////////////////

#define DECLARE_QACTION(ClassName, DataType) \
class ClassName : public QAction { \
    Q_OBJECT \
private: \
    DataType m_item; \
public: \
    ClassName(const char* _label, DataType _item) : QAction(_label, nullptr), m_item(_item) { \
        connect(this, &QAction::triggered, this, &ClassName::onToggle); } \
    ClassName(const QString& _label, DataType _item) : QAction(_label, nullptr), m_item(_item) { \
        connect(this, &QAction::triggered, this, &ClassName::onToggle); } \
    virtual ~ClassName() {}\
private: \
    void onToggle(bool) { emit clicked(m_item); }

DECLARE_QACTION(ProfItemAction, unsigned int) signals: void clicked(unsigned int _item); };
DECLARE_QACTION(ProfHideShowColumnAction, int) signals: void clicked(int _item); };

#undef DECLARE_QACTION

//////////////////////////////////////////////////////////////////////////

class ProfTreeWidget : public QTreeWidget
{
    Q_OBJECT

    typedef QTreeWidget    Parent;
    typedef ProfTreeWidget   This;

protected:

    typedef ::std::vector<ProfTreeWidgetItem*> Items;
    typedef ::std::unordered_map<::profiler::thread_id_t, ProfTreeWidgetItem*, ::profiler_gui::do_no_hash<::profiler::thread_id_t>::hasher_t> RootsMap;

    Items                        m_items;
    RootsMap                     m_roots;
    ::profiler::timestamp_t  m_beginTime;
    bool                    m_bColorRows;

public:

    ProfTreeWidget(QWidget* _parent = nullptr);
    ProfTreeWidget(const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree, QWidget* _parent = nullptr);
    virtual ~ProfTreeWidget();

    void clearSilent(bool _global = false);

public slots:

    void setTree(const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree);

    void setTreeBlocks(const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _session_begin_time, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict);

protected:

    size_t setTreeInternal(const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree);

    size_t setTreeInternal(const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict);

    size_t setTreeInternal(const ::profiler::BlocksTree::children_t& _children, ProfTreeWidgetItem* _parent, ProfTreeWidgetItem* _frame, ProfTreeWidgetItem* _thread, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, ::profiler::timestamp_t& _duration);

    void contextMenuEvent(QContextMenuEvent* _event) override;

private slots:

    void onJumpToItemClicked(unsigned int _block_index);

    void onCollapseAllClicked(bool);

    void onExpandAllClicked(bool);

    void onCollapseAllChildrenClicked(bool);

    void onExpandAllChildrenClicked(bool);

    void onItemExpand(QTreeWidgetItem*);

    void onColorizeRowsTriggered(bool _colorize);

    void onSelectedThreadChange(::profiler::thread_id_t _id);

    void onSelectedBlockChange(unsigned int _block_index);

    void resizeColumnsToContents();

    void onHideShowColumn(int _column);

protected:

    void loadSettings();
	void saveSettings();

}; // END of class ProfTreeWidget.

//////////////////////////////////////////////////////////////////////////

#endif // MY____TREE___VIEW_H
