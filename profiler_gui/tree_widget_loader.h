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
*                   : This program is free software : you can redistribute it and / or modify
*                   : it under the terms of the GNU General Public License as published by
*                   : the Free Software Foundation, either version 3 of the License, or
*                   : (at your option) any later version.
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
#include <thread>
#include <atomic>
#include "profiler/reader.h"
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////

class EasyTreeWidgetItem;
typedef ::std::vector<EasyTreeWidgetItem*> Items;
typedef ::std::vector<::std::pair<::profiler::thread_id_t, EasyTreeWidgetItem*> > ThreadedItems;
typedef ::std::unordered_map<::profiler::thread_id_t, EasyTreeWidgetItem*, ::profiler_gui::do_no_hash<::profiler::thread_id_t>::hasher_t> RootsMap;

//////////////////////////////////////////////////////////////////////////

class EasyTreeWidgetLoader final
{
    ThreadedItems   m_topLevelItems; ///< 
    Items                   m_items; ///< 
    ::std::thread          m_thread; ///< 
    ::std::atomic_bool      m_bDone; ///< 
    ::std::atomic_bool m_bInterrupt; ///< 
    ::std::atomic<int>   m_progress; ///< 

public:

    EasyTreeWidgetLoader();
    ~EasyTreeWidgetLoader();

    bool interrupted() const;
    int progress() const;
    bool done() const;

    void takeTopLevelItems(ThreadedItems& _output);
    void takeItems(Items& _output);

    void interrupt(bool _wait = false);
    void fillTree(::profiler::timestamp_t& _beginTime, const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree, bool _colorizeRows);
    void fillTreeBlocks(const::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _beginTime, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, bool _colorizeRows);

    void setDone();
    void setProgress(int _progress);

}; // END of class EasyTreeWidgetLoader.

//////////////////////////////////////////////////////////////////////////

template <class T>
struct FillTreeClass final
{
    static void setTreeInternal1(T& _safelocker, Items& _items, ThreadedItems& _topLevelItems, ::profiler::timestamp_t& _beginTime, const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree, bool _colorizeRows);
    static void setTreeInternal2(T& _safelocker, Items& _items, ThreadedItems& _topLevelItems, const ::profiler::timestamp_t& _beginTime, const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, bool _colorizeRows);
    static size_t setTreeInternal(T& _safelocker, Items& _items, const ::profiler::timestamp_t& _beginTime, const ::profiler::BlocksTree::children_t& _children, EasyTreeWidgetItem* _parent, EasyTreeWidgetItem* _frame, EasyTreeWidgetItem* _thread, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, ::profiler::timestamp_t& _duration, bool _colorizeRows);
};

//////////////////////////////////////////////////////////////////////////

struct StubLocker final
{
    void setDone() {}
    bool interrupted() const { return false; }
    void setProgress(int) {}
};

//////////////////////////////////////////////////////////////////////////

#endif // EASY__TREE_WIDGET_LOADER__H_
