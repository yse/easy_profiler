///std
#include <memory>
#include <algorithm>
#include <iostream>
///this
#include "reader.h"

#include "hashed_cstr.h"

////from easy_profiler_core/reader.cpp/////

typedef uint64_t processid_t;

extern const uint32_t PROFILER_SIGNATURE;
extern const uint32_t EASY_CURRENT_VERSION;

# define EASY_VERSION_INT(v_major, v_minor, v_patch) ((static_cast<uint32_t>(v_major) << 24) | (static_cast<uint32_t>(v_minor) << 16) | static_cast<uint32_t>(v_patch))
const uint32_t MIN_COMPATIBLE_VERSION = EASY_VERSION_INT(0, 1, 0); ///< minimal compatible version (.prof file format was not changed seriously since this version)
const uint32_t EASY_V_100 = EASY_VERSION_INT(1, 0, 0); ///< in v1.0.0 some additional data were added into .prof file
const uint32_t EASY_V_130 = EASY_VERSION_INT(1, 3, 0); ///< in v1.3.0 changed sizeof(thread_id_t) uint32_t -> uint64_t
# undef EASY_VERSION_INT

const uint64_t TIME_FACTOR = 1000000000ULL;

// TODO: use 128 bit integer operations for better accuracy
#define EASY_USE_FLOATING_POINT_CONVERSION

#ifdef EASY_USE_FLOATING_POINT_CONVERSION

// Suppress warnings about double to uint64 conversion
# ifdef _MSC_VER
#  pragma warning(disable:4244)
# elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wconversion"
#  pragma GCC diagnostic ignored "-Wsign-conversion"
# elif defined(__clang__)
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wconversion"
#  pragma clang diagnostic ignored "-Wsign-conversion"
# endif

# define EASY_CONVERT_TO_NANO(t, freq, factor) t *= factor

#else

# define EASY_CONVERT_TO_NANO(t, freq, factor) t *= TIME_FACTOR; t /= freq

#endif

inline bool isCompatibleVersion(uint32_t _version)
{
    return _version >= MIN_COMPATIBLE_VERSION;
}

#ifdef EASY_PROFILER_HASHED_CSTR_DEFINED

typedef ::std::unordered_map<::profiler::block_id_t, ::profiler::BlockStatistics*, ::profiler::passthrough_hash<::profiler::block_id_t> > StatsMap;

/** \note It is absolutely safe to use hashed_cstr (which simply stores pointer) because std::unordered_map,
which uses it as a key, exists only inside fillTreesFromFile function. */
typedef ::std::unordered_map<::profiler::hashed_cstr, ::profiler::block_id_t> IdMap;

typedef ::std::unordered_map<::profiler::hashed_cstr, ::profiler::BlockStatistics*> CsStatsMap;

#else

// TODO: Create optimized version of profiler::hashed_cstr for Linux too.
typedef ::std::unordered_map<::profiler::block_id_t, ::profiler::BlockStatistics*, ::profiler::passthrough_hash<::profiler::block_id_t> > StatsMap;
typedef ::std::unordered_map<::profiler::hashed_stdstring, ::profiler::block_id_t> IdMap;
typedef ::std::unordered_map<::profiler::hashed_stdstring, ::profiler::BlockStatistics*> CsStatsMap;

#endif

/// end from easy_profiler_core/reader.cpp/////

using namespace profiler::reader;
using namespace std;

void FileReader::readFile(const string &filename)
{
    fillTreesFromFile(filename.c_str(), serialized_blocks, serialized_descriptors, m_descriptors, m_blocks,
                      m_threaded_trees, descriptorsNumberInFile, version, true, errorMessage);
    prepareData();
}

const FileReader::TreeNodes &FileReader::getBlocks()
{
    return m_BlocksTree;
}

const FileReader::Events &FileReader::getEvents()
{
    return m_events;
}

const FileReader::ContextSwitches &FileReader::getContextSwitches()
{
    return m_ContextSwitches;
}

