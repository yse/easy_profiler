/************************************************************************
* file name         : reader.cpp
* ----------------- :
* creation time     : 2016/06/19
* copyright         : (c) 2016 Sergey Yagovtsev, Victor Zarubkin
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
* license           : TODO: add license text
************************************************************************/

#include "profiler/reader.h"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <unordered_map>
#include <thread>

//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

/** \brief Simple C-string pointer with length.

It is used as base class for a key in std::unordered_map.
It is used to get better performance than std::string.
It simply stores a pointer and a length, there is no
any memory allocation and copy.

\note It is absolutely safe to store pointer because std::unordered_map,
which uses it as a key, exists only inside fillTreesFromFile function.

*/
class cstring
{
protected:

    const char* str;
    size_t  str_len;

public:

    cstring(const char* _str) : str(_str), str_len(strlen(_str))
    {
    }

    cstring(const cstring& _other) : str(_other.str), str_len(_other.str_len)
    {
    }

    inline bool operator == (const cstring& _other) const
    {
        return str_len == _other.str_len && !strncmp(str, _other.str, str_len);
    }

    inline bool operator != (const cstring& _other) const
    {
        return !operator == (_other);
    }

    inline bool operator < (const cstring& _other) const
    {
        if (str_len == _other.str_len)
        {
            return strncmp(str, _other.str, str_len) < 0;
        }

        return str_len < _other.str_len;
    }
};

/** \brief cstring with precalculated hash.

This is used to calculate hash for C-string and to cache it
to be used in the future without recurring hash calculatoin.

\note This class is used as a key in std::unordered_map.

*/
class hashed_cstr : public cstring
{
    typedef cstring Parent;

public:

    size_t str_hash;

    hashed_cstr(const char* _str) : Parent(_str), str_hash(0)
    {
        str_hash = ::std::_Hash_seq((const unsigned char *)str, str_len);
    }

    hashed_cstr(const hashed_cstr& _other) : Parent(_other), str_hash(_other.str_hash)
    {
    }

    inline bool operator == (const hashed_cstr& _other) const
    {
        return str_hash == _other.str_hash && Parent::operator == (_other);
    }

    inline bool operator != (const hashed_cstr& _other) const
    {
        return !operator == (_other);
    }
};

namespace std {

    /** \brief Simply returns precalculated hash of a C-string. */
    template <>
    struct hash<hashed_cstr>
    {
        inline size_t operator () (const hashed_cstr& _str) const
        {
            return _str.str_hash;
        }
    };

}

typedef ::std::unordered_map<hashed_cstr, ::profiler::BlockStatistics*> StatsMap;

#else

// TODO: optimize for Linux too
#include <string>
typedef ::std::unordered_map<::std::string, ::profiler::BlockStatistics*> StatsMap;

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
::profiler::BlockStatistics* update_statistics(StatsMap& _stats_map, const ::profiler::BlocksTree& _current)
{
    auto duration = _current.node->block()->duration();
    StatsMap::key_type key(_current.node->getName());
    auto it = _stats_map.find(key);
    if (it != _stats_map.end())
    {
        // Update already existing statistics

        auto stats = it->second; // write pointer to statistics into output (this is BlocksTree:: per_thread_stats or per_parent_stats or per_frame_stats)

        ++stats->calls_number; // update calls number of this block
        stats->total_duration += duration; // update summary duration of all block calls

        if (duration > stats->max_duration)
        {
            // update max duration
            stats->max_duration_block = _current.block_index;
            stats->max_duration = duration;
        }

        if (duration < stats->min_duration)
        {
            // update min duraton
            stats->min_duration_block = _current.block_index;
            stats->min_duration = duration;
        }

        // average duration is calculated inside average_duration() method by dividing total_duration to the calls_number

        return stats;
    }

    // This is first time the block appear in the file.
    // Create new statistics.
    auto stats = new ::profiler::BlockStatistics(duration, _current.block_index);
    _stats_map.insert(::std::make_pair(key, stats));

    return stats;
}

//////////////////////////////////////////////////////////////////////////

void update_statistics_recursive(StatsMap& _stats_map, ::profiler::BlocksTree& _current)
{
    _current.per_frame_stats = update_statistics(_stats_map, _current);
    for (auto& child : _current.children)
    {
        update_statistics_recursive(_stats_map, child);
    }
}

//////////////////////////////////////////////////////////////////////////

typedef ::std::map<::profiler::thread_id_t, StatsMap> PerThreadStats;

