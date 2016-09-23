/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin

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

#include <unordered_map>
#include <vector>
#include <string>
#include <atomic>
#include "profiler/profiler.h"
#include "profiler/serialized_block.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace profiler {

    typedef uint32_t calls_number_t;
    typedef uint32_t block_index_t;

#pragma pack(push, 1)
    struct BlockStatistics EASY_FINAL
    {
        ::profiler::timestamp_t        total_duration; ///< Summary duration of all block calls
        ::profiler::timestamp_t          min_duration; ///< Cached block->duration() value. TODO: Remove this if memory consumption will be too high
        ::profiler::timestamp_t          max_duration; ///< Cached block->duration() value. TODO: Remove this if memory consumption will be too high
        ::profiler::block_index_t  min_duration_block; ///< Will be used in GUI to jump to the block with min duration
        ::profiler::block_index_t  max_duration_block; ///< Will be used in GUI to jump to the block with max duration
        ::profiler::block_index_t        parent_block; ///< Index of block which is "parent" for "per_parent_stats" or "frame" for "per_frame_stats" or thread-id for "per_thread_stats"
        ::profiler::calls_number_t       calls_number; ///< Block calls number

        explicit BlockStatistics(::profiler::timestamp_t _duration, ::profiler::block_index_t _block_index, ::profiler::block_index_t _parent_index)
            : total_duration(_duration)
            , min_duration(_duration)
            , max_duration(_duration)
            , min_duration_block(_block_index)
            , max_duration_block(_block_index)
            , parent_block(_parent_index)
            , calls_number(1)
        {
        }

        //BlockStatistics() = default;

        inline ::profiler::timestamp_t average_duration() const
        {
            return total_duration / calls_number;
        }

    }; // END of struct BlockStatistics.
