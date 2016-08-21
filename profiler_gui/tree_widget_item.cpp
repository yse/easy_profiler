/************************************************************************
* file name         : tree_widget_item.cpp
* ----------------- :
* creation time     : 2016/08/18
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of EasyTreeWidgetItem.
* ----------------- :
* change log        : * 2016/08/18 Victor Zarubkin: Moved sources from blocks_tree_widget.cpp
*                   :       and renamed classes from Prof* to Easy*.
*                   :
*                   : * 
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#include "tree_widget_item.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////

EasyTreeWidgetItem::EasyTreeWidgetItem(const ::profiler::BlocksTree* _treeBlock, Parent* _parent)
    : Parent(_parent)
    , m_block(_treeBlock)
    , m_customBGColor(0)
    , m_customTextColor(0)
{

}

EasyTreeWidgetItem::~EasyTreeWidgetItem()
{
}

bool EasyTreeWidgetItem::operator < (const Parent& _other) const
{
    const auto col = treeWidget()->sortColumn();

    switch (col)
    {
        //case COL_UNKNOWN:
        case COL_NAME:
        {
            if (parent() == nullptr)
                return false; // Do not sort topLevelItems by name
            return Parent::operator < (_other);
        }

        case COL_NCALLS_PER_THREAD:
        case COL_NCALLS_PER_PARENT:
        case COL_NCALLS_PER_FRAME:
        {
            return data(col, Qt::UserRole).toUInt() < _other.data(col, Qt::UserRole).toUInt();
        }

        case COL_SELF_DURATION_PERCENT:
        case COL_PERCENT_PER_PARENT:
        case COL_PERCENT_PER_FRAME:
        case COL_PERCENT_SUM_PER_PARENT:
        case COL_PERCENT_SUM_PER_FRAME:
        case COL_PERCENT_SUM_PER_THREAD:
        {
            return data(col, Qt::UserRole).toInt() < _other.data(col, Qt::UserRole).toInt();
        }

        default:
        {
            // durations min, max, average
            return data(col, Qt::UserRole).toULongLong() < _other.data(col, Qt::UserRole).toULongLong();
        }
    }

    return false;
}

const ::profiler::BlocksTree* EasyTreeWidgetItem::block() const
{
    return m_block;
}

::profiler::timestamp_t EasyTreeWidgetItem::duration() const
{
    if (m_block->node)
        return m_block->node->block()->duration();
    return data(COL_DURATION, Qt::UserRole).toULongLong();
}

::profiler::timestamp_t EasyTreeWidgetItem::selfDuration() const
{
    return data(COL_SELF_DURATION, Qt::UserRole).toULongLong();
}

void EasyTreeWidgetItem::setTimeSmart(int _column, const ::profiler::timestamp_t& _time, const QString& _prefix)
{
    const ::profiler::timestamp_t nanosecondsTime = PROF_NANOSECONDS(_time);

    setData(_column, Qt::UserRole, (quint64)nanosecondsTime);

    setToolTip(_column, QString("%1 ns").arg(nanosecondsTime));

    if (_time < 1e3)
    {
        setText(_column, QString("%1%2 ns").arg(_prefix).arg(nanosecondsTime));
    }
    else if (_time < 1e6)
    {
        setText(_column, QString("%1%2 us").arg(_prefix).arg(double(nanosecondsTime) * 1e-3, 0, 'f', 3));
    }
    else if (_time < 1e9)
    {
        setText(_column, QString("%1%2 ms").arg(_prefix).arg(double(nanosecondsTime) * 1e-6, 0, 'f', 3));
    }
    else
    {
        setText(_column, QString("%1%2 s").arg(_prefix).arg(double(nanosecondsTime) * 1e-9, 0, 'f', 3));
    }
}

void EasyTreeWidgetItem::setTimeMs(int _column, const ::profiler::timestamp_t& _time)
{
    const ::profiler::timestamp_t nanosecondsTime = PROF_NANOSECONDS(_time);
    setData(_column, Qt::UserRole, (quint64)nanosecondsTime);
    setToolTip(_column, QString("%1 ns").arg(nanosecondsTime));
    setText(_column, QString::number(double(nanosecondsTime) * 1e-6, 'g', 9));
}

void EasyTreeWidgetItem::setTimeMs(int _column, const ::profiler::timestamp_t& _time, const QString& _prefix)
{
    const ::profiler::timestamp_t nanosecondsTime = PROF_NANOSECONDS(_time);
    setData(_column, Qt::UserRole, (quint64)nanosecondsTime);
    setToolTip(_column, QString("%1 ns").arg(nanosecondsTime));
    setText(_column, QString("%1%2").arg(_prefix).arg(double(nanosecondsTime) * 1e-6, 0, 'g', 9));
}

void EasyTreeWidgetItem::setBackgroundColor(QRgb _color)
{
    m_customBGColor = _color;
}

void EasyTreeWidgetItem::setTextColor(QRgb _color)
{
    m_customTextColor = _color;
}

void EasyTreeWidgetItem::colorize(bool _colorize)
{
    if (_colorize)
    {
        for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
        {
            setBackground(i, QColor::fromRgb(m_customBGColor));
            setForeground(i, QColor::fromRgb(m_customTextColor));
        }
    }
    else
    {
        const QBrush nobrush;
        for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
        {
            setBackground(i, nobrush);
            setForeground(i, nobrush);
        }
    }
}

void EasyTreeWidgetItem::collapseAll()
{
    for (int i = 0, childrenNumber = childCount(); i < childrenNumber; ++i)
    {
        static_cast<EasyTreeWidgetItem*>(child(i))->collapseAll();
    }

    setExpanded(false);
    ::profiler_gui::EASY_GLOBALS.gui_blocks[m_block->block_index].expanded = false;
}

void EasyTreeWidgetItem::expandAll()
{
    for (int i = 0, childrenNumber = childCount(); i < childrenNumber; ++i)
    {
        static_cast<EasyTreeWidgetItem*>(child(i))->expandAll();
    }

    setExpanded(true);
    ::profiler_gui::EASY_GLOBALS.gui_blocks[m_block->block_index].expanded = true;
}

//////////////////////////////////////////////////////////////////////////
