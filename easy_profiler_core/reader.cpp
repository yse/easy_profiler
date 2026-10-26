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
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2019  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
*                   :
*                   : The MIT License
*                   :
*                   : Permission is hereby granted, free of charge, to any person obtaining a copy
*                   : of this software and associated documentation files (the "Software"), to deal
*                   : in the Software without restriction, including without limitation the rights
*                   : to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*                   : of the Software, and to permit persons to whom the Software is furnished
*                   : to do so, subject to the following conditions:
*                   :
*                   : The above copyright notice and this permission notice shall be included in all
*                   : copies or substantial portions of the Software.
*                   :
*                   : THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
*                   : INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
*                   : PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
*                   : LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*                   : TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
*                   : USE OR OTHER DEALINGS IN THE SOFTWARE.
*                   :
*                   : The Apache License, Version 2.0 (the "License")
*                   :
*                   : You may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
************************************************************************/

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <fstream>
#include <future>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <thread>

#include <easy/reader.h>
#include <easy/profiler.h>

#include "alignment_helpers.h"
#include "hashed_cstr.h"
#include "timestamp_scaling.h"

//////////////////////////////////////////////////////////////////////////

extern const uint32_t EASY_PROFILER_SIGNATURE;
extern const uint32_t EASY_PROFILER_FORMAT_VERSION;

# define EASY_VERSION_INT(v_major, v_minor, v_patch) ((static_cast<uint32_t>(v_major) << 24) | \
                                                      (static_cast<uint32_t>(v_minor) << 16) | \
                                                       static_cast<uint32_t>(v_patch))

EASY_CONSTEXPR uint32_t MIN_COMPATIBLE_VERSION = EASY_VERSION_INT(0, 1, 0); ///< minimal compatible version (.prof file format was not changed seriously since this version)
EASY_CONSTEXPR uint32_t EASY_V_100 = EASY_VERSION_INT(1, 0, 0); ///< in v1.0.0 some additional data were added into .prof file
EASY_CONSTEXPR uint32_t EASY_V_130 = EASY_VERSION_INT(1, 3, 0); ///< in v1.3.0 changed sizeof(thread_id_t) uint32_t -> uint64_t
EASY_CONSTEXPR uint32_t EASY_V_200 = EASY_VERSION_INT(2, 0, 0); ///< in v2.0.0 file header was slightly rearranged
EASY_CONSTEXPR uint32_t EASY_V_210 = EASY_VERSION_INT(2, 1, 0); ///< in v2.1.0 user bookmarks were added

# undef EASY_VERSION_INT

EASY_CONSTEXPR uint64_t TIME_FACTOR = 1000000000ULL;

//////////////////////////////////////////////////////////////////////////

static bool isCompatibleVersion(uint32_t _version)
{
    // Patch releases in one major.minor line use the same on-disk layout. Do
    // not make a 2.1.0 reader reject a 2.1.1 profile solely because the product
    // patch changed; reject a newer format line (for example 2.2) instead.
    const uint32_t formatLineMask = 0xffff0000U;
    return _version >= MIN_COMPATIBLE_VERSION &&
           (_version & formatLineMask) <= (EASY_PROFILER_FORMAT_VERSION & formatLineMask);
}

//////////////////////////////////////////////////////////////////////////

namespace {

#if defined(_MSC_VER) && _MSC_VER <= 1800
struct async_result_t {};
#define EASY_FINISH_ASYNC async_result_t dummy_async_result; return dummy_async_result
#else
using async_result_t = void;
#define EASY_FINISH_ASYNC 
#endif

using async_future = std::future<async_result_t>;

// Tasks queued on ReaderThreadPool capture their caller's locals by reference, so every
// future has to be waited on before those locals die. Retrieving them in a loop is not
// enough: an exception from any get(), or from async() itself, would leave the remaining
// tasks running against locals that the unwinding then destroys.
struct AsyncResults
{
    std::vector<async_future> futures;

    ~AsyncResults()
    {
        for (auto& future : futures)
        {
            if (!future.valid())
                continue;

            try
            {
                future.get();
            }
            catch (...)
            {
                // A task's exception has already been reported by the get() loop below, and
                // a destructor must not throw a second one.
            }
        }
    }
};

template <class T>
struct Counter
{
    T count = 0;
};

struct Stats
{
    profiler::BlockStatistics* stats;
    std::map<profiler::timestamp_t, Counter<size_t> > durations;

    Stats(profiler::BlockStatistics* stats_ptr, profiler::timestamp_t duration) EASY_NOEXCEPT
        : stats(stats_ptr)
    {
        durations[duration].count = 1;
    }

    Stats(Stats&& another) EASY_NOEXCEPT
        : stats(another.stats)
        , durations(std::move(another.durations))
    {
        another.stats = nullptr;
    }

    Stats(const Stats&) = delete;
};

class ReaderThreadPool
{
    std::vector<std::thread>                          m_threads;
    std::deque<std::packaged_task<async_result_t()> > m_tasks;
    std::mutex                                        m_mutex;
    std::condition_variable                           m_cv;
    const size_t                                      m_maxThreads;
    bool                                              m_stop; // guarded by m_mutex

public:

    ReaderThreadPool()
        : m_maxThreads(std::max(1U, std::thread::hardware_concurrency()))
        , m_stop(false)
    {
        m_threads.reserve(m_maxThreads);
    }

    ~ReaderThreadPool()
    {
        {
            // Written under the mutex so it cannot slip between a worker's
            // predicate check and its wait, where the notification would be lost.
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
        for (auto& t : m_threads)
        {
            t.join();
        }
    }

    size_t maxThreads() const
    {
        return m_maxThreads;
    }

    async_future async(std::function<async_result_t()> func)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_tasks.emplace_back(std::move(func));

        async_future future;
        try
        {
            future = m_tasks.back().get_future();
            startNewThreadIfNeeded();
        }
        catch (...)
        {
            // A worker needs m_mutex to take a task, so this one is still ours to withdraw.
            // It has to go: the caller leaves without a future, so nothing would ever wait
            // for it, and it captures locals that the caller's unwinding is about to destroy.
            m_tasks.pop_back();
            throw;
        }

        lock.unlock();
        m_cv.notify_one();
        return future;
    }

private:

    void startNewThreadIfNeeded()
    {
        if (m_threads.size() < m_maxThreads && m_threads.size() < m_tasks.size())
        {
            m_threads.emplace_back(&ReaderThreadPool::worker, this);
        }
    }

    void worker()
    {
        while (true)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_tasks.empty() || m_stop; });
            if (m_tasks.empty())
            {
                break;
            }

            auto task = std::move(m_tasks.front());
            m_tasks.pop_front();
            lock.unlock();

            task();
        }
    }
};

} // end of namespace <noname>.

namespace profiler {

    using stats_map_t = std::unordered_map<profiler::block_id_t, Stats, estd::hash<profiler::block_id_t> >;

    SerializedData::SerializedData() : m_size(0), m_data(nullptr)
    {
    }

    SerializedData::SerializedData(SerializedData&& that) : m_size(that.m_size), m_data(that.m_data)
    {
        that.m_size = 0;
        that.m_data = nullptr;
    }

    SerializedData::~SerializedData()
    {
        clear();
    }

    void SerializedData::set(char* _data, uint64_t _size)
    {
        delete [] m_data;
        m_size = _size;
        m_data = _data;
    }

