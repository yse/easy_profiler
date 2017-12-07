/************************************************************************
* file name         : tree_widget_item.cpp
* ----------------- :
* creation time     : 2016/08/18
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
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2017  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
*                   :
*                   : The MIT License
*                   :
*                   : Permission is hereby granted, free of charge, to any person obtaining a copy
*                   : of this software and associated documentation files (the "Software"), to deal
*                   : in the Software without restriction, including without limitation the rights
*                   : to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*                   : of the Software, and to permit persons to whom the Software is furnished
*                   : to do so, subject to the following conditions:
*                   :
*                   : The above copyright notice and this permission notice shall be included in all
*                   : copies or substantial portions of the Software.
*                   :
*                   : THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
*                   : INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
*                   : PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
*                   : LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*                   : TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
*                   : USE OR OTHER DEALINGS IN THE SOFTWARE.
*                   :
*                   : The Apache License, Version 2.0 (the "License")
*                   :
*                   : You may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
************************************************************************/

#include "tree_widget_item.h"
#include "globals.h"
#include <QPainter>
#include <QPoint>
#include <QBrush>
#include <QRect>
#include <QSize>
#include <QVariant>

//////////////////////////////////////////////////////////////////////////

EASY_CONSTEXPR int BlockColorRole = Qt::UserRole + 1;

//////////////////////////////////////////////////////////////////////////

EASY_CONSTEXPR int ColumnBit[COL_COLUMNS_NUMBER] = {
      -1 //    COL_NAME = 0,

    ,  0 //    COL_BEGIN,

    ,  1 //    COL_DURATION,
    ,  2 //    COL_SELF_DURATION,
    ,  3 //    COL_DURATION_SUM_PER_PARENT,
    ,  4 //    COL_DURATION_SUM_PER_FRAME,
    ,  5 //    COL_DURATION_SUM_PER_THREAD,

    , -1 //    COL_SELF_DURATION_PERCENT,
    , -1 //    COL_PERCENT_PER_PARENT,
    , -1 //    COL_PERCENT_PER_FRAME,
    , -1 //    COL_PERCENT_SUM_PER_PARENT,
    , -1 //    COL_PERCENT_SUM_PER_FRAME,
    , -1 //    COL_PERCENT_SUM_PER_THREAD,

    ,  6 //    COL_END,

    ,  7 //    COL_MIN_PER_FRAME,
    ,  8 //    COL_MAX_PER_FRAME,
    ,  9 //    COL_AVERAGE_PER_FRAME,
    , -1 //    COL_NCALLS_PER_FRAME,

    , 10 //    COL_MIN_PER_THREAD,
    , 11 //    COL_MAX_PER_THREAD,
    , 12 //    COL_AVERAGE_PER_THREAD,
    , -1 //    COL_NCALLS_PER_THREAD,

    , 13 //    COL_MIN_PER_PARENT,
    , 14 //    COL_MAX_PER_PARENT,
    , 15 //    COL_AVERAGE_PER_PARENT,
    , -1 //    COL_NCALLS_PER_PARENT,

    , 16 //    COL_ACTIVE_TIME,
    , -1 //    COL_ACTIVE_PERCENT,
};

//////////////////////////////////////////////////////////////////////////

EasyTreeWidgetItem::EasyTreeWidgetItem(const ::profiler::block_index_t _treeBlock, Parent* _parent)
    : Parent(_parent, QTreeWidgetItem::UserType)
    , m_block(_treeBlock)
    , m_customBGColor(0)
    , m_bMain(false)
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

        case COL_ACTIVE_PERCENT:
        {
            return data(col, Qt::UserRole).toDouble() < _other.data(col, Qt::UserRole).toDouble();
        }

        default:
        {
            // durations min, max, average
            return data(col, Qt::UserRole).toULongLong() < _other.data(col, Qt::UserRole).toULongLong();
        }
    }

    return false;
}

bool EasyTreeWidgetItem::hasToolTip(int _column) const
{
    const int bit = ColumnBit[_column];
    return bit < 0 ? false : m_bHasToolTip.test(static_cast<size_t>(bit));
}

