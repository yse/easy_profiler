/************************************************************************
* file name         : reader.cpp
* ----------------- : 
* creation time     : 2016/06/19
* authors           : Sergey Yagovtsev, Victor Zarubkin
* emails            : yse.sey@gmail.com, v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of fillTreesFromFile function
*                   : which reads profiler file and fill profiler blocks tree.
* ----------------- :
* change log        : * 2016/06/19 Sergey Yagovtsev: First fillTreesFromFile implementation.
*                   :
*                   : * 2016/06/25 Victor Zarubkin: Removed unnecessary memory allocation and copy
*                   :       when creating and inserting blocks into the tree.
*                   :
*                   : * 2016/06/26 Victor Zarubkin: Added statistics gathering (min, max, average duration,
*                   :       number of block calls).
*                   : * 2016/06/26 Victor Zarubkin, Sergey Yagovtsev: Added statistics gathering for root
*                   :       blocks in the tree.
*                   :
*                   : * 2016/06/29 Victor Zarubkin: Added calculaton of total children number for blocks.
*                   :
*                   : * 2016/06/30 Victor Zarubkin: Added this header.
*                   :       Added tree depth calculation.
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

#include "profiler/reader.h"
#include "hashed_cstr.h"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <unordered_map>
#include <thread>

//////////////////////////////////////////////////////////////////////////

struct passthrough_hash {
    template <class T> inline size_t operator () (T _value) const {
        return static_cast<size_t>(_value);
    }
};

//////////////////////////////////////////////////////////////////////////

namespace profiler {

    void SerializedData::set(char* _data)
    {
        if (m_data != nullptr)
            delete[] m_data;
        m_data = _data;
    }

    extern "C" void release_stats(BlockStatistics*& _stats)
    {
        if (!_stats)
        {
            return;
        }

        if (--_stats->calls_number == 0)
        {
            delete _stats;
        }

        _stats = nullptr;
    }

}

//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

typedef ::std::unordered_map<::profiler::block_id_t, ::profiler::BlockStatistics*, passthrough_hash> StatsMap;

/** \note It is absolutely safe to use hashed_cstr (which simply stores pointer) because std::unordered_map,
which uses it as a key, exists only inside fillTreesFromFile function. */
typedef ::std::unordered_map<::profiler::hashed_cstr, ::profiler::block_id_t> IdMap;

#else

// TODO: Create optimized version of profiler::hashed_cstr for Linux too.
typedef ::std::unordered_map<::profiler::block_id_t, ::profiler::BlockStatistics*, passthrough_hash> StatsMap;
typedef ::std::unordered_map<::profiler::hashed_stdstring, ::profiler::block_id_t> IdMap;

#endif

//////////////////////////////////////////////////////////////////////////

/** \brief Updates statistics for a profiler block.

\param _stats_map Storage of statistics for blocks.
\param _current Pointer to the current block.
\param _stats Reference to the variable where pointer to the block statistics must be written.

\note All blocks with similar name have the same pointer to statistics information.

\note As all profiler block keeps a pointer to it's statistics, all similar blocks
automatically receive statistics update.

*/
::profiler::BlockStatistics* update_statistics(StatsMap& _stats_map, const ::profiler::BlocksTree& _current, ::profiler::block_index_t _current_index)
{
    auto duration = _current.node->duration();
    //StatsMap::key_type key(_current.node->name());
    //auto it = _stats_map.find(key);
    auto it = _stats_map.find(_current.node->id());
    if (it != _stats_map.end())
    {
        // Update already existing statistics

        auto stats = it->second; // write pointer to statistics into output (this is BlocksTree:: per_thread_stats or per_parent_stats or per_frame_stats)

        ++stats->calls_number; // update calls number of this block
        stats->total_duration += duration; // update summary duration of all block calls

        if (duration > stats->max_duration)
        {
            // update max duration
            stats->max_duration_block = _current_index;
            stats->max_duration = duration;
        }

        if (duration < stats->min_duration)
        {
            // update min duraton
            stats->min_duration_block = _current_index;
            stats->min_duration = duration;
        }

        // average duration is calculated inside average_duration() method by dividing total_duration to the calls_number

        return stats;
    }

    // This is first time the block appear in the file.
    // Create new statistics.
    auto stats = new ::profiler::BlockStatistics(duration, _current_index);
    //_stats_map.emplace(key, stats);
    _stats_map.emplace(_current.node->id(), stats);

    return stats;
}