void FileReader::prepareData()
{
    vector<uint32_t> tmp_indexes_vector;

    for(auto& kv : m_threaded_trees)
    {
        //children & events are already sorted !!!
        ::std::set_difference(kv.second.children.begin(),kv.second.children.end(),
                              kv.second.events.begin(),kv.second.events.end(),
                              ::std::back_inserter(tmp_indexes_vector));
        for(auto& value : tmp_indexes_vector)
        {
            ::std::shared_ptr<BlocksTreeNode> element = ::std::make_shared<BlocksTreeNode>();
            element->current_block = ::std::make_shared<BlockInfo>();
            element->parent = nullptr;
            element->current_block->thread_name = kv.second.thread_name;
            prepareBlocksInfo(element, value);
            m_BlocksTree.push_back(::std::move(element));
        }
        tmp_indexes_vector.clear();

        prepareEventsInfo(kv.second.events);
        prepareCSInfo(kv.second.sync);
    }
}

void FileReader::prepareBlocksInfo(::std::shared_ptr<BlocksTreeNode> &element, uint32_t Id)
{
    getBlockInfo(element->current_block, Id);

    ///block's children info
    for(auto& value : m_blocks[element->current_block->blockId].children)
    {
        ::std::shared_ptr<BlocksTreeNode> btElement = ::std::make_shared<BlocksTreeNode>();
        btElement->current_block = ::std::make_shared<BlockInfo>();
        btElement->parent = element.get();
        btElement->current_block->thread_name = element->current_block->thread_name;

        element->children.push_back(btElement);
        prepareBlocksInfo(btElement,value);
    }
}

void FileReader::prepareEventsInfo(const ::std::vector<uint32_t>& events)
{
    for(auto Id : events)
    {
        m_events.push_back(::std::make_shared<BlockInfo>());
        getBlockInfo(m_events.back(), Id);
    }
}

void FileReader::prepareCSInfo(const::std::vector<uint32_t> &cs)
{
    for(auto Id : cs)
    {
        m_ContextSwitches.push_back(::std::make_shared<ContextSwitchEvent>());
        m_ContextSwitches.back()->switchName = m_blocks[Id].cs->name();
        m_ContextSwitches.back()->targetThreadId = m_blocks[Id].cs->tid();
        m_ContextSwitches.back()->beginTime = m_blocks[Id].cs->begin();
        m_ContextSwitches.back()->endTime = m_blocks[Id].cs->end();
    }
}

void FileReader::getBlockInfo(shared_ptr<BlockInfo> &current_block, uint32_t Id)
{
    ///block info
    current_block->beginTime = m_blocks[Id].node->begin();
    current_block->endTime = m_blocks[Id].node->end();
    current_block->blockId = m_blocks[Id].node->id();
    current_block->runTimeBlockName = m_blocks[Id].node->name();

    ///descriptor
    current_block->descriptor = ::std::make_shared<BlockDescriptor>();
    current_block->descriptor->argbColor = m_descriptors.at(current_block->blockId)->color();
    current_block->descriptor->lineNumber = m_descriptors.at(current_block->blockId)->line();
    current_block->descriptor->blockId = m_descriptors.at(current_block->blockId)->id();
    current_block->descriptor->blockType = m_descriptors.at(current_block->blockId)->type();
    current_block->descriptor->status = m_descriptors.at(current_block->blockId)->status();
    current_block->descriptor->compileTimeName = m_descriptors.at(current_block->blockId)->name();
    current_block->descriptor->fileName = m_descriptors.at(current_block->blockId)->file();
}


