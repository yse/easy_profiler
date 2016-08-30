/************************************************************************
* file name         : tree_widget_item.h
* ----------------- : 
* creation time     : 2016/08/18
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- : 
* description       : The file contains declaration of EasyTreeWidgetItem
*                   : for displyaing EasyProfiler blocks tree.
* ----------------- : 
* change log        : * 2016/08/18 Victor Zarubkin: moved sources from blocks_tree_widget.h
*                   :       and renamed classes from Prof* to Easy*.
*                   :
*                   : * 
* ----------------- : 
* license           : TODO: add license text
************************************************************************/

#ifndef EASY__TREE_WIDGET_ITEM__H_
#define EASY__TREE_WIDGET_ITEM__H_

#include <stdlib.h>
#include <QTreeWidget>
#include "profiler/reader.h"
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////

enum EasyColumnsIndexes
{
    COL_UNKNOWN = -1,

    COL_NAME = 0,

    COL_BEGIN,

    COL_DURATION,
    COL_SELF_DURATION,
    COL_DURATION_SUM_PER_PARENT,
    COL_DURATION_SUM_PER_FRAME,
    COL_DURATION_SUM_PER_THREAD,

    COL_SELF_DURATION_PERCENT,
    COL_PERCENT_PER_PARENT,
    COL_PERCENT_PER_FRAME,
    COL_PERCENT_SUM_PER_PARENT,
    COL_PERCENT_SUM_PER_FRAME,
    COL_PERCENT_SUM_PER_THREAD,

    COL_END,

    COL_MIN_PER_FRAME,
    COL_MAX_PER_FRAME,
    COL_AVERAGE_PER_FRAME,
    COL_NCALLS_PER_FRAME,

    COL_MIN_PER_THREAD,
    COL_MAX_PER_THREAD,
    COL_AVERAGE_PER_THREAD,
    COL_NCALLS_PER_THREAD,

    COL_MIN_PER_PARENT,
    COL_MAX_PER_PARENT,
    COL_AVERAGE_PER_PARENT,
    COL_NCALLS_PER_PARENT,

    COL_COLUMNS_NUMBER
};

//////////////////////////////////////////////////////////////////////////

class EasyTreeWidgetItem : public QTreeWidgetItem
{
    typedef QTreeWidgetItem    Parent;
    typedef EasyTreeWidgetItem   This;

    const ::profiler::block_index_t         m_block;
    QRgb                            m_customBGColor;
    QRgb                          m_customTextColor;

public:

    using Parent::setBackgroundColor;
    using Parent::setTextColor;

    EasyTreeWidgetItem(const ::profiler::block_index_t _treeBlock = ::profiler_gui::numeric_max<decltype(m_block)>(), Parent* _parent = nullptr);
    virtual ~EasyTreeWidgetItem();

    bool operator < (const Parent& _other) const override;

public:

    ::profiler::block_index_t block_index() const;
    ::profiler_gui::EasyBlock& guiBlock();
    const ::profiler::BlocksTree& block() const;

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

}; // END of class EasyTreeWidgetItem.

//////////////////////////////////////////////////////////////////////////

#endif // EASY__TREE_WIDGET_ITEM__H_