//////////////////////////////////////////////////////////////////////////

void update_statistics_recursive(StatsMap& _stats_map, ::profiler::BlocksTree& _current, ::profiler::block_index_t _current_index, ::profiler::blocks_t& _blocks)
{
    _current.per_frame_stats = update_statistics(_stats_map, _current, _current_index);
    for (auto i : _current.children)
    {
        update_statistics_recursive(_stats_map, _blocks[i], i, _blocks);
    }
}

//////////////////////////////////////////////////////////////////////////

extern "C" ::profiler::block_index_t fillTreesFromFile(::std::atomic<int>& progress, const char* filename, ::profiler::SerializedData& serialized_blocks, ::profiler::SerializedData& serialized_descriptors, ::profiler::descriptors_list_t& descriptors, ::profiler::blocks_t& blocks, ::profiler::thread_blocks_tree_t& threaded_trees, bool gather_statistics)
{
    EASY_FUNCTION(::profiler::colors::Cyan);

    ::std::ifstream inFile(filename, ::std::fstream::binary);
    progress.store(0);

    if (!inFile.is_open())
        return 0;

    int64_t cpu_frequency = 0LL;
    inFile.read((char*)&cpu_frequency, sizeof(int64_t));

    uint32_t total_blocks_number = 0;
    inFile.read((char*)&total_blocks_number, sizeof(decltype(total_blocks_number)));
    if (total_blocks_number == 0)
        return 0;

    uint64_t memory_size = 0;
    inFile.read((char*)&memory_size, sizeof(decltype(memory_size)));
    if (memory_size == 0)
        return 0;

    serialized_blocks.set(new char[memory_size]);
    //memset(serialized_blocks[0], 0, memory_size);


    uint32_t total_descriptors_number = 0;
    inFile.read((char*)&total_descriptors_number, sizeof(decltype(total_descriptors_number)));
    if (total_descriptors_number == 0)
        return 0;

    uint64_t descriptors_memory_size = 0;
    inFile.read((char*)&descriptors_memory_size, sizeof(decltype(descriptors_memory_size)));
    if (descriptors_memory_size == 0)
        return 0;

    descriptors.reserve(total_descriptors_number);
    serialized_descriptors.set(new char[descriptors_memory_size]);

    uint64_t i = 0;
    while (!inFile.eof() && descriptors.size() < total_descriptors_number)
    {
        uint16_t sz = 0;
        inFile.read((char*)&sz, sizeof(sz));
        if (sz == 0)
            return 0;

        //if (i + sz > descriptors_memory_size)
        //{
        //    printf("FILE CORRUPTED\n");
        //    return 0;
        //}

        char* data = serialized_descriptors[i];
        inFile.read(data, sz);
        auto descriptor = reinterpret_cast<::profiler::SerializedBlockDescriptor*>(data);
        descriptors.push_back(descriptor);

        i += sz;
        progress.store(static_cast<int>(10 * i / descriptors_memory_size));
    }

    typedef ::std::map<::profiler::thread_id_t, StatsMap> PerThreadStats;
    PerThreadStats thread_statistics, parent_statistics, frame_statistics;
    IdMap identification_table;

    i = 0;
    uint32_t read_number = 0;
    ::profiler::block_index_t blocks_counter = 0;
    blocks.reserve(total_blocks_number);
    while (!inFile.eof() && read_number < total_blocks_number)
    {
        EASY_BLOCK("Read thread data", ::profiler::colors::DarkGreen);

        ::profiler::thread_id_t thread_id = 0;
        inFile.read((char*)&thread_id, sizeof(decltype(thread_id)));

        auto& root = threaded_trees[thread_id];

        uint32_t blocks_number_in_thread = 0;
        inFile.read((char*)&blocks_number_in_thread, sizeof(decltype(blocks_number_in_thread)));
        auto threshold = read_number + blocks_number_in_thread;
        while (!inFile.eof() && read_number < threshold)
        {
            EASY_BLOCK("Read context switch", ::profiler::colors::Green);

            ++read_number;

            uint16_t sz = 0;
            inFile.read((char*)&sz, sizeof(sz));
            if (sz == 0)
                return 0;

            char* data = serialized_blocks[i];
            inFile.read(data, sz);
            i += sz;
            auto baseData = reinterpret_cast<::profiler::SerializedBlock*>(data);

            if (cpu_frequency != 0)
            {
                auto t_begin = reinterpret_cast<::profiler::timestamp_t*>(data);
                auto t_end = t_begin + 1;

                *t_begin *= 1000000000LL;
                *t_begin /= cpu_frequency;

                *t_end *= 1000000000LL;
                *t_end /= cpu_frequency;
            }

            blocks.emplace_back();
            ::profiler::BlocksTree& tree = blocks.back();
            tree.node = baseData;
            const auto block_index = blocks_counter++;

            auto descriptor = descriptors[baseData->id()];
            if (descriptor->type() != ::profiler::BLOCK_TYPE_CONTEXT_SWITCH)
                continue;

            root.sync.emplace_back(block_index);

            if (progress.load() < 0)
                break;

            progress.store(10 + static_cast<int>(80 * i / memory_size));
        }

        if (progress.load() < 0 || inFile.eof())
            break;

        blocks_number_in_thread = 0;
        inFile.read((char*)&blocks_number_in_thread, sizeof(decltype(blocks_number_in_thread)));
        threshold = read_number + blocks_number_in_thread;
        while (!inFile.eof() && read_number < threshold)
        {
            EASY_BLOCK("Read block", ::profiler::colors::Green);

            ++read_number;

            uint16_t sz = 0;
            inFile.read((char*)&sz, sizeof(sz));
            if (sz == 0)
                return 0;

            char* data = serialized_blocks[i];
            inFile.read(data, sz);
            i += sz;
            auto baseData = reinterpret_cast<::profiler::SerializedBlock*>(data);

            if (cpu_frequency != 0)
            {
                auto t_begin = reinterpret_cast<::profiler::timestamp_t*>(data);
                auto t_end = t_begin + 1;

                *t_begin *= 1000000000LL;
                *t_begin /= cpu_frequency;

                *t_end *= 1000000000LL;
                *t_end /= cpu_frequency;
            }

            blocks.emplace_back();
            ::profiler::BlocksTree& tree = blocks.back();
            tree.node = baseData;// new ::profiler::SerializedBlock(sz, data);
            const auto block_index = blocks_counter++;

            auto& per_parent_statistics = parent_statistics[thread_id];
            auto& per_thread_statistics = thread_statistics[thread_id];
            auto descriptor = descriptors[baseData->id()];

            if (descriptor->type() == ::profiler::BLOCK_TYPE_THREAD_SIGN)
            {
                root.thread_name = tree.node->name();
            }

            if (*tree.node->name() != 0)
            {
                // If block has runtime name then generate new id for such block.
                // Blocks with the same name will have same id.

                IdMap::key_type key(tree.node->name());
                auto it = identification_table.find(key);
                if (it != identification_table.end())
                {
                    // There is already block with such name, use it's id
                    baseData->setId(it->second);
                }
                else
                {
                    // There were no blocks with such name, generate new id and save it in the table for further usage.
                    auto id = static_cast<::profiler::block_id_t>(descriptors.size());
                    identification_table.emplace(key, id);
                    descriptors.push_back(descriptors[baseData->id()]);
                    baseData->setId(id);
                }
            }

            if (!root.children.empty())
            {
                auto& back = blocks[root.children.back()];
                auto t1 = back.node->end();
                auto mt0 = tree.node->begin();
                if (mt0 < t1)//parent - starts earlier than last ends
                {
                    //auto lower = ::std::lower_bound(root.children.begin(), root.children.end(), tree);
                    /**/
                    EASY_BLOCK("Find children", ::profiler::colors::Blue);
                    auto rlower1 = ++root.children.rbegin();
                    for (; rlower1 != root.children.rend() && !(mt0 > blocks[*rlower1].node->begin()); ++rlower1);
                    auto lower = rlower1.base();
                    ::std::move(lower, root.children.end(), ::std::back_inserter(tree.children));

                    root.children.erase(lower, root.children.end());
                    EASY_END_BLOCK;

                    ::profiler::timestamp_t children_duration = 0;
                    if (gather_statistics)
                    {
                        EASY_BLOCK("Gather statistic within parent", ::profiler::colors::Magenta);
                        per_parent_statistics.clear();

                        //per_parent_statistics.reserve(tree.children.size());     // this gives slow-down on Windows
                        //per_parent_statistics.reserve(tree.children.size() * 2); // this gives no speed-up on Windows
                        // TODO: check this behavior on Linux

                        for (auto i : tree.children)
                        {
                            auto& child = blocks[i];
                            child.per_parent_stats = update_statistics(per_parent_statistics, child, i);

                            children_duration += child.node->duration();
                            if (tree.depth < child.depth)
                                tree.depth = child.depth;
                        }
                    }
                    else
                    {
                        for (auto i : tree.children)
                        {
                            const auto& child = blocks[i];
                            children_duration += child.node->duration();
                            if (tree.depth < child.depth)
                                tree.depth = child.depth;
                        }
                    }

                    ++tree.depth;
                }
            }

            root.children.emplace_back(block_index);// ::std::move(tree));



            if (gather_statistics)
            {
                EASY_BLOCK("Gather per thread statistics", ::profiler::colors::Coral);
                tree.per_thread_stats = update_statistics(per_thread_statistics, tree, block_index);
            }

            if (progress.load() < 0)
                break;
            progress.store(10 + static_cast<int>(80 * i / memory_size));
        }
    }

    if (progress.load() < 0)
    {
        serialized_blocks.clear();
        threaded_trees.clear();
        blocks.clear();
        return 0;
    }

    EASY_BLOCK("Gather statistics for roots", ::profiler::colors::Purple);
    if (gather_statistics)
    {
        ::std::vector<::std::thread> statistics_threads;
        statistics_threads.reserve(threaded_trees.size());

        for (auto& it : threaded_trees)
        {
            auto& root = it.second;
            root.thread_id = it.first;
            //root.tree.shrink_to_fit();

            auto& per_frame_statistics = frame_statistics[root.thread_id];
            auto& per_parent_statistics = parent_statistics[it.first];
            per_parent_statistics.clear();

            statistics_threads.emplace_back(::std::thread([&per_parent_statistics, &per_frame_statistics, &blocks](::profiler::BlocksTreeRoot& root)
            {
                //::std::sort(root.sync.begin(), root.sync.end(), [&blocks](::profiler::block_index_t left, ::profiler::block_index_t right)
                //{
                //    return blocks[left].node->begin() < blocks[right].node->begin();
                //});

                for (auto i : root.children)
                {
                    auto& frame = blocks[i];
                    frame.per_parent_stats = update_statistics(per_parent_statistics, frame, i);

                    per_frame_statistics.clear();
                    update_statistics_recursive(per_frame_statistics, frame, i, blocks);

                    if (root.depth < frame.depth)
                        root.depth = frame.depth;
                }

                ++root.depth;
            }, ::std::ref(root)));
        }

        int j = 0, n = static_cast<int>(statistics_threads.size());
        for (auto& t : statistics_threads)
        {
            t.join();
            progress.store(90 + (10 * ++j) / n);
        }
    }
    else
    {
        int j = 0, n = static_cast<int>(threaded_trees.size());
        for (auto& it : threaded_trees)
        {
            auto& root = it.second;
            root.thread_id = it.first;

            //::std::sort(root.sync.begin(), root.sync.end(), [&blocks](::profiler::block_index_t left, ::profiler::block_index_t right)
            //{
            //    return blocks[left].node->begin() < blocks[right].node->begin();
            //});

            //root.tree.shrink_to_fit();
            for (auto i : root.children)
            {
                auto& frame = blocks[i];
                if (root.depth < frame.depth)
                    root.depth = frame.depth;
            }

            ++root.depth;

            progress.store(90 + (10 * ++j) / n);
        }
    }
    // No need to delete BlockStatistics instances - they will be deleted inside BlocksTree destructors

    return blocks_counter;
}