extern "C"{

    unsigned int fillTreesFromFile(::std::atomic<int>& progress, const char* filename, ::profiler::SerializedData& serialized_blocks, ::profiler::thread_blocks_tree_t& threaded_trees, bool gather_statistics)
	{
        PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(::profiler::colors::Cyan)

		::std::ifstream inFile(filename, ::std::fstream::binary);
        progress.store(0);

		if (!inFile.is_open())
        {
            progress.store(100);
            return 0;
		}

        PerThreadStats thread_statistics, parent_statistics, frame_statistics;
		unsigned int blocks_counter = 0;

        uint64_t memory_size = 0;
        inFile.read((char*)&memory_size, sizeof(uint64_t));
        if (memory_size == 0)
        {
            progress.store(100);
            return 0;
        }

        serialized_blocks.set(new char[memory_size]);
        //memset(serialized_blocks[0], 0, memory_size);
        uint64_t i = 0;

		while (!inFile.eof())
        {
            PROFILER_BEGIN_BLOCK_GROUPED("Read block from file", ::profiler::colors::Green)
			uint16_t sz = 0;
			inFile.read((char*)&sz, sizeof(sz));
			if (sz == 0)
			{
				inFile.read((char*)&sz, sizeof(sz));
				continue;
			}

            // TODO: use salloc::shared_allocator for allocation/deallocation safety
            char* data = serialized_blocks[i];//new char[sz];
			inFile.read(data, sz);
            i += sz;
			auto baseData = reinterpret_cast<::profiler::SerializedBlock*>(data);

            ::profiler::BlocksTree tree;
            tree.node = baseData;// new ::profiler::SerializedBlock(sz, data);
			tree.block_index = blocks_counter++;

            auto block_thread_id = baseData->getThreadId();
            auto& root = threaded_trees[block_thread_id];
            auto& per_parent_statistics = parent_statistics[block_thread_id];
            auto& per_thread_statistics = thread_statistics[block_thread_id];

            if (::profiler::BLOCK_TYPE_THREAD_SIGN == baseData->getType())
            {
                root.thread_name = tree.node->getName();
            }

            if (!root.tree.children.empty())
            {
                auto& back = root.tree.children.back();
                auto t1 = back.node->block()->getEnd();
                auto mt0 = tree.node->block()->getBegin();
                if (mt0 < t1)//parent - starts earlier than last ends
                {
                    //auto lower = ::std::lower_bound(root.children.begin(), root.children.end(), tree);
                    /**/
                    PROFILER_BEGIN_BLOCK_GROUPED("Find children", ::profiler::colors::Blue)
                    auto rlower1 = ++root.tree.children.rbegin();
                    for(; rlower1 != root.tree.children.rend(); ++rlower1){
                        if(mt0 > rlower1->node->block()->getBegin())
                        {
                            break;
                        }
                    }
                    auto lower = rlower1.base();
                    ::std::move(lower, root.tree.children.end(), ::std::back_inserter(tree.children));

                    root.tree.children.erase(lower, root.tree.children.end());

                    ::profiler::timestamp_t children_duration = 0;
                    if (gather_statistics)
                    {
                        PROFILER_BEGIN_BLOCK_GROUPED("Gather statistic within parent", ::profiler::colors::Magenta)
                        per_parent_statistics.clear();

                        //per_parent_statistics.reserve(tree.children.size());     // this gives slow-down on Windows
                        //per_parent_statistics.reserve(tree.children.size() * 2); // this gives no speed-up on Windows
                        // TODO: check this behavior on Linux

                        for (auto& child : tree.children)
                        {
                            child.per_parent_stats = update_statistics(per_parent_statistics, child);

                            children_duration += child.node->block()->duration();
                            if (tree.depth < child.depth)
                                tree.depth = child.depth;
                        }
                    }
                    else
                    {
                        for (const auto& child : tree.children)
                        {
                            children_duration += child.node->block()->duration();
                            if (tree.depth < child.depth)
                                tree.depth = child.depth;
                        }
                    }

                    ++tree.depth;
                }
            }

            root.tree.children.emplace_back(::std::move(tree));


			//delete[] data;

			

            if (gather_statistics)
            {
                PROFILER_BEGIN_BLOCK_GROUPED("Gather per thread statistics", ::profiler::colors::Coral)
                auto& current = root.tree.children.back();
                current.per_thread_stats = update_statistics(per_thread_statistics, current);
            }

            if (progress.load() < 0)
                break;
            progress.store(static_cast<int>(90 * i / memory_size));
		}

        if (progress.load() < 0)
        {
            progress.store(100);
            serialized_blocks.clear();
            threaded_trees.clear();
            return 0;
        }

        PROFILER_BEGIN_BLOCK_GROUPED("Gather statistics for roots", ::profiler::colors::Purple)
        if (gather_statistics)
		{
            ::std::vector<::std::thread> statistics_threads;
            statistics_threads.reserve(threaded_trees.size());

			for (auto& it : threaded_trees)
			{
                auto& root = it.second;
                root.thread_id = it.first;
                root.tree.shrink_to_fit();

                auto& per_frame_statistics = frame_statistics[root.thread_id];
                auto& per_parent_statistics = parent_statistics[it.first];
				per_parent_statistics.clear();

                statistics_threads.emplace_back(::std::thread([&per_parent_statistics, &per_frame_statistics](::profiler::BlocksTreeRoot& root)
                {
                    for (auto& frame : root.tree.children)
                    {
                        frame.per_parent_stats = update_statistics(per_parent_statistics, frame);

                        // TODO: Optimize per frame stats gathering
                        per_frame_statistics.clear();
                        update_statistics_recursive(per_frame_statistics, frame);

                        if (root.tree.depth < frame.depth)
                            root.tree.depth = frame.depth;
                    }

                    ++root.tree.depth;
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

                root.tree.shrink_to_fit();
                for (auto& frame : root.tree.children)
                {
                    if (root.tree.depth < frame.depth)
                        root.tree.depth = frame.depth;
                }

                ++root.tree.depth;

                progress.store(90 + (10 * ++j) / n);
            }
        }
        PROFILER_END_BLOCK
        // No need to delete BlockStatistics instances - they will be deleted inside BlocksTree destructors

        progress.store(100);
        return blocks_counter;
	}
}
