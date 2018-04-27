/************************************************************************
* file name         : tree_widget_loader.h
* ----------------- : 
* creation time     : 2016/08/18
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- : 
* description       : The file contains declaration of EasyTreeWidgetLoader which aim is
*                   : to load EasyProfiler blocks hierarchy in separate thread.
* ----------------- : 
* change log        : * 2016/08/18 Victor Zarubkin: moved sources from blocks_tree_widget.h/.cpp
*                   :       and renamed Prof* to Easy*.
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

#ifndef EASY__TREE_WIDGET_LOADER__H_
#define EASY__TREE_WIDGET_LOADER__H_

#include <stdlib.h>
#include <vector>
//#include <unordered_set>
#include <thread>
#include <atomic>
#include "easy/reader.h"
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////

class EasyTreeWidgetItem;

#ifndef EASY_TREE_WIDGET__USE_VECTOR
typedef ::std::unordered_map<::profiler::block_index_t, EasyTreeWidgetItem*, ::profiler_gui::do_no_hash<::profiler::block_index_t>::hasher_t> Items;
#else
typedef ::std::vector<EasyTreeWidgetItem*> Items;
#endif

typedef ::std::vector<::std::pair<::profiler::thread_id_t, EasyTreeWidgetItem*> > ThreadedItems;
typedef ::std::unordered_map<::profiler::thread_id_t, EasyTreeWidgetItem*, ::profiler_gui::do_no_hash<::profiler::thread_id_t>::hasher_t> RootsMap;
typedef ::std::unordered_map<::profiler::block_id_t, EasyTreeWidgetItem*, ::profiler_gui::do_no_hash<::profiler::block_index_t>::hasher_t> IdItems;

//////////////////////////////////////////////////////////////////////////

enum EasyTreeMode : uint8_t
{
    EasyTreeMode_Full,
    EasyTreeMode_Plain
};

//////////////////////////////////////////////////////////////////////////

class EasyTreeWidgetLoader Q_DECL_FINAL
{
    ThreadedItems   m_topLevelItems; ///< 
    Items                   m_items; ///< 
    IdItems               m_iditems; ///< 
    ::std::thread          m_thread; ///< 
    ::std::atomic_bool      m_bDone; ///< 
    ::std::atomic_bool m_bInterrupt; ///< 
    ::std::atomic<int>   m_progress; ///< 
    EasyTreeMode             m_mode; ///< 

public:

    EasyTreeWidgetLoader();
    ~EasyTreeWidgetLoader();

    int progress() const;
    bool done() const;

    void takeTopLevelItems(ThreadedItems& _output);
    void takeItems(Items& _output);

    void interrupt(bool _wait = false);
    void fillTree(::profiler::timestamp_t& _beginTime, const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree, bool _colorizeRows, EasyTreeMode _mode);
    void fillTreeBlocks(const::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _beginTime, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, bool _colorizeRows, EasyTreeMode _mode);

private:

    bool interrupted() const;
    void setDone();
    void setProgress(int _progress);

    void setTreeInternal1(::profiler::timestamp_t& _beginTime, const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree, bool _colorizeRows, bool _addZeroBlocks, bool _decoratedThreadNames, ::profiler_gui::TimeUnits _units);
    void setTreeInternal2(const ::profiler::timestamp_t& _beginTime, const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, bool _colorizeRows, bool _addZeroBlocks, bool _decoratedThreadNames, ::profiler_gui::TimeUnits _units);
    size_t setTreeInternal(const ::profiler::BlocksTreeRoot& _threadRoot, ::profiler::block_index_t _firstCswitch, const ::profiler::timestamp_t& _beginTime, const ::profiler::BlocksTree::children_t& _children, EasyTreeWidgetItem* _parent, EasyTreeWidgetItem* _frame, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, ::profiler::timestamp_t& _duration, bool _colorizeRows, bool _addZeroBlocks, ::profiler_gui::TimeUnits _units);
    size_t setTreeInternalPlain(const ::profiler::BlocksTreeRoot& _threadRoot, ::profiler::block_index_t _firstCswitch, const ::profiler::timestamp_t& _beginTime, const ::profiler::BlocksTree::children_t& _children, EasyTreeWidgetItem* _parent, EasyTreeWidgetItem* _frame, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, ::profiler::timestamp_t& _duration, bool _colorizeRows, bool _addZeroBlocks, ::profiler_gui::TimeUnits _units);

    ::profiler::timestamp_t calculateChildrenDurationRecursive(const ::profiler::BlocksTree::children_t& _children, ::profiler::block_id_t _id);

}; // END of class EasyTreeWidgetLoader.

//////////////////////////////////////////////////////////////////////////

#endif // EASY__TREE_WIDGET_LOADER__H_