    void SerializedData::set(uint64_t _size)
    {
        if (_size != 0)
            set(new char[_size], _size);
        else
            set(nullptr, 0);
    }

    void SerializedData::extend(uint64_t _size)
    {
        auto oldsize = m_size;
        auto olddata = m_data;

        m_size = oldsize + _size;
        m_data = new char[m_size];

        if (olddata != nullptr) {
            memcpy(m_data, olddata, oldsize);
            delete [] olddata;
        }
    }

    SerializedData& SerializedData::operator = (SerializedData&& that)
    {
        set(that.m_data, that.m_size);
        that.m_size = 0;
        that.m_data = nullptr;
        return *this;
    }

    char* SerializedData::operator [] (uint64_t i)
    {
        return m_data + i;
    }

    const char* SerializedData::operator [] (uint64_t i) const
    {
        return m_data + i;
    }

    bool SerializedData::empty() const
    {
        return m_size == 0;
    }

    uint64_t SerializedData::size() const
    {
        return m_size;
    }

    char* SerializedData::data()
    {
        return m_data;
    }

    const char* SerializedData::data() const
    {
        return m_data;
    }

    void SerializedData::clear()
    {
        set(nullptr, 0);
    }

    void SerializedData::swap(SerializedData& other)
    {
        char* d = other.m_data;
        const auto sz = other.m_size;

        other.m_data = m_data;
        other.m_size = m_size;

        m_data = d;
        m_size = sz;
    }

    extern "C" PROFILER_API void release_stats(BlockStatistics*& _stats)
    {
        if (_stats == nullptr)
            return;

        if (--_stats->calls_number == 0)
            delete _stats;

        _stats = nullptr;
    }

} // end of namespace profiler.

//////////////////////////////////////////////////////////////////////////

using IdMap = std::unordered_map<profiler::hashed_stdstring, profiler::block_id_t>;
using CsStatsMap = std::unordered_map<profiler::string_with_hash, Stats>;

//////////////////////////////////////////////////////////////////////////

/** \brief Updates statistics for a profiler block.

\param _stats_map Storage of statistics for blocks.
\param _current Pointer to the current block.
\param _stats Reference to the variable where pointer to the block statistics must be written.

\note All blocks with similar name have the same pointer to statistics information.

\note As all profiler block keeps a pointer to it's statistics, all similar blocks
automatically receive statistics update.

*/
static profiler::BlockStatistics* update_statistics(
    profiler::stats_map_t& _stats_map,
    const profiler::BlocksTree& _current,
    profiler::block_index_t _current_index,
    profiler::block_index_t _parent_index,
    const profiler::blocks_t& _blocks,
    bool _calculate_children = true
) {
    auto duration = _current.node->duration();
    //StatsMap::key_type key(_current.node->name());
    //auto it = _stats_map.find(key);
    auto it = _stats_map.find(_current.node->id());
    if (it != _stats_map.end())
    {
        // Update already existing statistics

        // write pointer to statistics into output (this is BlocksTree:: per_thread_stats or per_parent_stats or per_frame_stats)
        auto stats = it->second.stats;
        auto& durations = it->second.durations;
        ++durations[duration].count;

        ++stats->calls_number; // update calls number of this block
        stats->total_duration += duration; // update summary duration of all block calls

        if (_calculate_children)
        {
            for (auto i : _current.children)
                stats->total_children_duration += _blocks[i].node->duration();
        }

        if (duration > _blocks[stats->max_duration_block].node->duration())
        {
            // update max duration
            stats->max_duration_block = _current_index;
            //stats->max_duration = duration;
        }

        if (duration < _blocks[stats->min_duration_block].node->duration())
        {
            // update min duraton
            stats->min_duration_block = _current_index;
            //stats->min_duration = duration;
        }

        // average duration is calculated inside average_duration() method by dividing total_duration to the calls_number

        return stats;
    }

    // This is first time the block appear in the file.
    // Create new statistics.
    auto stats = new profiler::BlockStatistics(duration, _current_index, _parent_index);
    //_stats_map.emplace(key, stats);
    _stats_map.emplace(_current.node->id(), Stats {stats, duration});

    if (_calculate_children)
    {
        for (auto i : _current.children)
            stats->total_children_duration += _blocks[i].node->duration();
    }

    return stats;
}

static profiler::BlockStatistics* update_statistics(
    CsStatsMap& _stats_map,
    const profiler::BlocksTree& _current,
    profiler::block_index_t _current_index,
    profiler::block_index_t _parent_index,
    const profiler::blocks_t& _blocks,
    bool _calculate_children = true
) {
    auto duration = _current.node->duration();
    CsStatsMap::key_type key(_current.node->name());
    auto it = _stats_map.find(key);
    if (it != _stats_map.end())
    {
        // Update already existing statistics

        // write pointer to statistics into output (this is BlocksTree:: per_thread_stats or per_parent_stats or per_frame_stats)
        auto stats = it->second.stats;
        auto& durations = it->second.durations;

        ++durations[duration].count;

        ++stats->calls_number; // update calls number of this block
        stats->total_duration += duration; // update summary duration of all block calls

        if (_calculate_children)
        {
            for (auto i : _current.children)
                stats->total_children_duration += _blocks[i].node->duration();
        }

        if (duration > _blocks[stats->max_duration_block].node->duration())
        {
            // update max duration
            stats->max_duration_block = _current_index;
            //stats->max_duration = duration;
        }

        if (duration < _blocks[stats->min_duration_block].node->duration())
        {
            // update min duraton
            stats->min_duration_block = _current_index;
            //stats->min_duration = duration;
        }

        // average duration is calculated inside average_duration() method by dividing total_duration to the calls_number

        return stats;
    }

    // This is first time the block appear in the file.
    // Create new statistics.
    auto stats = new profiler::BlockStatistics(duration, _current_index, _parent_index);
    _stats_map.emplace(key, Stats {stats, duration});

    if (_calculate_children)
    {
        for (auto i : _current.children)
            stats->total_children_duration += _blocks[i].node->duration();
    }

    return stats;
}

template <class TStatsMapIterator>
static void calculate_medians(TStatsMapIterator begin, TStatsMapIterator end)
{
    for (auto it = begin; it != end; ++it)
    {
        auto& durations = it->second.durations;
        if (durations.empty())
        {
            continue;
        }

        size_t total_count = 0;
        for (auto& kv : durations)
        {
            total_count += kv.second.count;
        }

        auto stats = it->second.stats;
        if (total_count & 1)
        {
            const auto index = total_count >> 1;
            size_t i = 0;
            for (auto& kv : durations)
            {
                const auto count = kv.second.count;

                i += count;
                if (i < index)
                {
                    continue;
                }

                stats->median_duration = kv.first;
                break;
            }
        }
        else
        {
            const auto index2 = total_count >> 1;
            const auto index1 = index2 - 1;

            size_t i = 0;
            bool i1 = false;
            for (auto& kv : durations)
            {
                const auto count = kv.second.count;

                i += count;
                if (i < index1)
                {
                    continue;
                }

                if (!i1)
                {
                    i1 = true;
                    stats->median_duration = kv.first;
                }

                if (i < index2)
                {
                    continue;
                }

                stats->median_duration += kv.first;
                stats->median_duration >>= 1;

                break;
            }
        }

        decltype(it->second.durations) dummy;
        dummy.swap(durations);
    }
}

