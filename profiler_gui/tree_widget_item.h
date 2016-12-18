/************************************************************************
* file name         : tree_widget_item.h
* ----------------- : 
* creation time     : 2016/08/18
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

#ifndef EASY__TREE_WIDGET_ITEM__H_
#define EASY__TREE_WIDGET_ITEM__H_

#include <stdlib.h>
#include <QTreeWidget>
#include "easy/reader.h"
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

    COL_ACTIVE_TIME,
    COL_ACTIVE_PERCENT,

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

    explicit EasyTreeWidgetItem(const ::profiler::block_index_t _treeBlock = ::profiler_gui::numeric_max<decltype(m_block)>(), Parent* _parent = nullptr);
    virtual ~EasyTreeWidgetItem();

    bool operator < (const Parent& _other) const override;

public:

    ::profiler::block_index_t block_index() const;
    ::profiler_gui::EasyBlock& guiBlock();
    const ::profiler::BlocksTree& block() const;

    ::profiler::timestamp_t duration() const;
    ::profiler::timestamp_t selfDuration() const;

    void setTimeSmart(int _column, ::profiler_gui::TimeUnits _units, const ::profiler::timestamp_t& _time, const QString& _prefix);
    void setTimeSmart(int _column, ::profiler_gui::TimeUnits _units, const ::profiler::timestamp_t& _time);

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