::profiler::block_index_t FileReader::fillTreesFromStream2(::std::stringstream& inFile,
                                                           ::std::vector<::std::shared_ptr<BlockDescriptor>>& descriptors,
                                                           ::profiler::blocks_t& blocks,
                                                           ::profiler::thread_blocks_tree_t& threaded_trees,
                                                           uint32_t& version,
                                                           ::std::stringstream& _log)
{
    uint32_t signature = 0;
    inFile.read((char*)&signature, sizeof(uint32_t));
    if (signature != PROFILER_SIGNATURE)
    {
        _log << "Wrong signature " << signature << "\nThis is not EasyProfiler file/stream.";
        return 0;
    }

    version = 0;
    inFile.read((char*)&version, sizeof(uint32_t));
    if (!isCompatibleVersion(version))
    {
        _log << "Incompatible version: v" << (version >> 24) << "." << ((version & 0x00ff0000) >> 16) << "." << (version & 0x0000ffff);
        return 0;
    }

    processid_t pid = 0;
    if (version > EASY_V_100)
    {
        if (version < EASY_V_130)
        {
            uint32_t old_pid = 0;
            inFile.read((char*)&old_pid, sizeof(uint32_t));
            pid = old_pid;
        }
        else
        {
            inFile.read((char*)&pid, sizeof(processid_t));
        }
    }

    int64_t file_cpu_frequency = 0LL;
    inFile.read((char*)&file_cpu_frequency, sizeof(int64_t));
    uint64_t cpu_frequency = file_cpu_frequency;
    const double conversion_factor = static_cast<double>(TIME_FACTOR) / static_cast<double>(cpu_frequency);

    ::profiler::timestamp_t begin_time = 0ULL;
    ::profiler::timestamp_t end_time = 0ULL;
    inFile.read((char*)&begin_time, sizeof(::profiler::timestamp_t));
    inFile.read((char*)&end_time, sizeof(::profiler::timestamp_t));
    if (cpu_frequency != 0)
    {
        EASY_CONVERT_TO_NANO(begin_time, cpu_frequency, conversion_factor);
        EASY_CONVERT_TO_NANO(end_time, cpu_frequency, conversion_factor);
    }

    uint32_t total_blocks_number = 0;
    inFile.read((char*)&total_blocks_number, sizeof(uint32_t));
    if (total_blocks_number == 0)
    {
        _log << "Profiled blocks number == 0";
        return 0;
    }

    uint64_t memory_size = 0;
    inFile.read((char*)&memory_size, sizeof(decltype(memory_size)));
    if (memory_size == 0)
    {
        _log << "Wrong memory size == 0 for " << total_blocks_number << " blocks";
        return 0;
    }

    uint32_t total_descriptors_number = 0;
    inFile.read((char*)&total_descriptors_number, sizeof(uint32_t));
    if (total_descriptors_number == 0)
    {
        _log << "Blocks description number == 0";
        return 0;
    }

    uint64_t descriptors_memory_size = 0;
    inFile.read((char*)&descriptors_memory_size, sizeof(decltype(descriptors_memory_size)));
    if (descriptors_memory_size == 0)
    {
        _log << "Wrong memory size == 0 for " << total_descriptors_number << " blocks descriptions";
        return 0;
    }

    descriptors.reserve(total_descriptors_number);

    ///read descriptors data
    ::std::make_shared<BlockDescriptor>();
    uint64_t i = 0;
    while (!inFile.eof() && descriptors.size() < total_descriptors_number)
    {
        uint16_t sz = 0;
        inFile.read((char*)&sz, sizeof(sz));
        if (sz == 0)
        {
            descriptors.push_back(nullptr);
            continue;
        }

        char* data = serialized_descriptors[i];
        inFile.read(data, sz);
        auto descriptor = reinterpret_cast<::profiler::SerializedBlockDescriptor*>(data);

        descriptors.push_back(::std::make_shared<BlockDescriptor>());
        descriptors.back()->lineNumber = descriptor->line();
        descriptors.back()->blockId = descriptor->id();
        descriptors.back()->argbColor = descriptor->color();
        descriptors.back()->blockType = descriptor->type();
        descriptors.back()->status = descriptor->status();
        descriptors.back()->compileTimeName = descriptor->name();
        descriptors.back()->fileName = descriptor->file();

        i += sz;
    }
    ////////////////////some magic stuff(how to get rid of it???)/////////////////
    typedef ::std::unordered_map<::profiler::thread_id_t, StatsMap, ::profiler::passthrough_hash<::profiler::thread_id_t> > PerThreadStats;
    PerThreadStats parent_statistics;
    IdMap identification_table;

    blocks.reserve(total_blocks_number);
    serialized_blocks.set(memory_size);

    i = 0;
    uint32_t read_number = 0;
    ::profiler::block_index_t blocks_counter = 0;
    ::std::vector<char> name;

    const size_t thread_id_t_size = version < EASY_V_130 ? sizeof(uint32_t) : sizeof(::profiler::thread_id_t);

    while (!inFile.eof() && read_number < total_blocks_number)
    {
        ::profiler::thread_id_t thread_id = 0;
        inFile.read((char*)&thread_id, thread_id_t_size);

        auto& root = threaded_trees[thread_id];

        uint16_t name_size = 0;
        inFile.read((char*)&name_size, sizeof(uint16_t));
        if (name_size != 0)
        {
            name.resize(name_size);
            inFile.read(name.data(), name_size);
            root.thread_name = name.data();
        }

        CsStatsMap per_thread_statistics_cs;

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
            {
                _log << "Bad CSwitch block size == 0";
                return 0;
            }

            char* data = serialized_blocks[i];
            inFile.read(data, sz);
            i += sz;
            auto baseData = reinterpret_cast<::profiler::SerializedCSwitch*>(data);
            auto t_begin = reinterpret_cast<::profiler::timestamp_t*>(data);
            auto t_end = t_begin + 1;

            if (cpu_frequency != 0)
            {
                EASY_CONVERT_TO_NANO(*t_begin, cpu_frequency, conversion_factor);
                EASY_CONVERT_TO_NANO(*t_end, cpu_frequency, conversion_factor);
            }

            if (*t_end > begin_time)
            {
                if (*t_begin < begin_time)
                    *t_begin = begin_time;

                blocks.emplace_back();
                ::profiler::BlocksTree& tree = blocks.back();
                tree.cs = baseData;
                const auto block_index = blocks_counter++;

                root.wait_time += baseData->duration();
                root.sync.emplace_back(block_index);

            }

        }

        if (inFile.eof())
            break;

        StatsMap per_thread_statistics;

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
            {
                _log << "Bad block size == 0";
                return 0;
            }

            char* data = serialized_blocks[i];
            inFile.read(data, sz);
            i += sz;
            auto baseData = reinterpret_cast<::profiler::SerializedBlock*>(data);
            if (baseData->id() >= total_descriptors_number)
            {
                _log << "Bad block id == " << baseData->id();
                return 0;
            }

            auto desc = descriptors[baseData->id()];
            if (desc == nullptr)
            {
                _log << "Bad block id == " << baseData->id() << ". Description is null.";
                return 0;
            }

            auto t_begin = reinterpret_cast<::profiler::timestamp_t*>(data);
            auto t_end = t_begin + 1;

            if (cpu_frequency != 0)
            {
                EASY_CONVERT_TO_NANO(*t_begin, cpu_frequency, conversion_factor);
                EASY_CONVERT_TO_NANO(*t_end, cpu_frequency, conversion_factor);
            }

            if (*t_end >= begin_time)
            {
                if (*t_begin < begin_time)
                    *t_begin = begin_time;

                blocks.emplace_back();
                ::profiler::BlocksTree& tree = blocks.back();
                tree.node = baseData;
                const auto block_index = blocks_counter++;

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
                        if (descriptors.capacity() == descriptors.size())
                            descriptors.reserve((descriptors.size() * 3) >> 1);
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


                            for (auto i : tree.children)
                            {
                                const auto& child = blocks[i];
                                if (tree.depth < child.depth)
                                    tree.depth = child.depth;
                            }


                        if (tree.depth == 254)
                        {
                            // 254 because we need 1 additional level for root (thread).
                            // In other words: real stack depth = 1 root block + 254 children

                            if (*tree.node->name() != 0)
                                _log << "Stack depth exceeded value of 254\nfor block \"" << desc->compileTimeName << "\"";
                            else
                                _log << "Stack depth exceeded value of 254\nfor block \"" << desc->compileTimeName << "\"\nfrom file \"" << desc->fileName << "\":" << desc->lineNumber;

                            return 0;
                        }

                        ++tree.depth;
                    }
                }

                ++root.blocks_number;
                root.children.emplace_back(block_index);// ::std::move(tree));
                if (desc->blockType == ::profiler::BLOCK_TYPE_EVENT)
                    root.events.emplace_back(block_index);

            }
        }

    }

    ////////////////////end of some magic stuff/////////////////



    return blocks_counter;
}
