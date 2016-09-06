/************************************************************************
* file name         : tree_widget_loader.h
* ----------------- :
* creation time     : 2016/08/18
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of EasyTreeWidgetLoader which aim is
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

#include "tree_widget_loader.h"
#include "tree_widget_item.h"
#include "globals.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//////////////////////////////////////////////////////////////////////////

EasyTreeWidgetLoader::EasyTreeWidgetLoader() : m_bDone(ATOMIC_VAR_INIT(false)), m_bInterrupt(ATOMIC_VAR_INIT(false)), m_progress(ATOMIC_VAR_INIT(0))
{
}

EasyTreeWidgetLoader::~EasyTreeWidgetLoader()
{
    interrupt();
}

bool EasyTreeWidgetLoader::done() const
{
    return m_bDone.load();
}

void EasyTreeWidgetLoader::setDone()
{
    m_bDone.store(true);
    //m_progress.store(100);
}

void EasyTreeWidgetLoader::setProgress(int _progress)
{
    m_progress.store(_progress);
}

bool EasyTreeWidgetLoader::interrupted() const
{
    return m_bInterrupt.load();
}

int EasyTreeWidgetLoader::progress() const
{
    return m_progress.load();
}

void EasyTreeWidgetLoader::takeTopLevelItems(ThreadedItems& _output)
{
    if (done())
    {
        _output = ::std::move(m_topLevelItems);
        m_topLevelItems.clear();
    }
}

void EasyTreeWidgetLoader::takeItems(Items& _output)
{
    if (done())
    {
        _output = ::std::move(m_items);
        m_items.clear();
    }
}

void EasyTreeWidgetLoader::interrupt()
{
    m_bInterrupt.store(true);
    if (m_thread.joinable())
        m_thread.join();

    m_bInterrupt.store(false);
    m_bDone.store(false);
    m_progress.store(0);

    auto deleter_thread = ::std::thread([](decltype(m_topLevelItems) _items) {
        for (auto item : _items)
            delete item.second;
    }, ::std::move(m_topLevelItems));
    deleter_thread.detach();

    m_items.clear();
    m_topLevelItems.clear();
}

void EasyTreeWidgetLoader::fillTree(::profiler::timestamp_t& _beginTime, const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree, bool _colorizeRows)
{
    interrupt();
    m_thread = ::std::move(::std::thread(&FillTreeClass<EasyTreeWidgetLoader>::setTreeInternal1, ::std::ref(*this), ::std::ref(m_items), ::std::ref(m_topLevelItems), ::std::ref(_beginTime), _blocksNumber, ::std::ref(_blocksTree), _colorizeRows));
}

void EasyTreeWidgetLoader::fillTreeBlocks(const::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _beginTime, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, bool _colorizeRows)
{
    interrupt();
    m_thread = ::std::move(::std::thread(&FillTreeClass<EasyTreeWidgetLoader>::setTreeInternal2, ::std::ref(*this), ::std::ref(m_items), ::std::ref(m_topLevelItems), _beginTime, ::std::ref(_blocks), _left, _right, _strict, _colorizeRows));
}

//////////////////////////////////////////////////////////////////////////

template <class T>
void FillTreeClass<T>::setTreeInternal1(T& _safelocker, Items& _items, ThreadedItems& _topLevelItems, ::profiler::timestamp_t& _beginTime, const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree, bool _colorizeRows)
{
    _items.reserve(_blocksNumber + _blocksTree.size()); // _blocksNumber does not include Thread root blocks

    ::profiler::timestamp_t finishtime = 0;
    for (const auto& threadTree : _blocksTree)
    {
        const auto node_block = blocksTree(threadTree.second.children.front()).node;
        const auto startTime = node_block->begin();
        const auto endTime = node_block->end();

        if (_beginTime > startTime)
            _beginTime = startTime;

        if (finishtime < endTime)
            finishtime = endTime;
    }

    //const QSignalBlocker b(this);
    int i = 0;
    const int total = static_cast<int>(_blocksTree.size());
    for (const auto& threadTree : _blocksTree)
    {
        if (_safelocker.interrupted())
            break;

        const auto& root = threadTree.second;
        auto item = new EasyTreeWidgetItem();

        if (root.thread_name && root.thread_name[0] != 0)
        {
            item->setText(COL_NAME, QString("%1 Thread %2").arg(root.thread_name).arg(root.thread_id));
        }
        else
        {
            item->setText(COL_NAME, QString("Thread %1").arg(root.thread_id));
        }

        ::profiler::timestamp_t duration = 0;
        if (!root.children.empty())
        {
            duration = blocksTree(root.children.back()).node->end() - blocksTree(root.children.front()).node->begin();
        }

        item->setTimeSmart(COL_DURATION, duration);
        item->setBackgroundColor(::profiler_gui::SELECTED_THREAD_BACKGROUND);
        item->setTextColor(::profiler_gui::SELECTED_THREAD_FOREGROUND);

        //_items.push_back(item);

        // TODO: Optimize children duration calculation (it must be calculated before setTreeInternal now)
        ::profiler::timestamp_t children_duration = 0;
        for (auto i : root.children)
            children_duration += blocksTree(i).node->duration();
        item->setTimeSmart(COL_SELF_DURATION, children_duration);

        children_duration = 0;
        const auto children_items_number = FillTreeClass<T>::setTreeInternal(_safelocker, _items, _beginTime, root.children, item, nullptr, item, _beginTime, finishtime + 1000000000ULL, false, children_duration, _colorizeRows);

        if (children_items_number > 0)
        {
            //total_items += children_items_number + 1;
            //addTopLevelItem(item);
            //m_roots[threadTree.first] = item;
            _topLevelItems.emplace_back(root.thread_id, item);
        }
        else
        {
            //_items.pop_back();
            delete item;
        }

        _safelocker.setProgress((100 * ++i) / total);
    }

    _safelocker.setDone();
    //return total_items;
}

//////////////////////////////////////////////////////////////////////////

auto calculateTotalChildrenNumber(const ::profiler::BlocksTree& _tree) -> decltype(_tree.children.size())
{
    auto children_number = _tree.children.size();
    for (auto i : _tree.children)
        children_number += calculateTotalChildrenNumber(blocksTree(i));
    return children_number;
}

template <class T>
void FillTreeClass<T>::setTreeInternal2(T& _safelocker, Items& _items, ThreadedItems& _topLevelItems, const ::profiler::timestamp_t& _beginTime, const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, bool _colorizeRows)
{
    //size_t blocksNumber = 0;
    //for (const auto& block : _blocks)
    //    blocksNumber += calculateTotalChildrenNumber(*block.tree);
    //    //blocksNumber += block.tree->total_children_number;
    //m_items.reserve(blocksNumber + _blocks.size()); // blocksNumber does not include root blocks

    RootsMap threadsMap;

    int i = 0, total = static_cast<int>(_blocks.size());
    //const QSignalBlocker b(this);
    for (const auto& block : _blocks)
    {
        if (_safelocker.interrupted())
            break;

        auto& gui_block = easyBlock(block.tree);
        const auto startTime = gui_block.tree.node->begin();
        const auto endTime = gui_block.tree.node->end();
        if (startTime > _right || endTime < _left)
        {
            _safelocker.setProgress((90 * ++i) / total);
            continue;
        }

        ::profiler::timestamp_t duration = 0;
        EasyTreeWidgetItem* thread_item = nullptr;
        auto thread_item_it = threadsMap.find(block.root->thread_id);
        if (thread_item_it != threadsMap.end())
        {
            thread_item = thread_item_it->second;
        }
        else
        {
            thread_item = new EasyTreeWidgetItem();

            if (block.root->thread_name && block.root->thread_name[0] != 0)
            {
                thread_item->setText(COL_NAME, QString("%1 Thread %2").arg(block.root->thread_name).arg(block.root->thread_id));
            }
            else
            {
                thread_item->setText(COL_NAME, QString("Thread %1").arg(block.root->thread_id));
            }

            if (!block.root->children.empty())
            {
                duration = blocksTree(block.root->children.back()).node->end() - blocksTree(block.root->children.front()).node->begin();
            }

            thread_item->setTimeSmart(COL_DURATION, duration);
            thread_item->setBackgroundColor(::profiler_gui::SELECTED_THREAD_BACKGROUND);
            thread_item->setTextColor(::profiler_gui::SELECTED_THREAD_FOREGROUND);

            // Calculate clean duration (sum of all children durations)
            ::profiler::timestamp_t children_duration = 0;
            for (auto i : block.root->children)
                children_duration += blocksTree(i).node->duration();
            thread_item->setTimeSmart(COL_SELF_DURATION, children_duration);

            threadsMap.insert(::std::make_pair(block.root->thread_id, thread_item));
        }

        auto item = new EasyTreeWidgetItem(block.tree, thread_item);
        duration = endTime - startTime;

        auto name = *gui_block.tree.node->name() != 0 ? gui_block.tree.node->name() : easyDescriptor(gui_block.tree.node->id()).name();
        item->setText(COL_NAME, ::profiler_gui::toUnicode(name));
        item->setTimeSmart(COL_DURATION, duration);
        item->setTimeMs(COL_BEGIN, startTime - _beginTime);
        item->setTimeMs(COL_END, endTime - _beginTime);

        item->setData(COL_PERCENT_PER_PARENT, Qt::UserRole, 0);
        item->setText(COL_PERCENT_PER_PARENT, "");

        item->setData(COL_PERCENT_PER_FRAME, Qt::UserRole, 0);
        item->setText(COL_PERCENT_PER_FRAME, "");

        if (gui_block.tree.per_thread_stats != nullptr) // if there is per_thread_stats then there are other stats also
        {
            const auto& per_thread_stats = gui_block.tree.per_thread_stats;
            const auto& per_parent_stats = gui_block.tree.per_parent_stats;
            const auto& per_frame_stats = gui_block.tree.per_frame_stats;


            if (per_thread_stats->calls_number > 1 || !EASY_GLOBALS.display_only_relevant_stats)
            {
                item->setTimeSmart(COL_MIN_PER_THREAD, per_thread_stats->min_duration, "min ");
                item->setTimeSmart(COL_MAX_PER_THREAD, per_thread_stats->max_duration, "max ");
                item->setTimeSmart(COL_AVERAGE_PER_THREAD, per_thread_stats->average_duration());
                item->setTimeSmart(COL_DURATION_SUM_PER_THREAD, per_thread_stats->total_duration);
            }

            item->setData(COL_NCALLS_PER_THREAD, Qt::UserRole, per_thread_stats->calls_number);
            item->setText(COL_NCALLS_PER_THREAD, QString::number(per_thread_stats->calls_number));

            auto percentage_per_thread = static_cast<int>(0.5 + 100. * static_cast<double>(per_thread_stats->total_duration) / static_cast<double>(thread_item->selfDuration()));
            item->setData(COL_PERCENT_SUM_PER_THREAD, Qt::UserRole, percentage_per_thread);
            item->setText(COL_PERCENT_SUM_PER_THREAD, QString::number(percentage_per_thread));


            if (per_parent_stats->calls_number > 1 || !EASY_GLOBALS.display_only_relevant_stats)
            {
                item->setTimeSmart(COL_MIN_PER_PARENT, per_parent_stats->min_duration, "min ");
                item->setTimeSmart(COL_MAX_PER_PARENT, per_parent_stats->max_duration, "max ");
                item->setTimeSmart(COL_AVERAGE_PER_PARENT, per_parent_stats->average_duration());
                item->setTimeSmart(COL_DURATION_SUM_PER_PARENT, per_parent_stats->total_duration);
            }

            item->setData(COL_NCALLS_PER_PARENT, Qt::UserRole, per_parent_stats->calls_number);
            item->setText(COL_NCALLS_PER_PARENT, QString::number(per_parent_stats->calls_number));


            if (per_frame_stats->calls_number > 1 || !EASY_GLOBALS.display_only_relevant_stats)
            {
                item->setTimeSmart(COL_MIN_PER_FRAME, per_frame_stats->min_duration, "min ");
                item->setTimeSmart(COL_MAX_PER_FRAME, per_frame_stats->max_duration, "max ");
                item->setTimeSmart(COL_AVERAGE_PER_FRAME, per_frame_stats->average_duration());
                item->setTimeSmart(COL_DURATION_SUM_PER_FRAME, per_frame_stats->total_duration);
            }

            item->setData(COL_NCALLS_PER_FRAME, Qt::UserRole, per_frame_stats->calls_number);
            item->setText(COL_NCALLS_PER_FRAME, QString::number(per_frame_stats->calls_number));
        }
        else
        {
            item->setData(COL_PERCENT_SUM_PER_THREAD, Qt::UserRole, 0);
            item->setText(COL_PERCENT_SUM_PER_THREAD, "");
        }

        const auto color = easyDescriptor(gui_block.tree.node->id()).color();
        const auto bgColor = ::profiler_gui::fromProfilerRgb(::profiler::colors::get_red(color), ::profiler::colors::get_green(color), ::profiler::colors::get_blue(color));
        const auto fgColor = 0x00ffffff - bgColor;
        item->setBackgroundColor(bgColor);
        item->setTextColor(fgColor);

        auto item_index = static_cast<unsigned int>(_items.size());
        _items.push_back(item);

        size_t children_items_number = 0;
        ::profiler::timestamp_t children_duration = 0;
        if (!gui_block.tree.children.empty())
        {
            children_items_number = FillTreeClass<T>::setTreeInternal(_safelocker, _items, _beginTime, gui_block.tree.children, item, item, thread_item, _left, _right, _strict, children_duration, _colorizeRows);
            if (_safelocker.interrupted())
                break;
        }

        int percentage = 100;
        auto self_duration = duration - children_duration;
        if (children_duration > 0 && duration > 0)
        {
            percentage = static_cast<int>(0.5 + 100. * static_cast<double>(self_duration) / static_cast<double>(duration));
        }

        item->setTimeSmart(COL_SELF_DURATION, self_duration);
        item->setData(COL_SELF_DURATION_PERCENT, Qt::UserRole, percentage);
        item->setText(COL_SELF_DURATION_PERCENT, QString::number(percentage));

        if (children_items_number > 0 || !_strict || (startTime >= _left && endTime <= _right))
        {
            //total_items += children_items_number + 1;
            gui_block.tree_item = item_index;

            if (_colorizeRows)
                item->colorize(_colorizeRows);

            if (gui_block.expanded)
                item->setExpanded(true);

        }
        else
        {
            _items.pop_back();
            delete item;
        }

        _safelocker.setProgress((90 * ++i) / total);
    }

    i = 0;
    total = static_cast<int>(threadsMap.size());
    for (auto& it : threadsMap)
    {
        auto item = it.second;

        if (item->childCount() > 0)
        {
            //addTopLevelItem(item);
            //m_roots[it.first] = item;

            //_items.push_back(item);
            _topLevelItems.emplace_back(it.first, item);

            //++total_items;
        }
        else
        {
            delete item;
        }

        _safelocker.setProgress(90 + (10 * ++i) / total);
    }

    _safelocker.setDone();
    //return total_items;
}

template <class T>
size_t FillTreeClass<T>::setTreeInternal(T& _safelocker, Items& _items, const ::profiler::timestamp_t& _beginTime, const ::profiler::BlocksTree::children_t& _children, EasyTreeWidgetItem* _parent, EasyTreeWidgetItem* _frame, EasyTreeWidgetItem* _thread, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, ::profiler::timestamp_t& _duration, bool _colorizeRows)
{
    size_t total_items = 0;
    for (auto child_index : _children)
    {
        if (_safelocker.interrupted())
            break;

        auto& gui_block = easyBlock(child_index);
        const auto& child = gui_block.tree;
        const auto startTime = child.node->begin();
        const auto endTime = child.node->end();
        const auto duration = endTime - startTime;
        _duration += duration;

        if (startTime > _right || endTime < _left)
        {
            continue;
        }

        auto item = new EasyTreeWidgetItem(child_index, _parent);

        auto name = *child.node->name() != 0 ? child.node->name() : easyDescriptor(child.node->id()).name();
        item->setText(COL_NAME, ::profiler_gui::toUnicode(name));
        item->setTimeSmart(COL_DURATION, duration);
        item->setTimeMs(COL_BEGIN, startTime - _beginTime);
        item->setTimeMs(COL_END, endTime - _beginTime);
        item->setData(COL_PERCENT_SUM_PER_THREAD, Qt::UserRole, 0);

        if (child.per_thread_stats != nullptr) // if there is per_thread_stats then there are other stats also
        {
            const auto& per_thread_stats = child.per_thread_stats;
            const auto& per_parent_stats = child.per_parent_stats;
            const auto& per_frame_stats = child.per_frame_stats;

            auto percentage = duration == 0 ? 0 : static_cast<int>(0.5 + 100. * static_cast<double>(duration) / static_cast<double>(_parent->duration()));
            auto percentage_sum = static_cast<int>(0.5 + 100. * static_cast<double>(per_parent_stats->total_duration) / static_cast<double>(_parent->duration()));
            item->setData(COL_PERCENT_PER_PARENT, Qt::UserRole, percentage);
            item->setText(COL_PERCENT_PER_PARENT, QString::number(percentage));
            item->setData(COL_PERCENT_SUM_PER_PARENT, Qt::UserRole, percentage_sum);
            item->setText(COL_PERCENT_SUM_PER_PARENT, QString::number(percentage_sum));

            if (_frame != nullptr)
            {
                if (_parent != _frame)
                {
                    percentage = duration == 0 ? 0 : static_cast<int>(0.5 + 100. * static_cast<double>(duration) / static_cast<double>(_frame->duration()));
                    percentage_sum = static_cast<int>(0.5 + 100. * static_cast<double>(per_frame_stats->total_duration) / static_cast<double>(_frame->duration()));
                }

                item->setData(COL_PERCENT_PER_FRAME, Qt::UserRole, percentage);
                item->setText(COL_PERCENT_PER_FRAME, QString::number(percentage));
                item->setData(COL_PERCENT_SUM_PER_FRAME, Qt::UserRole, percentage_sum);
                item->setText(COL_PERCENT_SUM_PER_FRAME, QString::number(percentage_sum));
            }
            else
            {
                item->setData(COL_PERCENT_PER_FRAME, Qt::UserRole, 0);
                item->setText(COL_PERCENT_PER_FRAME, "");
                item->setData(COL_PERCENT_SUM_PER_FRAME, Qt::UserRole, 0);
                item->setText(COL_PERCENT_SUM_PER_FRAME, "");
            }


            if (per_thread_stats->calls_number > 1 || !EASY_GLOBALS.display_only_relevant_stats)
            {
                item->setTimeSmart(COL_MIN_PER_THREAD, per_thread_stats->min_duration, "min ");
                item->setTimeSmart(COL_MAX_PER_THREAD, per_thread_stats->max_duration, "max ");
                item->setTimeSmart(COL_AVERAGE_PER_THREAD, per_thread_stats->average_duration());
                item->setTimeSmart(COL_DURATION_SUM_PER_THREAD, per_thread_stats->total_duration);
            }

            item->setData(COL_NCALLS_PER_THREAD, Qt::UserRole, per_thread_stats->calls_number);
            item->setText(COL_NCALLS_PER_THREAD, QString::number(per_thread_stats->calls_number));

            if (_thread)
            {
                auto percentage_per_thread = static_cast<int>(0.5 + 100. * static_cast<double>(per_thread_stats->total_duration) / static_cast<double>(_thread->selfDuration()));
                item->setData(COL_PERCENT_SUM_PER_THREAD, Qt::UserRole, percentage_per_thread);
                item->setText(COL_PERCENT_SUM_PER_THREAD, QString::number(percentage_per_thread));
            }


            if (per_parent_stats->calls_number > 1 || !EASY_GLOBALS.display_only_relevant_stats)
            {
                item->setTimeSmart(COL_MIN_PER_PARENT, per_parent_stats->min_duration, "min ");
                item->setTimeSmart(COL_MAX_PER_PARENT, per_parent_stats->max_duration, "max ");
                item->setTimeSmart(COL_AVERAGE_PER_PARENT, per_parent_stats->average_duration());
                item->setTimeSmart(COL_DURATION_SUM_PER_PARENT, per_parent_stats->total_duration);
            }

            item->setData(COL_NCALLS_PER_PARENT, Qt::UserRole, per_parent_stats->calls_number);
            item->setText(COL_NCALLS_PER_PARENT, QString::number(per_parent_stats->calls_number));


            if (per_frame_stats->calls_number > 1 || !EASY_GLOBALS.display_only_relevant_stats)
            {
                item->setTimeSmart(COL_MIN_PER_FRAME, per_frame_stats->min_duration, "min ");
                item->setTimeSmart(COL_MAX_PER_FRAME, per_frame_stats->max_duration, "max ");
                item->setTimeSmart(COL_AVERAGE_PER_FRAME, per_frame_stats->average_duration());
                item->setTimeSmart(COL_DURATION_SUM_PER_FRAME, per_frame_stats->total_duration);
            }

            item->setData(COL_NCALLS_PER_FRAME, Qt::UserRole, per_frame_stats->calls_number);
            item->setText(COL_NCALLS_PER_FRAME, QString::number(per_frame_stats->calls_number));
        }
        else
        {
            item->setData(COL_PERCENT_PER_PARENT, Qt::UserRole, 0);
            item->setText(COL_PERCENT_PER_PARENT, "");
            item->setData(COL_PERCENT_SUM_PER_PARENT, Qt::UserRole, 0);
            item->setText(COL_PERCENT_SUM_PER_PARENT, "");
            item->setData(COL_PERCENT_SUM_PER_THREAD, Qt::UserRole, 0);
            item->setText(COL_PERCENT_SUM_PER_THREAD, "");
        }

        const auto color = easyDescriptor(child.node->id()).color();
        const auto bgColor = ::profiler_gui::fromProfilerRgb(::profiler::colors::get_red(color), ::profiler::colors::get_green(color), ::profiler::colors::get_blue(color));
        const auto fgColor = 0x00ffffff - bgColor;
        item->setBackgroundColor(bgColor);
        item->setTextColor(fgColor);

        auto item_index = static_cast<uint32_t>(_items.size());
        _items.push_back(item);

        size_t children_items_number = 0;
        ::profiler::timestamp_t children_duration = 0;
        if (!child.children.empty())
        {
            children_items_number = FillTreeClass<T>::setTreeInternal(_safelocker, _items, _beginTime, child.children, item, _frame ? _frame : item, _thread, _left, _right, _strict, children_duration, _colorizeRows);
            if (_safelocker.interrupted())
                break;
        }

        int percentage = 100;
        auto self_duration = duration - children_duration;
        if (children_duration > 0 && duration > 0)
        {
            percentage = static_cast<int>(0.5 + 100. * static_cast<double>(self_duration) / static_cast<double>(duration));
        }

        item->setTimeSmart(COL_SELF_DURATION, self_duration);
        item->setData(COL_SELF_DURATION_PERCENT, Qt::UserRole, percentage);
        item->setText(COL_SELF_DURATION_PERCENT, QString::number(percentage));

        if (children_items_number > 0 || !_strict || (startTime >= _left && endTime <= _right))
        {
            total_items += children_items_number + 1;
            gui_block.tree_item = item_index;

            if (_colorizeRows)
                item->colorize(_colorizeRows);

            if (gui_block.expanded)
                item->setExpanded(true);
        }
        else
        {
            _items.pop_back();
            delete item;
        }
    }

    return total_items;
}

//////////////////////////////////////////////////////////////////////////

template struct FillTreeClass<EasyTreeWidgetLoader>;
template struct FillTreeClass<StubLocker>;

//////////////////////////////////////////////////////////////////////////
