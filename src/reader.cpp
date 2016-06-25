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

extern "C"{
    int fillTreesFromFile(const char* filename, thread_blocks_tree_t& threaded_trees, bool gather_statistics)
	{
		std::ifstream inFile(filename, std::fstream::binary);

		if (!inFile.is_open()){
			return -1;
		}

        StatsMap overall_statistics;

		int blocks_counter = 0;

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
			blocks_counter++;

            if (gather_statistics)
            {
                auto duration = tree.node->block()->duration();
                StatsMap::key_type key(tree.node->getBlockName());
                auto it = overall_statistics.find(key);
                if (it != overall_statistics.end())
                {
                    tree.total_statistics = it->second;

                    ++tree.total_statistics->calls_number;
                    tree.total_statistics->total_duration += duration;

                    if (duration > tree.total_statistics->max_duration)
                    {
                        tree.total_statistics->max_duration = duration;
                    }

                    if (duration < tree.total_statistics->min_duration)
                    {
                        tree.total_statistics->min_duration = duration;
                    }
                }
                else
                {
                    tree.total_statistics = new ::profiler::BlockStatistics(duration);
                    overall_statistics.insert(::std::make_pair(key, tree.total_statistics));
                }
            }

			if (root.children.empty()){
				root.children.push_back(std::move(tree));
			}
			else{
				BlocksTree& back = root.children.back();
				auto t1 = back.node->block()->getEnd();
				auto mt0 = tree.node->block()->getBegin();
				if (mt0 < t1)//parent - starts earlier than last ends
				{
					auto lower = std::lower_bound(root.children.begin(), root.children.end(), tree);

					std::move(lower, root.children.end(), std::back_inserter(tree.children));

					root.children.erase(lower, root.children.end());

				}

				root.children.push_back(std::move(tree));
			}


			//delete[] data;

		}

        if (gather_statistics)
        {
            for (auto it = overall_statistics.begin(), end = overall_statistics.end(); it != end; ++it)
            {
                it->second->average_duration = it->second->total_duration / it->second->calls_number;
            }

            // No need to delete BlockStatistics instances - they will be deleted on BlocksTree destructors
        }

		return blocks_counter;
	}
}