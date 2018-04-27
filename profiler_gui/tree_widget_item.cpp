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
*                   : Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   :
*                   : Licensed under the Apache License, Version 2.0 (the "License");
*                   : you may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
*                   :
*                   :
*                   : GNU General Public License Usage
*                   : Alternatively, this file may be used under the terms of the GNU
*                   : General Public License as published by the Free Software Foundation,
*                   : either version 3 of the License, or (at your option) any later version.
*                   :
*                   : This program is distributed in the hope that it will be useful,
*                   : but WITHOUT ANY WARRANTY; without even the implied warranty of
*                   : MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*                   : GNU General Public License for more details.
*                   :
*                   : You should have received a copy of the GNU General Public License
*                   : along with this program.If not, see <http://www.gnu.org/licenses/>.
************************************************************************/

#include "tree_widget_item.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////

EasyTreeWidgetItem::EasyTreeWidgetItem(const ::profiler::block_index_t _treeBlock, Parent* _parent)
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
    return blocksTree(m_block);
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
    setToolTip(_column, QString("%1 ns").arg(nanosecondsTime));
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
    setToolTip(_column, QString("%1 ns").arg(nanosecondsTime));
    setText(_column, ::profiler_gui::timeStringRealNs(_units, nanosecondsTime, 3));
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

//////////////////////////////////////////////////////////////////////////
