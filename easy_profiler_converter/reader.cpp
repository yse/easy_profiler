///std
#include <memory>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <iterator>

///this
#include "reader.h"

#include "hashed_cstr.h"

////from easy_profiler_core/reader.cpp/////

typedef uint64_t processid_t;

extern const uint32_t PROFILER_SIGNATURE = ('E' << 24) | ('a' << 16) | ('s' << 8) | 'y';

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

::profiler::block_index_t FileReader::readFile(const ::std::string &filename)
{
    ::std::ifstream file(filename, ::std::fstream::binary);
    if (!file.is_open())
    {
        errorMessage << "Can not open file " << filename;
        return 0;
    }

    ::std::stringstream inFile;

    inFile << file.rdbuf();
    file.close();

    uint32_t signature = 0;
    inFile.read((char*)&signature, sizeof(uint32_t));
    if (signature != PROFILER_SIGNATURE)
    {
        errorMessage << "Wrong signature " << signature << "\nThis is not EasyProfiler file/stream.";
        return 0;
    }

    m_version = 0;
    inFile.read((char*)&m_version, sizeof(uint32_t));
    if (!isCompatibleVersion(m_version))
    {
        errorMessage << "Incompatible version: v" << (m_version >> 24) << "." << ((m_version & 0x00ff0000) >> 16) << "." << (m_version & 0x0000ffff);
        return 0;
    }

    processid_t pid = 0;
    if (m_version > EASY_V_100)
    {
        if (m_version < EASY_V_130)
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
        errorMessage << "Profiled blocks number == 0";
        return 0;
    }

    uint64_t memory_size = 0;
    inFile.read((char*)&memory_size, sizeof(decltype(memory_size)));
    if (memory_size == 0)
    {
        errorMessage << "Wrong memory size == 0 for " << total_blocks_number << " blocks";
        return 0;
    }

    uint32_t total_descriptors_number = 0;
    inFile.read((char*)&total_descriptors_number, sizeof(uint32_t));
    if (total_descriptors_number == 0)
    {
        errorMessage << "Blocks description number == 0";
        return 0;
    }

    uint64_t descriptors_memory_size = 0;
    inFile.read((char*)&descriptors_memory_size, sizeof(decltype(descriptors_memory_size)));
    if (descriptors_memory_size == 0)
    {
        errorMessage << "Wrong memory size == 0 for " << total_descriptors_number << " blocks descriptions";
        return 0;
    }

    m_BlockDescriptors.reserve(total_descriptors_number);
    serialized_descriptors.set(descriptors_memory_size);

    ///read descriptors data
    uint64_t i = 0;
    while (!inFile.eof() && m_BlockDescriptors.size() < total_descriptors_number)
    {
        uint16_t sz = 0;
        inFile.read((char*)&sz, sizeof(sz));
        if (sz == 0)
        {
            m_BlockDescriptors.push_back(nullptr);
            continue;
        }

        char* data = serialized_descriptors[i];
        inFile.read(data, sz);
        auto descriptor = reinterpret_cast<::profiler::SerializedBlockDescriptor*>(data);

        m_BlockDescriptors.push_back(::std::make_shared<BlockDescriptor>());
        m_BlockDescriptors.back()->lineNumber = descriptor->line();
        m_BlockDescriptors.back()->blockId = descriptor->id();
        m_BlockDescriptors.back()->argbColor = descriptor->color();
        m_BlockDescriptors.back()->blockType = descriptor->type();
        m_BlockDescriptors.back()->status = descriptor->status();
        m_BlockDescriptors.back()->compileTimeName = descriptor->name();
        m_BlockDescriptors.back()->fileName = descriptor->file();

        i += sz;
    }

    serialized_blocks.set(memory_size);

    i = 0;
    uint32_t read_number = 0;
    ::profiler::block_index_t blocks_counter = 0;
    ::std::vector<char> name;

    const size_t thread_id_t_size = m_version < EASY_V_130 ? sizeof(uint32_t) : sizeof(::profiler::thread_id_t);
    ///read blocks info for every thread
    while (!inFile.eof() && read_number < total_blocks_number)
    {
        ::profiler::thread_id_t thread_id = 0;
        inFile.read((char*)&thread_id, thread_id_t_size);
        auto& root = m_BlocksTree[thread_id];

        uint16_t name_size = 0;
        inFile.read((char*)&name_size, sizeof(uint16_t));
        if (name_size != 0)
        {
            name.resize(name_size);
            inFile.read(name.data(), name_size);
            m_threadNames[thread_id] = name.data();
        }

        uint32_t blocks_number_in_thread = 0;
        inFile.read((char*)&blocks_number_in_thread, sizeof(decltype(blocks_number_in_thread)));
        auto threshold = read_number + blocks_number_in_thread;
        while (!inFile.eof() && read_number < threshold)
        {
            ++read_number;

            uint16_t sz = 0;
            inFile.read((char*)&sz, sizeof(sz));
            if (sz == 0)
            {
                errorMessage << "Bad CSwitch block size == 0";
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

                m_ContextSwitches.emplace_back();
                ::std::shared_ptr<ContextSwitchEvent>& cs = m_ContextSwitches.back();
                cs->switchName = baseData->name();
                cs->targetThreadId = baseData->tid();
                cs->beginTime = baseData->begin();
                cs->endTime = baseData->end();

                const auto block_index = blocks_counter++;
            }

        }

        if (inFile.eof())
            break;

        blocks_number_in_thread = 0;
        inFile.read((char*)&blocks_number_in_thread, sizeof(decltype(blocks_number_in_thread)));
        threshold = read_number + blocks_number_in_thread;
        ::std::vector<::std::shared_ptr<BlocksTreeNode>> siblings;
        ::std::shared_ptr<BlocksTreeNode> prev_node = ::std::make_shared<BlocksTreeNode>();
        prev_node->current_block = ::std::make_shared<BlockInfo>();

        ::std::shared_ptr<BlocksTreeNode> element;
        uint level = 0;

        while (!inFile.eof() && read_number < threshold)
        {
            element = ::std::make_shared<BlocksTreeNode>();
            element->current_block = ::std::make_shared<BlockInfo>();

            ++read_number;

            uint16_t sz = 0;
            inFile.read((char*)&sz, sizeof(sz));
            if (sz == 0)
            {
                errorMessage << "Bad block size == 0";
                return 0;
            }

            char* data = serialized_blocks[i];
            inFile.read(data, sz);
            i += sz;
            auto baseData = reinterpret_cast<::profiler::SerializedBlock*>(data);
            if (baseData->id() >= total_descriptors_number)
            {
                errorMessage << "Bad block id == " << baseData->id();
                return 0;
            }
            element->current_block->blockId = baseData->id();

            auto desc = m_BlockDescriptors[baseData->id()];
            if (desc == nullptr)
            {
                errorMessage << "Bad block id == " << baseData->id() << ". Description is null.";
                return 0;
            }
            element->current_block->descriptor = m_BlockDescriptors[baseData->id()];

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

                element->current_block->beginTime = baseData->begin();
                element->current_block->endTime = baseData->end();

                ///is sibling?
                if(element->current_block->beginTime >= prev_node->current_block->endTime)
                {
                    prev_node = element;
                    ///all siblings
                    root.children.push_back(element);
                }
                else
                {
                    auto iter = root.children.begin();
                    for(;iter != root.children.end(); ++iter)
                    {
                        if(iter->get()->current_block->beginTime >= element->current_block->beginTime)
                        {
                            ::std::move(iter,root.children.end(),::std::back_inserter(element->children));
                            root.children.erase(std::remove(begin(root.children), end(root.children), nullptr),
                                                end(root.children));
                            root.children.emplace_back(element);
                            break;
                        }
                    }
                }
                const auto block_index = blocks_counter++;
                ///TODO: make optimization BLOCK_TYPE_EVENT. leave it here commented.
//                if (desc->blockType == ::profiler::BLOCK_TYPE_EVENT)
//                {
//                    root.children.emplace_back(element);
//                }
            }
        }
    }
    return blocks_counter;
}

const thread_blocks_tree_t &FileReader::getBlocksTreeData()
{
    return m_BlocksTree;
}

const ::std::string &FileReader::getThreadName(uint64_t threadId)
{
    return m_threadNames[threadId];
}

uint32_t FileReader::getVersion()
{
    return m_version;
}

const context_switches_t &FileReader::getContextSwitches()
{
    return m_ContextSwitches;
}