template <class TStatsMap>
static void calculate_medians_async(ReaderThreadPool& pool, TStatsMap& stats_map)
{
    if (stats_map.empty())
    {
        return;
    }

    if (stats_map.size() < 1000)
    {
        calculate_medians(stats_map.begin(), stats_map.end());
        return;
    }

    const auto threads_count = std::min(pool.maxThreads(), stats_map.size());
    const auto count_per_thread = stats_map.size() / threads_count;

    AsyncResults results;
    results.futures.reserve(threads_count);

    for (size_t i = 1; i <= threads_count; ++i)
    {
        results.futures.emplace_back(pool.async([i, threads_count, count_per_thread, &stats_map] () -> async_result_t
        {
            auto begin = stats_map.begin();
            const auto advance_count = (i - 1) * count_per_thread;
            std::advance(begin, advance_count);

            auto end = begin;
            if (i == threads_count)
            {
                end = stats_map.end();
            }
            else
            {
                std::advance(end, count_per_thread);
            }

            calculate_medians(begin, end);

            EASY_FINISH_ASYNC; // MSVC 2013 hack
        }));
    }

    for (auto& result : results.futures)
    {
        if (result.valid())
        {
            result.get();
        }
    }
}

//////////////////////////////////////////////////////////////////////////

static void update_statistics_recursive(profiler::stats_map_t& _stats_map, profiler::BlocksTree& _current, profiler::block_index_t _current_index, profiler::block_index_t _parent_index, profiler::blocks_t& _blocks)
{
    _current.per_frame_stats = update_statistics(_stats_map, _current, _current_index, _parent_index, _blocks, false);
    for (auto i : _current.children)
    {
        _current.per_frame_stats->total_children_duration += _blocks[i].node->duration();
        update_statistics_recursive(_stats_map, _blocks[i], i, _parent_index, _blocks);
    }
}

//////////////////////////////////////////////////////////////////////////

