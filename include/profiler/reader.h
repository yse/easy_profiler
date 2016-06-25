/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef PROFILER_READER____H
#define PROFILER_READER____H

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <vector>
#include <map>
#include "profiler/profiler.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace profiler {

    typedef uint32_t calls_number_t;

    struct BlockStatistics
    {
        ::profiler::timestamp_t      total_duration;
        ::profiler::timestamp_t        min_duration;
        ::profiler::timestamp_t        max_duration;
        ::profiler::timestamp_t    average_duration;
        ::profiler::calls_number_t     calls_number;

        BlockStatistics() : total_duration(0), min_duration(0), max_duration(0), average_duration(0), calls_number(0)
        {
        }

    }; // END of struct BlockStatistics.

} // END of namespace profiler.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct BlocksTree
{
    typedef ::std::vector<BlocksTree> children_t;

    children_t                           children;
    ::profiler::SerilizedBlock*              node;
    ::profiler::BlockStatistics* frame_statistics; ///< Pointer to statistics for this block within the parent (may be nullptr for top-level blocks)
    ::profiler::BlockStatistics* total_statistics; ///< Pointer to statistics for this block within the bounds of all frames per current thread 

    BlocksTree() : node(nullptr), frame_statistics(nullptr), total_statistics(nullptr)
	{

    }

    BlocksTree(BlocksTree&& that)
    {
        makeMove(::std::forward<BlocksTree&&>(that));
    }

    BlocksTree& operator=(BlocksTree&& that)
    {
        makeMove(::std::forward<BlocksTree&&>(that));
        return *this;
    }

    ~BlocksTree()
    {
        if (node)
        {
            delete node;
        }
    }

    bool operator < (const BlocksTree& other) const
    {
        if (!node || !other.node)
        {
            return false;
        }
        return node->block()->getBegin() < other.node->block()->getBegin();
    }

private:

    void makeMove(BlocksTree&& that)
    {
        children = ::std::move(that.children);
        node = that.node;
        frame_statistics = that.frame_statistics;
        total_statistics = that.total_statistics;

        that.node = nullptr;
        that.frame_statistics = nullptr;
        that.total_statistics = nullptr;
    }

}; // END of struct BlocksTree.


typedef ::std::map<::profiler::thread_id_t, BlocksTree> thread_blocks_tree_t;

extern "C"{
	int PROFILER_API fillTreesFromFile(const char* filename, thread_blocks_tree_t& threaded_trees);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // PROFILER_READER____H
