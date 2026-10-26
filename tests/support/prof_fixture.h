/** Test harness for the easy_profiler core.

The profiler is a process-wide singleton, so tests must share state explicitly.
The key tool here is capture(): it discards whatever the previous test left
behind, enables capture, runs the body, dumps to a temporary file and reads it
back. After capture() the profiler is always disabled and the storage is empty.
*/

#ifndef EASY_PROFILER_TESTS_PROF_FIXTURE_H
#define EASY_PROFILER_TESTS_PROF_FIXTURE_H

#include <easy/profiler.h>
#include <easy/reader.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
# include <process.h>
#else
# include <unistd.h>
#endif

namespace easy_test {

//////////////////////////////////////////////////////////////////////////
// Temporary file

inline unsigned long processId()
{
#ifdef _WIN32
    return static_cast<unsigned long>(::_getpid());
#else
    return static_cast<unsigned long>(::getpid());
#endif
}

inline std::string tempDirectory()
{
#ifdef _WIN32
    const char* directory = std::getenv("TEMP");
    if (directory == nullptr || *directory == '\0')
        directory = std::getenv("TMP");
#else
    const char* directory = std::getenv("TMPDIR");
#endif

    return directory != nullptr && *directory != '\0' ? directory : ".";
}

inline std::string uniqueName(const char* stem, const char* extension)
{
    static std::atomic<unsigned> counter(0);
    std::ostringstream stream;
    const std::string directory = tempDirectory();
    stream << directory;
    if (!directory.empty() && directory.back() != '/' && directory.back() != '\\')
        stream << '/';
    stream << "easy_profiler_" << stem << '_' << processId() << '_'
           << counter.fetch_add(1) << extension;
    return stream.str();
}

class TempFile
{
public:
    explicit TempFile(const char* stem, const char* extension = ".prof")
        : m_path(uniqueName(stem, extension))
    {
    }

    TempFile(const TempFile&) = delete;
    TempFile& operator = (const TempFile&) = delete;

    ~TempFile() { std::remove(m_path.c_str()); }

    const std::string& path() const { return m_path; }
    const char* c_str() const { return m_path.c_str(); }

    bool exists() const
    {
        std::ifstream file(m_path.c_str(), std::ios::binary);
        return file.good();
    }

    uint64_t sizeBytes() const
    {
        std::ifstream file(m_path.c_str(), std::ios::binary | std::ios::ate);
        if (!file.good())
            return 0;
        return static_cast<uint64_t>(file.tellg());
    }

private:
    std::string m_path;
};

//////////////////////////////////////////////////////////////////////////
// A loaded profile

struct LoadedProfile
{
    profiler::SerializedData      serializedBlocks;
    profiler::SerializedData      serializedDescriptors;
    profiler::descriptors_list_t  descriptors;
    profiler::blocks_t            blocks;
    profiler::thread_blocks_tree_t threadedTrees;
    profiler::bookmarks_t         bookmarks;
    profiler::BeginEndTime        beginEndTime;

    uint32_t                  descriptorsCount = 0;
    uint32_t                  version = 0;
    profiler::processid_t     pid = 0;
    profiler::block_index_t   blocksRead = 0;   ///< blocks returned by the reader
    uint32_t                  blocksDumped = 0; ///< blocks returned by dumpBlocksToFile
    std::string               readerLog;        ///< a non-empty log means a parse error

    LoadedProfile() { beginEndTime.beginTime = 0; beginEndTime.endTime = 0; }