void EasyTreeWidgetItem::setHasToolTip(int _column)
{
    const int bit = ColumnBit[_column];
    if (bit >= 0)
        m_bHasToolTip.set(static_cast<size_t>(bit), true);
}

QVariant EasyTreeWidgetItem::data(int _column, int _role) const
{
    if (_column == COL_NAME)
    {
        if (_role == Qt::SizeHintRole)
        {
#ifdef _WIN32
            const float k = m_font.bold() ? 1.2f : 1.f;
#else
            const float k = m_font.bold() ? 1.15f : 1.f;
#endif
            return QSize(static_cast<int>(QFontMetrics(m_font).width(text(COL_NAME)) * k) + 20, 26);
        }

        if (_role == BlockColorRole)
        {
            if (parent() != nullptr || m_bMain)
                return QBrush(QColor::fromRgba(m_customBGColor));
            return QVariant();
        }
    }

    switch (_role)
    {
        case Qt::FontRole:
            return m_font;

        case Qt::ForegroundRole:
            return m_bMain ? QVariant::fromValue(QColor::fromRgb(::profiler_gui::SELECTED_THREAD_FOREGROUND)) : QVariant();
        
        case Qt::ToolTipRole:
            return hasToolTip(_column) ?
                   QVariant::fromValue(QString("%1 ns").arg(QTreeWidgetItem::data(_column, Qt::UserRole).toULongLong())) :
                   QVariant();

        default:
            return QTreeWidgetItem::data(_column, _role);
    }
}

::profiler::block_index_t EasyTreeWidgetItem::block_index() const
{
    return m_block;
}

::profiler_gui::EasyBlock& EasyTreeWidgetItem::guiBlock()
{
    return easyBlock(m_block);
}

const ::profiler::BlocksTree& EasyTreeWidgetItem::block() const
{
    return easyBlocksTree(m_block);
}

::profiler::timestamp_t EasyTreeWidgetItem::duration() const
{
    if (parent() != nullptr)
        return block().node->duration();
    return data(COL_DURATION, Qt::UserRole).toULongLong();
}

::profiler::timestamp_t EasyTreeWidgetItem::selfDuration() const
{
    return data(COL_SELF_DURATION, Qt::UserRole).toULongLong();
}

void EasyTreeWidgetItem::setTimeSmart(int _column, ::profiler_gui::TimeUnits _units, const ::profiler::timestamp_t& _time, const QString& _prefix)
{
    const ::profiler::timestamp_t nanosecondsTime = PROF_NANOSECONDS(_time);

    setData(_column, Qt::UserRole, (quint64)nanosecondsTime);
    setHasToolTip(_column);
    setText(_column, QString("%1%2").arg(_prefix).arg(::profiler_gui::timeStringRealNs(_units, nanosecondsTime, 3)));

//     if (_time < 1e3)
//     {
//         setText(_column, QString("%1%2 ns").arg(_prefix).arg(nanosecondsTime));
//     }
//     else if (_time < 1e6)
//     {
//         setText(_column, QString("%1%2 us").arg(_prefix).arg(double(nanosecondsTime) * 1e-3, 0, 'f', 3));
//     }
//     else if (_time < 1e9)
//     {
//         setText(_column, QString("%1%2 ms").arg(_prefix).arg(double(nanosecondsTime) * 1e-6, 0, 'f', 3));
//     }
//     else
//     {
//         setText(_column, QString("%1%2 s").arg(_prefix).arg(double(nanosecondsTime) * 1e-9, 0, 'f', 3));
//     }
}

void EasyTreeWidgetItem::setTimeSmart(int _column, ::profiler_gui::TimeUnits _units, const ::profiler::timestamp_t& _time)
{
    const ::profiler::timestamp_t nanosecondsTime = PROF_NANOSECONDS(_time);

    setData(_column, Qt::UserRole, (quint64)nanosecondsTime);
    setHasToolTip(_column);
    setText(_column, ::profiler_gui::timeStringRealNs(_units, nanosecondsTime, 3));
}

void EasyTreeWidgetItem::setTimeMs(int _column, const ::profiler::timestamp_t& _time)
{
    const ::profiler::timestamp_t nanosecondsTime = PROF_NANOSECONDS(_time);
    setData(_column, Qt::UserRole, (quint64)nanosecondsTime);
    setHasToolTip(_column);
    setText(_column, QString::number(double(nanosecondsTime) * 1e-6, 'g', 9));
}

