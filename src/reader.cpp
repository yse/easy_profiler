#include "profiler/reader.h"
#include <fstream>
#include <iterator>
#include <algorithm>
#include <unordered_map>

//////////////////////////////////////////////////////////////////////////

#ifdef _WIN32

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

void update_statistics(StatsMap& _stats_map, ::profiler::SerilizedBlock* _current, ::profiler::BlockStatistics*& _stats)
{
    auto duration = _current->block()->duration();
    StatsMap::key_type key(_current->getBlockName());
    auto it = _stats_map.find(key);
    if (it != _stats_map.end())
    {
        _stats = it->second;

        ++_stats->calls_number;
        _stats->total_duration += duration;

        //if (duration > _stats->max_duration_block->block()->duration())
        if (duration > _stats->max_duration)
        {
            _stats->max_duration_block = _current;
            _stats->max_duration = duration;
        }

        //if (duration < _stats->min_duration_block->block()->duration())
        if (duration < _stats->min_duration)
        {
            _stats->min_duration_block = _current;
            _stats->min_duration = duration;
        }
    }
    else
    {
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
                    //auto lower = std::lower_bound(root.children.begin(), root.children.end(), tree);
                    /**/
                    auto rlower1 = ++root.children.rbegin();
                    for(; rlower1 != root.children.rend(); ++rlower1){
                        if(mt0 > rlower1->node->block()->getBegin())
                        {
                            break;
                        }
                    }
                    auto lower = rlower1.base();
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
                        }

                        tree.total_children_number += static_cast<unsigned int>(tree.children.size());
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
			for (auto& root : threaded_trees)
			{
				frame_statistics.clear();
				for (auto& frame : root.second.children)
				{
                    root.second.total_children_number += frame.total_children_number;
                    update_statistics(frame_statistics, frame.node, frame.frame_statistics);
				}
                root.second.total_children_number += static_cast<unsigned int>(root.second.children.size());
			}
		}
        else
        {
            for (auto& root : threaded_trees)
            {
                for (auto& frame : root.second.children)
                {
                    root.second.total_children_number += frame.total_children_number;
                }
                root.second.total_children_number += static_cast<unsigned int>(root.second.children.size());
            }
        }

        // No need to delete BlockStatistics instances - they will be deleted on BlocksTree destructors

        return blocks_counter;
	}
}