    LoadedProfile(const LoadedProfile&) = delete;
    LoadedProfile& operator = (const LoadedProfile&) = delete;
};

/** Read a .prof into out. Never throws: parse errors land in out.readerLog.

gatherStatistics turns on per-block statistics (per_parent_stats, per_frame_stats,
per_thread_stats). That is a separate half of the reader, thread pool and median
computation included, so it has tests of its own.
*/
inline void loadProfile(const std::string& path, LoadedProfile& out, bool gatherStatistics = false)
{
    std::stringstream log;
    out.blocksRead = fillTreesFromFile(path.c_str(), out.beginEndTime, out.serializedBlocks,
                                       out.serializedDescriptors, out.descriptors, out.blocks,
                                       out.threadedTrees, out.bookmarks, out.descriptorsCount,
                                       out.version, out.pid, gatherStatistics, log);
    out.readerLog = log.str();
}

/** Discard everything previous tests accumulated.

Without this, one test's blocks leak into the next test's dump.
*/
inline void drainPending()
{
    profiler::setEnabled(false);
    TempFile sink("drain");
    profiler::dumpBlocksToFile(sink.c_str());
}

/** Enable capture, run fn, dump and read the result back. */
template <class F>
void capture(LoadedProfile& out, F&& fn, bool gatherStatistics = false)
{
    drainPending();

    TempFile file("capture");

    profiler::setEnabled(true);
    fn();
    // dumpBlocksToFile disables the profiler itself; doing it explicitly keeps
    // anything between the test body and the dump out of the capture.
    profiler::setEnabled(false);

    out.blocksDumped = profiler::dumpBlocksToFile(file.c_str());
    if (out.blocksDumped != 0)
        loadProfile(file.path(), out, gatherStatistics);
}

//////////////////////////////////////////////////////////////////////////
// Lookups inside a loaded profile

inline const profiler::SerializedBlockDescriptor*
findDescriptor(const LoadedProfile& profile, profiler::BlockType type, const char* name)
{
    for (size_t i = 0; i < profile.descriptors.size(); ++i)
    {
        const profiler::SerializedBlockDescriptor* descriptor = profile.descriptors[i];
        if (descriptor == nullptr)
            continue;
        if (descriptor->type() == type && std::strcmp(descriptor->name(), name) == 0)
            return descriptor;
    }
    return nullptr;
}

/** The display name of a block.

A block with a compile-time name has an empty runtime name and its real name
lives in the descriptor. A block with a runtime name stores the name in the
record itself.
*/
inline const char* blockName(const LoadedProfile& profile, profiler::block_index_t index)
{
    const profiler::BlocksTree& entry = profile.blocks[index];
    if (entry.node == nullptr)
        return "";

    const char* runtimeName = entry.node->name();
    if (runtimeName != nullptr && *runtimeName != 0)
        return runtimeName;

    // The bounds check is mandatory. The flat blocks array also holds context
    // switch records, and their m_id field does NOT hold a descriptor id:
    // beginContextSwitch writes the target thread id there (the "Dirty hack" in
    // profile_manager.cpp). Indexing the descriptor table with that value runs
    // off the end of the array.
    const profiler::block_id_t id = entry.node->id();
    if (id >= profile.descriptors.size())
        return "";

    const profiler::SerializedBlockDescriptor* descriptor = profile.descriptors[id];
    return descriptor != nullptr ? descriptor->name() : "";
}

inline const profiler::ArbitraryValue* findValue(const LoadedProfile& profile, const char* name)
{
    for (size_t i = 0; i < profile.blocks.size(); ++i)
    {
        const profiler::BlocksTree& entry = profile.blocks[i];
        if (entry.node == nullptr)
            continue;

        // See the note in blockName(): for context switch records m_id holds a
        // thread id rather than a descriptor id.
        const profiler::block_id_t id = entry.node->id();
        if (id >= profile.descriptors.size())
            continue;

        const profiler::SerializedBlockDescriptor* descriptor = profile.descriptors[id];
        if (descriptor == nullptr || descriptor->type() != profiler::BlockType::Value)
            continue;

        if (std::strcmp(descriptor->name(), name) == 0)
            return entry.value;
    }
    return nullptr;
}

/** Index of the first block with the given name, or -1. */
inline long findBlockIndex(const LoadedProfile& profile, const char* name)
{
    for (size_t i = 0; i < profile.blocks.size(); ++i)
    {
        if (profile.blocks[i].node == nullptr)
            continue;
        if (std::strcmp(blockName(profile, static_cast<profiler::block_index_t>(i)), name) == 0)
            return static_cast<long>(i);
    }
    return -1;
}

/** Names of a block's direct children, in storage order. */
inline std::vector<std::string> childNames(const LoadedProfile& profile, profiler::block_index_t index)
{
    std::vector<std::string> result;
    const profiler::BlocksTree& entry = profile.blocks[index];
    result.reserve(entry.children.size());
    for (size_t i = 0; i < entry.children.size(); ++i)
        result.push_back(blockName(profile, entry.children[i]));
    return result;
}

/** Names of the root blocks of the single thread. Handy for topology checks. */
inline std::vector<std::string> rootNames(const LoadedProfile& profile)
{
    std::vector<std::string> result;
    if (profile.threadedTrees.empty())
        return result;

    const profiler::BlocksTreeRoot& root = profile.threadedTrees.begin()->second;
    result.reserve(root.children.size());
    for (size_t i = 0; i < root.children.size(); ++i)
        result.push_back(blockName(profile, root.children[i]));
    return result;
}

/** How many blocks with the given name the profile contains.

More robust than comparing total block counts: EASY_THREAD_SCOPE adds one
"ThreadFinished" event per thread (profile_manager.cpp:243), so the grand total
depends on how threads were registered.
*/
inline size_t countBlocksNamed(const LoadedProfile& profile, const char* name)
{
    size_t count = 0;
    for (size_t i = 0; i < profile.blocks.size(); ++i)
    {
        if (profile.blocks[i].node == nullptr)
            continue;
        if (std::strcmp(blockName(profile, static_cast<profiler::block_index_t>(i)), name) == 0)
            ++count;
    }
    return count;
}

/** How many blocks in total (nested included) were actually parsed. */
inline size_t totalBlocks(const LoadedProfile& profile)
{
    size_t count = 0;
    for (size_t i = 0; i < profile.blocks.size(); ++i)
    {
        if (profile.blocks[i].node != nullptr)
            ++count;
    }
    return count;
}

} // namespace easy_test

#endif // EASY_PROFILER_TESTS_PROF_FIXTURE_H