void EasyTreeWidgetItem::setTimeMs(int _column, const ::profiler::timestamp_t& _time, const QString& _prefix)
{
    const ::profiler::timestamp_t nanosecondsTime = PROF_NANOSECONDS(_time);
    setData(_column, Qt::UserRole, (quint64)nanosecondsTime);
    setHasToolTip(_column);
    setText(_column, QString("%1%2").arg(_prefix).arg(double(nanosecondsTime) * 1e-6, 0, 'g', 9));
}

void EasyTreeWidgetItem::setBackgroundColor(QRgb _color)
{
    m_customBGColor = _color;
}

void EasyTreeWidgetItem::setMain(bool _main)
{
    m_bMain = _main;
}

void EasyTreeWidgetItem::collapseAll()
{
    for (int i = 0, childrenNumber = childCount(); i < childrenNumber; ++i)
    {
        static_cast<EasyTreeWidgetItem*>(child(i))->collapseAll();
    }

    setExpanded(false);
    if (parent() != nullptr)
        guiBlock().expanded = false;
}

void EasyTreeWidgetItem::expandAll()
{
    for (int i = 0, childrenNumber = childCount(); i < childrenNumber; ++i)
    {
        static_cast<EasyTreeWidgetItem*>(child(i))->expandAll();
    }

    setExpanded(true);
    if (parent() != nullptr)
        guiBlock().expanded = true;
}

void EasyTreeWidgetItem::setBold(bool _bold)
{
    m_font.setBold(_bold);
}

//////////////////////////////////////////////////////////////////////////

EasyItemDelegate::EasyItemDelegate(QTreeWidget* parent) : QStyledItemDelegate(parent), m_treeWidget(parent)
{

}

EasyItemDelegate::~EasyItemDelegate()
{

}

void EasyItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    auto brushData = m_treeWidget->model()->data(index, BlockColorRole);
    if (brushData.isNull())
    {
#ifdef _WIN32
        const auto currentTreeIndex = m_treeWidget->currentIndex();
        if (index.parent() == currentTreeIndex.parent() && index.row() == currentTreeIndex.row())
        {
            // Draw selection background for selected row
            painter->save();
            painter->setBrush(QColor::fromRgba(0xCC98DE98));
            painter->setPen(Qt::NoPen);
            painter->drawRect(QRect(0, option.rect.top(), option.rect.left() + 16, option.rect.height()));
            painter->restore();
        }
#endif

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

        return;
    }

    const auto currentTreeIndex = m_treeWidget->currentIndex();
    if (index.parent() == currentTreeIndex.parent() && index.row() == currentTreeIndex.row())
    {
        // Draw selection background for selected row

        painter->save();

        painter->setBrush(QColor::fromRgba(0xCC98DE98));
        painter->setPen(Qt::NoPen);

#ifdef _WIN32
        painter->drawRect(QRect(0, option.rect.top(), option.rect.left() + 16, option.rect.height()));
#else
        painter->drawRect(QRect(option.rect.left(), option.rect.top(), 16, option.rect.height()));
#endif

        painter->restore();
    }

    // Adjust rect size for drawing color marker
    QStyleOptionViewItem opt = option;
    opt.rect.adjust(16, 0, 0, 0);

    // Draw item as usual
    QStyledItemDelegate::paint(painter, opt, index);

    painter->save();

    // Draw color marker with block color
    const auto brush = m_treeWidget->model()->data(index, Qt::UserRole + 1).value<QBrush>();
    painter->setBrush(brush);
    painter->setPen(::profiler_gui::SYSTEM_BORDER_COLOR);
    painter->drawRect(QRect(option.rect.left(), option.rect.top() + 5, 16, option.rect.height() - 10));

    // Draw line under tree indicator
    const auto bottomLeft = opt.rect.bottomLeft();
    if (bottomLeft.x() > 0)
    {
        painter->setBrush(Qt::NoBrush);
        painter->drawLine(QPoint(0, bottomLeft.y()), bottomLeft);
    }

    painter->restore();
}
