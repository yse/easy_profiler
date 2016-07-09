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
void update_statistics(StatsMap& _stats_map, ::profiler::SerilizedBlock* _current, ::profiler::BlockStatistics*& _stats)
{
    auto duration = _current->block()->duration();
    StatsMap::key_type key(_current->getBlockName());
    auto it = _stats_map.find(key);
    if (it != _stats_map.end())
    {
        // Update already existing statistics

        _stats = it->second; // write pointer to statistics into output (this is BlocksTree::total_statistics or BlocksTree::frame_statistics)

        ++_stats->calls_number; // update calls number of this block
        _stats->total_duration += duration; // update summary duration of all block calls

        //if (duration > _stats->max_duration_block->block()->duration())
        if (duration > _stats->max_duration)
        {
            // update max duration
            _stats->max_duration_block = _current;
            _stats->max_duration = duration;
        }

        //if (duration < _stats->min_duration_block->block()->duration())
        if (duration < _stats->min_duration)
        {
            // update min duraton
            _stats->min_duration_block = _current;
            _stats->min_duration = duration;
        }

        // average duration is calculated inside average_duration() method by dividing total_duration to the calls_number
    }
    else
    {
        // This is first time the block appear in the file.
        // Create new statistics.
        _stats = new ::profiler::BlockStatistics(duration, _current);
        _stats_map.insert(::std::make_pair(key, _stats));
    }
}

//////////////////////////////////////////////////////////////////////////

extern "C"{
    unsigned int fillTreesFromFile(const char* filename, thread_blocks_tree_t& threaded_trees, bool gather_statistics)
	{
		std::ifstream inFile(filename, std::fstream::binary);

		if (!inFile.is_open()){
            return 0;
		}

        StatsMap overall_statistics, frame_statistics;

		unsigned int blocks_counter = 0;

		while (!inFile.eof()){
			uint16_t sz = 0;
			inFile.read((char*)&sz, sizeof(sz));
			if (sz == 0)
			{
				inFile.read((char*)&sz, sizeof(sz));
				continue;
			}

            // TODO: use salloc::shared_allocator for allocation/deallocation safety
			char* data = new char[sz];
			inFile.read((char*)&data[0], sz);
			profiler::BaseBlockData* baseData = (profiler::BaseBlockData*)data;

			BlocksTree& root = threaded_trees[baseData->getThreadId()];

			BlocksTree tree;
			tree.node = new profiler::SerilizedBlock(sz, data);
			++blocks_counter;

            if (!root.children.empty())
            {
                BlocksTree& back = root.children.back();
                auto t1 = back.node->block()->getEnd();
                auto mt0 = tree.node->block()->getBegin();
                if (mt0 < t1)//parent - starts earlier than last ends
                {
                    auto lower = std::lower_bound(root.children.begin(), root.children.end(), tree);

                    std::move(lower, root.children.end(), std::back_inserter(tree.children));

                    root.children.erase(lower, root.children.end());

                    if (gather_statistics)
                    {
                        frame_statistics.clear();

                        //frame_statistics.reserve(tree.children.size());     // this gives slow-down on Windows
                        //frame_statistics.reserve(tree.children.size() * 2); // this gives no speed-up on Windows
                        // TODO: check this behavior on Linux

                        for (auto& child : tree.children)
                        {
                            tree.total_children_number += child.total_children_number;
                            update_statistics(frame_statistics, child.node, child.frame_statistics);
                            if (tree.sublevels < child.sublevels)
                                tree.sublevels = child.sublevels;
                        }

                        tree.total_children_number += static_cast<unsigned int>(tree.children.size());
                        ++tree.sublevels;
                    }
                }
            }

            root.children.push_back(std::move(tree));


			//delete[] data;

			

            if (gather_statistics)
            {
                BlocksTree& current = root.children.back();
                update_statistics(overall_statistics, current.node, current.total_statistics);
            }

		}

		if (gather_statistics)
		{
			for (auto& root_value : threaded_trees)
			{
                auto& root = root_value.second;

				frame_statistics.clear();
				for (auto& frame : root.children)
				{
                    root.total_children_number += frame.total_children_number;
                    update_statistics(frame_statistics, frame.node, frame.frame_statistics);
                    if (root.sublevels < frame.sublevels)
                        root.sublevels = frame.sublevels;
				}
                root.total_children_number += static_cast<unsigned int>(root.children.size());
                ++root.sublevels;
			}
		}
        else
        {
            for (auto& root_value : threaded_trees)
            {
                auto& root = root_value.second;
                for (auto& frame : root.children)
                {
                    root.total_children_number += frame.total_children_number;
                    if (root.sublevels < frame.sublevels)
                        root.sublevels = frame.sublevels;
                }
                root.total_children_number += static_cast<unsigned int>(root.children.size());
                ++root.sublevels;
            }
        }

        // No need to delete BlockStatistics instances - they will be deleted inside BlocksTree destructors

        return blocks_counter;
	}
}