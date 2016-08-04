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

#include <list>
#include <map>
#include "profiler/profiler.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace profiler {

    typedef uint32_t calls_number_t;

    struct BlockStatistics final
    {
        ::profiler::timestamp_t        total_duration; ///< Summary duration of all block calls
        ::profiler::timestamp_t          min_duration; ///< Cached block->duration() value. TODO: Remove this if memory consumption will be too high
        ::profiler::timestamp_t          max_duration; ///< Cached block->duration() value. TODO: Remove this if memory consumption will be too high
        unsigned int               min_duration_block; ///< Will be used in GUI to jump to the block with min duration
        unsigned int               max_duration_block; ///< Will be used in GUI to jump to the block with max duration
        ::profiler::calls_number_t       calls_number; ///< Block calls number

        // TODO: It is better to replace SerilizedBlock* with BlocksTree*, but this requires to store pointers in children list.

        BlockStatistics()
            : total_duration(0)
            , min_duration(0)
            , max_duration(0)
            , min_duration_block(0)
            , max_duration_block(0)
            , calls_number(1)
        {
        }

        BlockStatistics(::profiler::timestamp_t _duration, unsigned int _block_index)
            : total_duration(_duration)
            , min_duration(_duration)
            , max_duration(_duration)
            , min_duration_block(_block_index)
            , max_duration_block(_block_index)
            , calls_number(1)
        {
        }

        inline ::profiler::timestamp_t average_duration() const
        {
            return total_duration / calls_number;
        }

    }; // END of struct BlockStatistics.

    inline void release(BlockStatistics*& _stats)
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

    //////////////////////////////////////////////////////////////////////////

    class BlocksTree
    {
        typedef BlocksTree This;

    public:

        typedef ::std::list<BlocksTree> children_t;

        children_t                           children; ///< List of children blocks. May be empty.
        ::profiler::SerilizedBlock*              node; ///< Pointer to serilized data (type, name, begin, end etc.)
        ::profiler::BlockStatistics* frame_statistics; ///< Pointer to statistics for this block within the parent (may be nullptr for top-level blocks)
        ::profiler::BlockStatistics* total_statistics; ///< Pointer to statistics for this block within the bounds of all frames per current thread

        unsigned int                      block_index; ///< Index of this block
        unsigned int            total_children_number; ///< Number of all children including number of grandchildren (and so on)
        unsigned short                          depth; ///< Maximum number of sublevels (maximum children depth)

        BlocksTree()
            : node(nullptr)
            , frame_statistics(nullptr)
            , total_statistics(nullptr)
            , block_index(0)
            , total_children_number(0)
            , depth(0)
        {

        }

        BlocksTree(This&& that) : BlocksTree()
        {
            makeMove(::std::forward<This&&>(that));
        }

        This& operator = (This&& that)
        {
            makeMove(::std::forward<This&&>(that));
            return *this;
        }

        ~BlocksTree()
        {
            if (node)
            {
                delete node;
            }

            release(total_statistics);
            release(frame_statistics);
        }

        bool operator < (const This& other) const
        {
            if (!node || !other.node)
            {
                return false;
            }
            return node->block()->getBegin() < other.node->block()->getBegin();
        }

    private:

        BlocksTree(const This&) = delete;
        This& operator = (const This&) = delete;

        void makeMove(This&& that)
        {
            if (node && node != that.node)
            {
                delete node;
            }

            if (total_statistics != that.total_statistics)
            {
                release(total_statistics);
            }

            if (frame_statistics != that.frame_statistics)
            {
                release(frame_statistics);
            }

            children = ::std::move(that.children);
            node = that.node;
            frame_statistics = that.frame_statistics;
            total_statistics = that.total_statistics;

            block_index = that.block_index;
            total_children_number = that.total_children_number;
            depth = that.depth;

            that.node = nullptr;
            that.frame_statistics = nullptr;
            that.total_statistics = nullptr;
        }

    }; // END of class BlocksTree.

    //////////////////////////////////////////////////////////////////////////

    class BlocksTreeRoot final
    {
        typedef BlocksTreeRoot This;

    public:

        BlocksTree                     tree;
        const char*             thread_name;
        ::profiler::thread_id_t   thread_id;

        BlocksTreeRoot() : thread_name(""), thread_id(0)
        {
        }

        BlocksTreeRoot(This&& that) : tree(::std::move(that.tree)), thread_name(that.thread_name), thread_id(that.thread_id)
        {
        }

        This& operator = (This&& that)
        {
            tree = ::std::move(that.tree);
            thread_name = that.thread_name;
            return *this;
        }

        bool operator < (const This& other) const
        {
            return tree < other.tree;
        }

    private:

        BlocksTreeRoot(const This&) = delete;
        This& operator = (const This&) = delete;

    }; // END of class BlocksTreeRoot.

    typedef ::std::map<::profiler::thread_id_t, ::profiler::BlocksTreeRoot> thread_blocks_tree_t;

} // END of namespace profiler.

extern "C"{
    unsigned int PROFILER_API fillTreesFromFile(const char* filename, ::profiler::thread_blocks_tree_t& threaded_trees, bool gather_statistics = false);
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // PROFILER_READER____H