#pragma pack(pop)

    extern "C" PROFILER_API void release_stats(BlockStatistics*& _stats);

    //////////////////////////////////////////////////////////////////////////

    class BlocksTree EASY_FINAL
    {
        typedef BlocksTree This;

    public:

        typedef ::std::vector<This> blocks_t;
        typedef ::std::vector<::profiler::block_index_t> children_t;

        children_t                           children; ///< List of children blocks. May be empty.
        ::profiler::SerializedBlock*             node; ///< Pointer to serilized data (type, name, begin, end etc.)
        ::profiler::BlockStatistics* per_parent_stats; ///< Pointer to statistics for this block within the parent (may be nullptr for top-level blocks)
        ::profiler::BlockStatistics*  per_frame_stats; ///< Pointer to statistics for this block within the frame (may be nullptr for top-level blocks)
        ::profiler::BlockStatistics* per_thread_stats; ///< Pointer to statistics for this block within the bounds of all frames per current thread
        uint16_t                                depth; ///< Maximum number of sublevels (maximum children depth)

        BlocksTree()
            : node(nullptr)
            , per_parent_stats(nullptr)
            , per_frame_stats(nullptr)
            , per_thread_stats(nullptr)
            , depth(0)
        {

        }

        BlocksTree(This&& that) : BlocksTree()
        {
            make_move(::std::forward<This&&>(that));
        }

        This& operator = (This&& that)
        {
            make_move(::std::forward<This&&>(that));
            return *this;
        }

        ~BlocksTree()
        {
            release_stats(per_thread_stats);
            release_stats(per_parent_stats);
            release_stats(per_frame_stats);
        }

        bool operator < (const This& other) const
        {
            if (!node || !other.node)
                return false;
            return node->begin() < other.node->begin();
        }

        void shrink_to_fit()
        {
            //for (auto& child : children)
            //    child.shrink_to_fit();

            // shrink version 1:
            //children.shrink_to_fit();

            // shrink version 2:
            //children_t new_children;
            //new_children.reserve(children.size());
            //::std::move(children.begin(), children.end(), ::std::back_inserter(new_children));
            //new_children.swap(children);
        }

    private:

        BlocksTree(const This&) = delete;
        This& operator = (const This&) = delete;

        void make_move(This&& that)
        {
            if (per_thread_stats != that.per_thread_stats)
                release_stats(per_thread_stats);

            if (per_parent_stats != that.per_parent_stats)
                release_stats(per_parent_stats);

            if (per_frame_stats != that.per_frame_stats)
                release_stats(per_frame_stats);

            children = ::std::move(that.children);
            node = that.node;
            per_parent_stats = that.per_parent_stats;
            per_frame_stats = that.per_frame_stats;
            per_thread_stats = that.per_thread_stats;
            depth = that.depth;

            that.node = nullptr;
            that.per_parent_stats = nullptr;
            that.per_frame_stats = nullptr;
            that.per_thread_stats = nullptr;
        }

    }; // END of class BlocksTree.

    //////////////////////////////////////////////////////////////////////////

    class BlocksTreeRoot EASY_FINAL
    {
        typedef BlocksTreeRoot This;

    public:

        BlocksTree::children_t     children; ///< List of children indexes
        BlocksTree::children_t         sync; ///< List of context-switch events
        BlocksTree::children_t       events; ///< List of events indexes
        std::string             thread_name; ///< Name of this thread
        ::profiler::timestamp_t active_time; ///< Active time of this thread (sum of all children duration)
        ::profiler::thread_id_t   thread_id; ///< System Id of this thread
        uint16_t                      depth; ///< Maximum stack depth (number of levels)

        BlocksTreeRoot() : thread_name(""), active_time(0), thread_id(0), depth(0)
        {
        }

        BlocksTreeRoot(This&& that)
            : children(::std::move(that.children))
            , sync(::std::move(that.sync))
            , events(::std::move(that.events))
            , thread_name(::std::move(that.thread_name))
            , active_time(that.active_time)
            , thread_id(that.thread_id)
            , depth(that.depth)
        {
        }

        This& operator = (This&& that)
        {
            children = ::std::move(that.children);
            sync = ::std::move(that.sync);
            events = ::std::move(that.events);
            thread_name = ::std::move(that.thread_name);
            active_time = that.active_time;
            thread_id = that.thread_id;
            depth = that.depth;
            return *this;
        }

        inline bool got_name() const
        {
            return thread_name.front() != 0;
        }

        inline const char* name() const
        {
            return thread_name.c_str();
        }

        bool operator < (const This& other) const
        {
            return thread_id < other.thread_id;
        }

    private:

        BlocksTreeRoot(const This&) = delete;
        This& operator = (const This&) = delete;

    }; // END of class BlocksTreeRoot.

    typedef ::profiler::BlocksTree::blocks_t blocks_t;
    typedef ::std::unordered_map<::profiler::thread_id_t, ::profiler::BlocksTreeRoot, ::profiler::passthrough_hash> thread_blocks_tree_t;

    //////////////////////////////////////////////////////////////////////////

    class PROFILER_API SerializedData EASY_FINAL
    {
        char*  m_data;
        size_t m_size;

    public:

        SerializedData() : m_data(nullptr), m_size(0)
        {
        }

        SerializedData(SerializedData&& that) : m_data(that.m_data), m_size(that.m_size)
        {
            that.m_data = nullptr;
            that.m_size = 0;
        }

        ~SerializedData()
        {
            clear();
        }

        void set(size_t _size);
        void extend(size_t _size);

        SerializedData& operator = (SerializedData&& that)
        {
            set(that.m_data, that.m_size);
            that.m_data = nullptr;
            that.m_size = 0;
            return *this;
        }

        char* operator [] (uint64_t i)
        {
            return m_data + i;
        }

        size_t size() const
        {
            return m_size;
        }

        char* data()
        {
            return m_data;
        }

        const char* data() const
        {
            return m_data;
        }

        void clear()
        {
            set(nullptr, 0);
        }

        void swap(SerializedData& other)
        {
            char* d = other.m_data;
            size_t sz = other.m_size;

            other.m_data = m_data;
            other.m_size = m_size;

            m_data = d;
            m_size = sz;
        }

    private:

        void set(char* _data, size_t _size);

        SerializedData(const SerializedData&) = delete;
        SerializedData& operator = (const SerializedData&) = delete;

    }; // END of class SerializedData.

    //////////////////////////////////////////////////////////////////////////

    typedef ::std::vector<SerializedBlockDescriptor*> descriptors_list_t;

} // END of namespace profiler.

extern "C" PROFILER_API ::profiler::block_index_t fillTreesFromFile(::std::atomic<int>& progress, const char* filename,
                                                                    ::profiler::SerializedData& serialized_blocks,
                                                                    ::profiler::SerializedData& serialized_descriptors,
                                                                    ::profiler::descriptors_list_t& descriptors,
                                                                    ::profiler::blocks_t& _blocks,
                                                                    ::profiler::thread_blocks_tree_t& threaded_trees,
                                                                    bool gather_statistics);

inline ::profiler::block_index_t fillTreesFromFile(const char* filename, ::profiler::SerializedData& serialized_blocks,
                                                   ::profiler::SerializedData& serialized_descriptors, ::profiler::descriptors_list_t& descriptors,
                                                   ::profiler::blocks_t& _blocks, ::profiler::thread_blocks_tree_t& threaded_trees,
                                                   bool gather_statistics)
{
    ::std::atomic<int> progress = ATOMIC_VAR_INIT(0);
    return fillTreesFromFile(progress, filename, serialized_blocks, serialized_descriptors, descriptors, _blocks, threaded_trees, gather_statistics);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // PROFILER_READER____H