static bool update_progress(std::atomic<int>& progress, int new_value, std::ostream& _log)
{
    auto oldprogress = progress.exchange(new_value, std::memory_order_release);
    if (oldprogress < 0)
    {
        _log << "Reading was interrupted";
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

static bool read(std::istream& inStream, char* value, size_t size)
{
    inStream.read(value, size);
    const auto bytesRead = inStream.gcount();
    if (bytesRead < static_cast<std::streamsize>(size))
    {
        const size_t valid = bytesRead > 0 ? static_cast<size_t>(bytesRead) : 0U;
        std::memset(value + valid, 0, size - valid);
        return false;
    }
    return true;
}

template <class T>
static bool read(std::istream& inStream, T& value)
{
    return read(inStream, reinterpret_cast<char*>(&value), sizeof(T));
}

static bool tryReadMarker(std::istream& inStream, uint32_t& marker)
{
    return read(inStream, marker) && marker == EASY_PROFILER_SIGNATURE;
}

static bool tryReadMarker(std::istream& inStream)
{
    uint32_t marker = 0;
    return tryReadMarker(inStream, marker);
}

static bool remainingStreamSize(std::istream& inStream, uint64_t& size)
{
    const std::istream::iostate originalState = inStream.rdstate();
    const std::istream::pos_type current = inStream.tellg();
    if (current == std::istream::pos_type(-1))
    {
        inStream.clear(originalState);
        return false;
    }

    inStream.seekg(0, std::ios::end);
    const std::istream::pos_type end = inStream.tellg();
    inStream.seekg(current);
    if (end == std::istream::pos_type(-1) || end < current || !inStream)
    {
        inStream.clear(originalState);
        return false;
    }

    size = static_cast<uint64_t>(end - current);
    inStream.clear(originalState);
    return true;
}

static bool bufferStream(std::atomic<int>& progress, std::istream& input,
                         std::string& bytes, std::ostream& log)
{
    static const size_t MAX_BUFFERED_STREAM_SIZE = 64U * 1024U * 1024U;
    char chunk[64 * 1024];
    while (input)
    {
        input.read(chunk, sizeof(chunk));
        const std::streamsize count = input.gcount();
        if (count > 0)
        {
            const size_t chunkSize = static_cast<size_t>(count);
            if (chunkSize > MAX_BUFFERED_STREAM_SIZE - bytes.size())
            {
                log << "Non-seekable profile stream exceeds the 64 MiB buffering limit.";
                return false;
            }
            bytes.append(chunk, chunkSize);
        }

        if (progress.load(std::memory_order_acquire) < 0)
        {
            log << "Reading was interrupted";
            return false;
        }
    }

    if (input.bad())
    {
        log << "Unable to buffer a non-seekable profile stream.";
        return false;
    }

    return true;
}

static bool ticksToNanoseconds(profiler::timestamp_t& ticks, uint64_t frequency)
{
    return frequency == 0 || scaleTimestamp(ticks, TIME_FACTOR, frequency, ticks);
}

static bool validateDescriptorRecord(const char* data, uint16_t size)
{
    const size_t fixedSize = sizeof(profiler::SerializedBlockDescriptor);
    if (size < fixedSize + 2U)
        return false;

    const size_t typeOffset = sizeof(profiler::block_id_t) + sizeof(int32_t) +
                              sizeof(profiler::color_t);
    const size_t statusOffset = typeOffset + sizeof(profiler::block_type_t);
    const auto encodedType = static_cast<uint8_t>(data[typeOffset]);
    const auto encodedStatus = static_cast<uint8_t>(data[statusOffset]);
    if (encodedType >= static_cast<uint8_t>(profiler::BlockType::TypesCount) ||
        encodedStatus > static_cast<uint8_t>(profiler::FORCE_ON_WITHOUT_CHILDREN))
        return false;

    uint16_t nameLength = 0;
    std::memcpy(&nameLength, data + sizeof(profiler::BaseBlockDescriptor), sizeof(nameLength));
    const size_t stringsSize = static_cast<size_t>(size) - fixedSize;
    if (nameLength == 0 || nameLength >= stringsSize)
        return false;

    const char* strings = data + fixedSize;
    if (strings[nameLength - 1U] != '\0')
        return false;

    const char* file = strings + nameLength;
    const size_t fileSize = stringsSize - nameLength;
    return std::memchr(file, '\0', fileSize) != nullptr;
}

static size_t arbitraryValueElementSize(uint8_t encodedType)
{
    using profiler::DataType;
    switch (static_cast<DataType>(encodedType))
    {
        case DataType::Bool:   return sizeof(bool);
        case DataType::Char:   return sizeof(char);
        case DataType::Int8:   return sizeof(int8_t);
        case DataType::Uint8:  return sizeof(uint8_t);
        case DataType::Int16:  return sizeof(int16_t);
        case DataType::Uint16: return sizeof(uint16_t);
        case DataType::Int32:  return sizeof(int32_t);
        case DataType::Uint32: return sizeof(uint32_t);
        case DataType::Int64:  return sizeof(int64_t);
        case DataType::Uint64: return sizeof(uint64_t);
        case DataType::Float:  return sizeof(float);
        case DataType::Double: return sizeof(double);
        case DataType::String: return sizeof(char);
        case DataType::TypesCount: break;
    }
    return 0;
}

static bool validateArbitraryValueRecord(const char* data, uint16_t size)
{
    if (size < sizeof(profiler::ArbitraryValue))
        return false;

    static_assert(sizeof(profiler::DataType) == 1, "DataType wire field must be one byte");
    static_assert(sizeof(bool) == 1, "Boolean wire field must be one byte");

    const size_t nameStubOffset = sizeof(profiler::BaseBlockData);
    const size_t dataSizeOffset = nameStubOffset + 2U;
    const size_t typeOffset = dataSizeOffset + sizeof(uint16_t);
    const size_t arrayFlagOffset = typeOffset + sizeof(profiler::DataType);

    if (data[nameStubOffset] != '\0')
        return false;

    uint16_t dataSize = 0;
    std::memcpy(&dataSize, data + dataSizeOffset, sizeof(dataSize));
    const auto encodedType = static_cast<uint8_t>(data[typeOffset]);
    const auto arrayFlag = static_cast<uint8_t>(data[arrayFlagOffset]);
    if (encodedType >= static_cast<uint8_t>(profiler::DataType::TypesCount) ||
        arrayFlag > 1U || dataSize > profiler::MAX_BLOCK_DATA_SIZE ||
        dataSize != size - sizeof(profiler::ArbitraryValue))
        return false;

    const size_t elementSize = arbitraryValueElementSize(encodedType);
    if (elementSize == 0)
        return false;

    if (encodedType == static_cast<uint8_t>(profiler::DataType::String))
        // EASY_STRING explicitly permits callers to omit the trailing zero.
        // Keep accepting those profiles; consumers that need C-string
        // semantics must use the recorded size or provide the terminator.
        return arrayFlag == 1U;

    return arrayFlag == 1U ? dataSize % elementSize == 0 : dataSize == elementSize;
}

//////////////////////////////////////////////////////////////////////////

struct EasyFileHeader
{
    uint32_t signature = 0;
    uint32_t version = 0;
    profiler::processid_t pid = 0;
    int64_t cpu_frequency = 0;
    profiler::timestamp_t begin_time = 0;
    profiler::timestamp_t end_time = 0;
    uint64_t memory_size = 0;
    uint64_t descriptors_memory_size = 0;
    uint32_t blocks_count = 0;
    uint32_t descriptors_count = 0;
    uint32_t threads_count = 0;
    uint16_t bookmarks_count = 0;
    uint16_t padding = 0;
};

static bool readHeader_v1(EasyFileHeader& _header, std::istream& inStream, std::ostream& _log)
{
    // File header before v2.0.0

    if (_header.version > EASY_V_100)
    {
        if (_header.version < EASY_V_130)
        {
            uint32_t old_pid = 0;
            read(inStream, old_pid);
            _header.pid = old_pid;
        }
        else
        {
            read(inStream, _header.pid);
        }
    }

    read(inStream, _header.cpu_frequency);
    read(inStream, _header.begin_time);
    read(inStream, _header.end_time);

    read(inStream, _header.blocks_count);
    if (_header.blocks_count == 0)
    {
        _log << "Profiled blocks number == 0";
        return false;
    }

    read(inStream, _header.memory_size);
    if (_header.memory_size == 0)
    {
        _log << "Wrong memory size == 0 for " << _header.blocks_count << " blocks";
        return false;
    }

    read(inStream, _header.descriptors_count);
    if (_header.descriptors_count == 0)
    {
        _log << "Blocks description number == 0";
        return false;
    }

    read(inStream, _header.descriptors_memory_size);
    if (_header.descriptors_memory_size == 0)
    {
        _log << "Wrong memory size == 0 for " << _header.descriptors_count << " blocks descriptions";
        return false;
    }

    return true;
}

static bool readHeader_v2(EasyFileHeader& _header, std::istream& inStream, std::ostream& _log)
{
    // File header after v2.0.0

    read(inStream, _header.pid);
    read(inStream, _header.cpu_frequency);
    read(inStream, _header.begin_time);
    read(inStream, _header.end_time);

    read(inStream, _header.memory_size);
    if (_header.memory_size == 0)
    {
        _log << "Wrong memory size == 0 for " << _header.blocks_count << " blocks";
        return false;
    }

    read(inStream, _header.descriptors_memory_size);
    if (_header.descriptors_memory_size == 0)
    {
        _log << "Wrong memory size == 0 for " << _header.descriptors_count << " blocks descriptions";
        return false;
    }

    read(inStream, _header.blocks_count);
    if (_header.blocks_count == 0)
    {
        _log << "Profiled blocks number == 0";
        return false;
    }

    read(inStream, _header.descriptors_count);
    if (_header.descriptors_count == 0)
    {
        _log << "Blocks description number == 0";
        return false;
    }

    return true;
}

static bool readHeader_v2_1(EasyFileHeader& _header, std::istream& inStream, std::ostream& _log)
{
    if (!readHeader_v2(_header, inStream, _log))
        return false;

    read(inStream, _header.threads_count);
    if (_header.threads_count == 0)
    {
        _log << "Threads count == 0.\nNothing to read.";
        return false;
    }

    read(inStream, _header.bookmarks_count);
    read(inStream, _header.padding);

    if (_header.padding != 0)
    {
        _log << "Header padding != 0.\nFile corrupted.";
        return false;
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

extern "C" PROFILER_API profiler::block_index_t fillTreesFromFile(std::atomic<int>& progress, const char* filename,
                                                                  profiler::BeginEndTime& begin_end_time,
                                                                  profiler::SerializedData& serialized_blocks,
                                                                  profiler::SerializedData& serialized_descriptors,
                                                                  profiler::descriptors_list_t& descriptors,
                                                                  profiler::blocks_t& blocks,
                                                                  profiler::thread_blocks_tree_t& threaded_trees,
                                                                  profiler::bookmarks_t& bookmarks,
                                                                  uint32_t& descriptors_count,
                                                                  uint32_t& version,
                                                                  profiler::processid_t& pid,
                                                                  bool gather_statistics,
                                                                  std::ostream& _log)
{
    if (!update_progress(progress, 0, _log))
    {
        return 0;
    }

    std::ifstream inFile(filename, std::fstream::binary);
    if (!inFile.is_open())
    {
        _log << "Can not open file " << filename;
        return 0;
    }

    // Read data from file
    auto result = fillTreesFromStream(progress, inFile, begin_end_time, serialized_blocks, serialized_descriptors,
                                      descriptors, blocks, threaded_trees, bookmarks, descriptors_count, version, pid,
                                      gather_statistics, _log);

    return result;
}

//////////////////////////////////////////////////////////////////////////

static profiler::block_index_t fillTreesFromStreamImpl(std::atomic<int>& progress, std::istream& inStream,
                                                       profiler::BeginEndTime& begin_end_time,
                                                       profiler::SerializedData& serialized_blocks,
                                                       profiler::SerializedData& serialized_descriptors,
                                                       profiler::descriptors_list_t& descriptors,
                                                       profiler::blocks_t& blocks,
                                                       profiler::thread_blocks_tree_t& threaded_trees,
                                                       profiler::bookmarks_t& bookmarks,
                                                       uint32_t& descriptors_count,
                                                       uint32_t& version,
                                                       profiler::processid_t& pid,
                                                       bool gather_statistics,
                                                       std::ostream& _log)
{
    EASY_FUNCTION(profiler::colors::Cyan);

    if (!update_progress(progress, 0, _log))
    {
        return 0;
    }

    uint64_t initialSize = 0;
    if (!remainingStreamSize(inStream, initialSize))
    {
        std::string bytes;
        if (!bufferStream(progress, inStream, bytes, _log))
            return 0;
        std::istringstream buffered(bytes, std::ios::in | std::ios::binary);
        return fillTreesFromStream(progress, buffered, begin_end_time, serialized_blocks,
                                   serialized_descriptors, descriptors, blocks, threaded_trees,
                                   bookmarks, descriptors_count, version, pid,
                                   gather_statistics, _log);
    }

    uint32_t signature = 0;
    if (!tryReadMarker(inStream, signature))
    {
        _log << "Wrong signature " << signature << ".\nThis is not EasyProfiler file/stream.";
        return 0;
    }

    version = 0;
    read(inStream, version);
    if (!isCompatibleVersion(version))
    {
        _log << "Incompatible version: v"
             << (version >> 24) << "." << ((version & 0x00ff0000) >> 16) << "." << (version & 0x0000ffff);
        return 0;
    }

    EasyFileHeader header;
    header.signature = signature;
    header.version = version;

    if (version < EASY_V_200)
    {
        if (!readHeader_v1(header, inStream, _log))
            return 0;
        header.threads_count = std::numeric_limits<decltype(header.threads_count)>::max();
    }
    else if (version < EASY_V_210)
    {
        if (!readHeader_v2(header, inStream, _log))
            return 0;
        header.threads_count = std::numeric_limits<decltype(header.threads_count)>::max();
    }
    else
    {
        if (!readHeader_v2_1(header, inStream, _log))
            return 0;
    }

    if (!inStream)
    {
        _log << "Profile header is truncated.";
        return 0;
    }

    pid = header.pid;

    if (header.cpu_frequency < 0)
    {
        _log << "CPU frequency cannot be negative.";
        return 0;
    }

    const uint64_t cpu_frequency = static_cast<uint64_t>(header.cpu_frequency);
    auto begin_time = header.begin_time;
    auto end_time = header.end_time;

    const auto memory_size = header.memory_size;
    const auto descriptors_memory_size = header.descriptors_memory_size;
    const auto total_blocks_count = header.blocks_count;
    descriptors_count = header.descriptors_count;

    if (memory_size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) ||
        descriptors_memory_size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    {
        _log << "Profile declares buffers too large for this platform.";
        return 0;
    }

    // descriptors_memory_size is not a physical byte count: easy_profiler 2.1
    // and earlier declared it as the size of the buffer the reading side had
    // allocated plus the two-byte record prefixes, and that reader allocated
    // the declared size, so every GUI re-save added another
    // descriptors_count * 2 bytes on top. A trace saved three times by a 2.x
    // GUI declares payloadSum + 6 * descriptors_count. The field is therefore
    // only an allocation hint here; the physical bound on the descriptor
    // section is the one prefix every record carries.
    uint64_t remainingSize = 0;
    if (!remainingStreamSize(inStream, remainingSize))
    {
        _log << "Profile stream size is unavailable.";
        return 0;
    }

    // Every record has at least a uint16_t size prefix, and a block record is
    // rejected below sizeof(BaseBlockData) + 1 payload bytes. These checks keep
    // forged counts from causing disproportionate reserve() allocations.
    const uint64_t maximumRecordCount = remainingSize / sizeof(uint16_t);
    const uint64_t maximumBlockCount = remainingSize / (sizeof(uint16_t) + sizeof(profiler::BaseBlockData) + 1U);
    if (descriptors_count > maximumRecordCount || total_blocks_count > maximumBlockCount ||
        (version >= EASY_V_210 && header.threads_count > maximumRecordCount))
    {
        _log << "Profile record count exceeds the available stream data.";
        return 0;
    }

    const uint64_t descriptorPrefixBytes = static_cast<uint64_t>(descriptors_count) * sizeof(uint16_t);
    if (descriptorPrefixBytes > remainingSize ||
        memory_size > remainingSize - descriptorPrefixBytes)
    {
        _log << "Profile declares more payload data than the stream contains.";
        return 0;
    }

    const uint64_t descriptorsCapacity = std::min(descriptors_memory_size, remainingSize);

    if (cpu_frequency != 0)
    {
        if (!ticksToNanoseconds(begin_time, cpu_frequency) ||
            !ticksToNanoseconds(end_time, cpu_frequency))
        {
            _log << "Timestamp conversion overflow in profile header.";
            return 0;
        }
    }

    begin_end_time.beginTime = begin_time;
    begin_end_time.endTime = end_time;

    descriptors.reserve(descriptors_count);
    //const char* olddata = append_regime ? serialized_descriptors.data() : nullptr;
    serialized_descriptors.set(descriptorsCapacity);
    //validate_pointers(progress, olddata, serialized_descriptors, descriptors, descriptors.size());

    uint64_t i = 0;
    while (!inStream.eof() && descriptors.size() < descriptors_count)
    {
        uint16_t sz = 0;
        read(inStream, sz);
        if (sz == 0)
        {
            descriptors.push_back(nullptr);
            continue;
        }

        if (i > descriptorsCapacity || sz > descriptorsCapacity - i)
        {
            _log << "Exceeded descriptor memory size.\npos: " << i
                 << "\nsize: " << sz
                 << "\nmax pos: " << descriptorsCapacity
                 << "\nFile/Stream corrupted.";
            return 0;
        }

        char* data = serialized_descriptors[i];
        if (!read(inStream, data, sz))
        {
            _log << "Descriptor data is truncated.";
            return 0;
        }
        if (!validateDescriptorRecord(data, sz))
        {
            _log << "Invalid descriptor record.";
            return 0;
        }
        auto descriptor = reinterpret_cast<profiler::SerializedBlockDescriptor*>(data);
        if (descriptor->id() != descriptors.size())
        {
            _log << "Descriptor id does not match its table position.";
            return 0;
        }
        descriptors.push_back(descriptor);

        i += sz;
        if (!update_progress(progress, static_cast<int>(15 * i / descriptors_memory_size), _log))
        {
            return 0;
        }
    }

    // Accept the historic spelling described above for any number of old-GUI saves. The
    // declared total is not a safety boundary and capping it only risks rejecting a valid
    // file: the allocation is clamped to the stream size and the per-record bounds checks
    // above keep every read inside the buffer.
    const uint64_t declaredSurplus = descriptors_memory_size - i;
    const uint64_t prefixBytes = static_cast<uint64_t>(descriptors_count) * sizeof(uint16_t);
    const bool historicSurplus = prefixBytes != 0 && (declaredSurplus % prefixBytes) == 0;
    if (descriptors.size() != descriptors_count || (declaredSurplus != 0 && !historicSurplus))
    {
        _log << "Descriptor count or memory size does not match the profile header.";
        return 0;
    }

    using PerThreadStats = std::unordered_map<profiler::thread_id_t, profiler::stats_map_t, estd::hash<profiler::thread_id_t> >;
    PerThreadStats parent_statistics, frame_statistics;
    IdMap identification_table;

    blocks.reserve(total_blocks_count);
    //olddata = append_regime ? serialized_blocks.data() : nullptr;
    serialized_blocks.set(memory_size);
    //validate_pointers(progress, olddata, serialized_blocks, blocks, blocks.size());

    i = 0;
    uint32_t read_number = 0, threads_read_number = 0;
    profiler::block_index_t blocks_counter = 0;
    std::vector<char> name;

    ReaderThreadPool pool;

    while (!inStream.eof() && threads_read_number++ < header.threads_count)
    {
        EASY_BLOCK("Read thread data", profiler::colors::DarkGreen);

        profiler::thread_id_t thread_id = 0;
        if (version < EASY_V_130)
        {
            uint32_t thread_id32 = 0;
            if (!read(inStream, thread_id32))
                break;
            thread_id = thread_id32;
        }
        else
        {
            if (!read(inStream, thread_id))
                break;
        }

        auto& root = threaded_trees[thread_id];

        uint16_t name_size = 0;
        if (!read(inStream, name_size))
        {
            _log << "Thread header is truncated.";
            return 0;
        }
        if (name_size != 0)
        {
            name.resize(name_size);
            if (!read(inStream, name.data(), name_size) || name.back() != '\0')
            {
                _log << "Thread name is truncated or not terminated.";
                return 0;
            }
            root.thread_name.assign(name.data(), name_size - 1U);
        }

        CsStatsMap per_thread_statistics_cs;

        uint32_t blocks_number_in_thread = 0;
        if (!read(inStream, blocks_number_in_thread) ||
            read_number > total_blocks_count ||
            blocks_number_in_thread > total_blocks_count - read_number)
        {
            _log << "Context-switch count exceeds the total block count.";
            return 0;
        }
        auto threshold = read_number + blocks_number_in_thread;
        while (!inStream.eof() && read_number < threshold)
        {
            EASY_BLOCK("Read context switch", profiler::colors::Green);

            ++read_number;

            uint16_t sz = 0;
            read(inStream, sz);
            if (sz == 0)
            {
                _log << "Bad CSwitch block size == 0";
                return 0;
            }

            if (i > memory_size || sz > memory_size - i)
            {
                _log << "File corrupted.\nActual context switches data size > size pointed in file.";
                return 0;
            }

            if (sz < sizeof(profiler::CSwitchEvent) + 1U)
            {
                _log << "Context switch record is too small.";
                return 0;
            }

            char* data = serialized_blocks[i];
            if (!read(inStream, data, sz))
            {
                _log << "Context switch data is truncated.";
                return 0;
            }
            if (data[sz - 1U] != '\0')
            {
                _log << "Context switch name is not terminated.";
                return 0;
            }
            i += sz;

            auto baseData = reinterpret_cast<profiler::SerializedCSwitch*>(data);
            profiler::timestamp_t t_begin = unaligned_load64<profiler::timestamp_t>(data);
            profiler::timestamp_t t_end = unaligned_load64<profiler::timestamp_t>(data + sizeof(t_begin));

            if (cpu_frequency != 0)
            {
                if (!ticksToNanoseconds(t_begin, cpu_frequency) ||
                    !ticksToNanoseconds(t_end, cpu_frequency))
                {
                    _log << "Timestamp conversion overflow in context switch record.";
                    return 0;
                }
                unaligned_store64(data, t_begin);
                unaligned_store64(data + sizeof(t_begin), t_end);
            }

            if (t_end > begin_time)
            {
                if (t_begin < begin_time)
                {
                    t_begin = begin_time;
                    unaligned_store64(data, t_begin);
                }

                blocks.emplace_back();
                profiler::BlocksTree& tree = blocks.back();
                tree.cs = baseData;
                const auto block_index = blocks_counter++;

                root.wait_time += baseData->duration();
                root.sync.emplace_back(block_index);

                if (gather_statistics)
                {
                    EASY_BLOCK("Gather per thread statistics", profiler::colors::Coral);
                    tree.per_thread_stats = update_statistics(per_thread_statistics_cs, tree, block_index, ~0U, blocks);//, thread_id, blocks);
                }
            }

            // calculate medians for each block
            calculate_medians_async(pool, per_thread_statistics_cs);

            if (!update_progress(progress, 20 + static_cast<int>(67 * i / memory_size), _log))
            {
                return 0; // Loading interrupted
            }
        }

        if (inStream.eof())
            break;

        profiler::stats_map_t per_thread_statistics;

        blocks_number_in_thread = 0;
        if (!read(inStream, blocks_number_in_thread) ||
            read_number > total_blocks_count ||
            blocks_number_in_thread > total_blocks_count - read_number)
        {
            _log << "Thread block count exceeds the total block count.";
            return 0;
        }
        threshold = read_number + blocks_number_in_thread;
        while (!inStream.eof() && read_number < threshold)
        {
            EASY_BLOCK("Read block", profiler::colors::Green);

            ++read_number;

            uint16_t sz = 0;
            read(inStream, sz);
            if (sz == 0)
            {
                _log << "Bad block size == 0";
                return 0;
            }

            if (i > memory_size || sz > memory_size - i)
            {
                _log << "File corrupted.\nActual blocks data size > size pointed in file.";
                return 0;
            }

            if (sz < sizeof(profiler::BaseBlockData) + 1U)
            {
                _log << "Block record is too small.";
                return 0;
            }

            char* data = serialized_blocks[i];
            if (!read(inStream, data, sz))
            {
                _log << "Block data is truncated.";
                return 0;
            }
            i += sz;
            auto baseData = reinterpret_cast<profiler::SerializedBlock*>(data);
            if (baseData->id() >= descriptors_count)
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

            if (desc->type() == profiler::BlockType::Value)
            {
                if (!validateArbitraryValueRecord(data, sz))
                {
                    _log << "Invalid arbitrary value record.";
                    return 0;
                }
            }
            else if (std::memchr(data + sizeof(profiler::BaseBlockData), '\0',
                                 sz - sizeof(profiler::BaseBlockData)) == nullptr)
            {
                _log << "Block name is not terminated.";
                return 0;
            }

            profiler::timestamp_t t_begin = unaligned_load64<profiler::timestamp_t>(data);
            profiler::timestamp_t t_end = unaligned_load64<profiler::timestamp_t>(data + sizeof(t_begin));

            if (cpu_frequency != 0)
            {
                if (!ticksToNanoseconds(t_begin, cpu_frequency) ||
                    !ticksToNanoseconds(t_end, cpu_frequency))
                {
                    _log << "Timestamp conversion overflow in block record.";
                    return 0;
                }
                unaligned_store64(data, t_begin);
                unaligned_store64(data + sizeof(t_begin), t_end);
            }

            if (t_end >= begin_time)
            {
                if (t_begin < begin_time)
                {
                    t_begin = begin_time;
                    unaligned_store64(data, t_begin);
                }

                blocks.emplace_back();
                profiler::BlocksTree& tree = blocks.back();
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
                        auto id = static_cast<profiler::block_id_t>(descriptors.size());
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
                        //auto lower = std::lower_bound(root.children.begin(), root.children.end(), tree);
                        /**/
                        EASY_BLOCK("Find children", profiler::colors::Blue);
                        auto rlower1 = ++root.children.rbegin();
                        for (; rlower1 != root.children.rend() && mt0 <= blocks[*rlower1].node->begin(); ++rlower1);
                        auto lower = rlower1.base();
                        std::move(lower, root.children.end(), std::back_inserter(tree.children));

                        root.children.erase(lower, root.children.end());
                        EASY_END_BLOCK;

                        if (gather_statistics)
                        {
                            EASY_BLOCK("Gather statistic within parent", profiler::colors::Magenta);
                            auto& per_parent_statistics = parent_statistics[thread_id];
                            per_parent_statistics.clear();

                            //per_parent_statistics.reserve(tree.children.size());     // this gives slow-down on Windows
                            //per_parent_statistics.reserve(tree.children.size() * 2); // this gives no speed-up on Windows
                            // TODO: check this behavior on Linux

                            for (auto child_block_index : tree.children)
                            {
                                auto& child = blocks[child_block_index];
                                child.per_parent_stats = update_statistics(per_parent_statistics, child, child_block_index, block_index, blocks);
                                if (tree.depth < child.depth)
                                    tree.depth = child.depth;
                            }

                            // calculate medians for each block
                            calculate_medians_async(pool, per_parent_statistics);
                        }
                        else
                        {
                            for (auto child_block_index : tree.children)
                            {
                                const auto& child = blocks[child_block_index];
                                if (tree.depth < child.depth)
                                    tree.depth = child.depth;
                            }
                        }

                        if (tree.depth == 254)
                        {
                            // 254 because we need 1 additional level for root (thread).
                            // In other words: real stack depth = 1 root block + 254 children

                            if (*tree.node->name() != 0)
                                _log << "Stack depth exceeded value of 254\nfor block \"" << desc->name() << "\"";
                            else
                                _log << "Stack depth exceeded value of 254\nfor block \"" << desc->name() << "\"\nfrom file \"" << desc->file() << "\":" << desc->line();

                            return 0;
                        }

                        ++tree.depth;
                    }
                }

                ++root.blocks_number;
                root.children.emplace_back(block_index);// std::move(tree));
                if (desc->type() != profiler::BlockType::Block)
                    root.events.emplace_back(block_index);


                if (gather_statistics)
                {
                    EASY_BLOCK("Gather per thread statistics", profiler::colors::Coral);
                    tree.per_thread_stats = update_statistics(per_thread_statistics, tree, block_index, ~0U, blocks);//, thread_id, blocks);
                }
            }

            if (!update_progress(progress, 20 + static_cast<int>(67 * i / memory_size), _log))
                return 0; // Loading interrupted
        }

        // calculate medians for each block
        calculate_medians_async(pool, per_thread_statistics);
    }

    if (i != memory_size || total_blocks_count != blocks_counter)
    {
        _log << "Read blocks count: " << blocks_counter
             << "\ndoes not match blocks count\nstored in header: " << total_blocks_count
             << ", or the block memory size does not match.\nFile corrupted.";
        return 0;
    }

    if (!inStream.eof() && version >= EASY_V_210)
    {
        if (!tryReadMarker(inStream))
        {
            _log << "Bad threads section end mark.\nFile corrupted.";
            return 0;
        }

        if (!inStream.eof() && header.bookmarks_count != 0)
        {
            // Read bookmarks
            bookmarks.reserve(header.bookmarks_count);

            std::vector<char> stringBuffer;
            read_number = 0;

            while (!inStream.eof() && read_number < header.bookmarks_count)
            {
                profiler::Bookmark bookmark;

                uint16_t usedMemorySize = 0;
                if (!read(inStream, usedMemorySize) ||
                    !read(inStream, bookmark.pos) ||
                    !read(inStream, bookmark.color))
                {
                    _log << "Bookmark record is truncated.";
                    return 0;
                }

                if (usedMemorySize < profiler::Bookmark::BaseSize)
                {
                    _log << "Bad bookmark size: " << usedMemorySize
                         << ", which is less than Bookmark::BaseSize: "
                         << profiler::Bookmark::BaseSize;
                    return 0;
                }

                usedMemorySize -= static_cast<uint16_t>(profiler::Bookmark::BaseSize) - 1;
                if (usedMemorySize > 0)
                {
                    stringBuffer.resize(usedMemorySize);
                    if (!read(inStream, stringBuffer.data(), usedMemorySize))
                    {
                        _log << "Bookmark description is truncated.";
                        return 0;
                    }

                    if (stringBuffer.back() != 0)
                    {
                        stringBuffer.resize(stringBuffer.size() + 1);
                        stringBuffer.back() = 0;

                        _log << "Bad bookmark description:\n\"" << const_cast<const char*>(stringBuffer.data())
                            << "\"\nWhich is not zero terminated string.\nLast symbol is: '"
                            << const_cast<const char*>(stringBuffer.data() + stringBuffer.size() - 2) << "'";

                        return 0;
                    }

                    if (usedMemorySize != 1)
                        bookmark.text = stringBuffer.data();
                }
                else
                {
                    bookmark.text.clear();
                }

                bookmarks.push_back(bookmark);

                ++read_number;

                if (!update_progress(progress, 87 + static_cast<int>(3 * read_number / header.bookmarks_count), _log))
                    return 0; // Loading interrupted
            }

            if (!inStream.eof() && !tryReadMarker(inStream))
            {
                _log << "Bad bookmarks section end mark.\nFile corrupted.";
                return 0;
            }
        }
    }

    if (!update_progress(progress, 90, _log))
        return 0; // Loading interrupted

    EASY_BLOCK("Gather statistics for roots", profiler::colors::Purple);
    if (gather_statistics)
    {
        AsyncResults results;
        results.futures.reserve(threaded_trees.size());

        for (auto& it : threaded_trees)
        {
            auto& root = it.second;
            root.thread_id = it.first;
            //root.tree.shrink_to_fit();

            auto& per_frame_statistics = frame_statistics[root.thread_id];
            auto& per_parent_statistics = parent_statistics[it.first];
            per_parent_statistics.clear();

            results.futures.emplace_back(pool.async([&] () -> async_result_t
            {
                //std::sort(root.sync.begin(), root.sync.end(), [&blocks](profiler::block_index_t left, profiler::block_index_t right)
                //{
                //    return blocks[left].node->begin() < blocks[right].node->begin();
                //});

                profiler::block_index_t cs_index = 0;
                for (auto child_index : root.children)
                {
                    auto& frame = blocks[child_index];

                    if (descriptors[frame.node->id()]->type() == profiler::BlockType::Block)
                        ++root.frames_number;

                    frame.per_parent_stats = update_statistics(per_parent_statistics, frame, child_index, ~0U, blocks);//, root.thread_id, blocks);

                    per_frame_statistics.clear();
                    update_statistics_recursive(per_frame_statistics, frame, child_index, child_index, blocks);

                    calculate_medians(per_parent_statistics.begin(), per_parent_statistics.end());
                    calculate_medians(per_frame_statistics.begin(), per_frame_statistics.end());

                    if (cs_index < root.sync.size())
                    {
                        CsStatsMap frame_stats_cs;
                        do {

                            auto j = root.sync[cs_index];
                            auto& cs = blocks[j];
                            if (cs.node->end() < frame.node->begin())
                                continue;
                            if (cs.node->begin() > frame.node->end())
                                break;
                            cs.per_frame_stats = update_statistics(frame_stats_cs, cs, cs_index, child_index, blocks);

                        } while (++cs_index < root.sync.size());

                        calculate_medians(frame_stats_cs.begin(), frame_stats_cs.end());
                    }

                    if (root.depth < frame.depth)
                        root.depth = frame.depth;

                    root.profiled_time += frame.node->duration();
                }

                ++root.depth;

                EASY_FINISH_ASYNC; // MSVC 2013 hack
            }));
        }

        int j = 0, n = static_cast<int>(results.futures.size());
        for (auto& result : results.futures)
        {
            if (result.valid())
            {
                result.get();
            }
            progress.store(90 + (10 * ++j) / n, std::memory_order_release);
        }
    }
    else
    {
        int j = 0, n = static_cast<int>(threaded_trees.size());
        for (auto& it : threaded_trees)
        {
            auto& root = it.second;
            root.thread_id = it.first;

            //std::sort(root.sync.begin(), root.sync.end(), [&blocks](profiler::block_index_t left, profiler::block_index_t right)
            //{
            //    return blocks[left].node->begin() < blocks[right].node->begin();
            //});

            //root.tree.shrink_to_fit();
            for (auto child_block_index : root.children)
            {
                auto& frame = blocks[child_block_index];

                if (descriptors[frame.node->id()]->type() == profiler::BlockType::Block)
                    ++root.frames_number;

                if (root.depth < frame.depth)
                    root.depth = frame.depth;

                root.profiled_time += frame.node->duration();
            }

            ++root.depth;

            progress.store(90 + (10 * ++j) / n, std::memory_order_release);
        }
    }
    // No need to delete BlockStatistics instances - they will be deleted inside BlocksTree destructors

    progress.store(100, std::memory_order_release);
    return blocks_counter;
}

// The GUI runs the readers on a thread it joins later, so an exception escaping
// here (std::bad_alloc from a forged size, std::system_error from thread
// creation) would be std::terminate rather than a failed load.
extern "C" PROFILER_API profiler::block_index_t fillTreesFromStream(std::atomic<int>& progress, std::istream& inStream,
                                                                    profiler::BeginEndTime& begin_end_time,
                                                                    profiler::SerializedData& serialized_blocks,
                                                                    profiler::SerializedData& serialized_descriptors,
                                                                    profiler::descriptors_list_t& descriptors,
                                                                    profiler::blocks_t& blocks,
                                                                    profiler::thread_blocks_tree_t& threaded_trees,
                                                                    profiler::bookmarks_t& bookmarks,
                                                                    uint32_t& descriptors_count,
                                                                    uint32_t& version,
                                                                    profiler::processid_t& pid,
                                                                    bool gather_statistics,
                                                                    std::ostream& _log)
{
    try
    {
        return fillTreesFromStreamImpl(progress, inStream, begin_end_time, serialized_blocks, serialized_descriptors,
                                       descriptors, blocks, threaded_trees, bookmarks, descriptors_count, version,
                                       pid, gather_statistics, _log);
    }
    catch (const std::exception& e)
    {
        _log << "Reading the profile failed: " << e.what();
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////

static bool readDescriptionsFromStreamImpl(std::atomic<int>& progress, std::istream& inStream,
                                           profiler::SerializedData& serialized_descriptors,
                                           profiler::descriptors_list_t& descriptors,
                                           std::ostream& _log)
{
    EASY_FUNCTION(profiler::colors::Cyan);

    if (!update_progress(progress, 0, _log))
        return false;

    uint64_t initialSize = 0;
    if (!remainingStreamSize(inStream, initialSize))
    {
        std::string bytes;
        if (!bufferStream(progress, inStream, bytes, _log))
            return false;
        std::istringstream buffered(bytes, std::ios::in | std::ios::binary);
        return readDescriptionsFromStream(progress, buffered, serialized_descriptors,
                                          descriptors, _log);
    }

    uint32_t signature = 0;
    read(inStream, signature);
    if (signature != EASY_PROFILER_SIGNATURE)
    {
        _log << "Wrong file signature.\nThis is not EasyProfiler file/stream.";
        return false;
    }

    uint32_t version = 0;
    read(inStream, version);
    if (!isCompatibleVersion(version))
    {
        _log << "Incompatible version: v"
             << (version >> 24) << "." << ((version & 0x00ff0000) >> 16) << "." << (version & 0x0000ffff);
        return false;
    }

    uint32_t descriptors_count = 0;
    read(inStream, descriptors_count);
    if (descriptors_count == 0)
    {
        _log << "Blocks description number == 0";
        return false;
    }

    uint64_t descriptors_memory_size = 0;
    read(inStream, descriptors_memory_size);
    if (descriptors_memory_size == 0)
    {
        _log << "Wrong memory size == 0 for " << descriptors_count << " blocks descriptions";
        return false;
    }

    if (!inStream || descriptors_memory_size > static_cast<uint64_t>(std::numeric_limits<size_t>::max()))
    {
        _log << "Description stream header is truncated or too large.";
        return false;
    }

    uint64_t remainingSize = 0;
    if (!remainingStreamSize(inStream, remainingSize))
    {
        _log << "Description stream size is unavailable.";
        return false;
    }

    if (descriptors_memory_size > remainingSize || descriptors_count > remainingSize / sizeof(uint16_t))
    {
        _log << "Description stream declares more data than it contains.";
        return false;
    }

    descriptors.reserve(descriptors_count);
    //const char* olddata = append_regime ? serialized_descriptors.data() : nullptr;
    serialized_descriptors.set(descriptors_memory_size);
    //validate_pointers(progress, olddata, serialized_descriptors, descriptors, descriptors.size());

    uint64_t i = 0;
    while (!inStream.eof() && descriptors.size() < descriptors_count)
    {
        uint16_t sz = 0;
        read(inStream, sz);
        if (sz == 0)
        {
            //descriptors.push_back(nullptr);
            _log << "Zero descriptor size.\nFile/Stream corrupted.";
            return false;
        }

        if (i > descriptors_memory_size || sz > descriptors_memory_size - i)
        {
            _log << "Exceeded memory size.\npos: " << i << "\nsize: " << sz
                 << "\nnext pos: " << i + sz
                 << "\nmax pos: " << descriptors_memory_size
                 << "\nFile/Stream corrupted.";
            return false;
        }

        char* data = serialized_descriptors[i];
        if (!read(inStream, data, sz) || !validateDescriptorRecord(data, sz))
        {
            _log << "Invalid or truncated descriptor record.";
            return false;
        }
        auto descriptor = reinterpret_cast<profiler::SerializedBlockDescriptor*>(data);
        if (descriptor->id() != descriptors.size())
        {
            _log << "Descriptor id does not match its table position.";
            return false;
        }
        descriptors.push_back(descriptor);

        i += sz;
        if (!update_progress(progress, static_cast<int>(100 * i / descriptors_memory_size), _log))
            return false; // Loading interrupted
    }

    if (descriptors.size() != descriptors_count || i != descriptors_memory_size)
    {
        _log << "Descriptor count or memory size does not match the stream header.";
        return false;
    }

    // Preserve the public contract explicitly even though a zero descriptor
    // count is rejected while reading the stream header above.
    return !descriptors.empty();
}

extern "C" PROFILER_API bool readDescriptionsFromStream(std::atomic<int>& progress, std::istream& inStream,
                                                        profiler::SerializedData& serialized_descriptors,
                                                        profiler::descriptors_list_t& descriptors,
                                                        std::ostream& _log)
{
    try
    {
        return readDescriptionsFromStreamImpl(progress, inStream, serialized_descriptors, descriptors, _log);
    }
    catch (const std::exception& e)
    {
        _log << "Reading the descriptions failed: " << e.what();
        return false;
    }
}

//////////////////////////////////////////////////////////////////////////

#undef EASY_FINISH_ASYNC
