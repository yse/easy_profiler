/** Building and corrupting .prof files for reader robustness tests.

The header layout is defined in ProfileManager::dumpBlocksToStream
(easy_profiler_core/profile_manager.cpp:148-175) and repeated in writer.cpp:501-518:

    offset  type     field
    0       uint32   signature ('Easy' == 0x45617379)
    4       uint32   version   (major<<24 | minor<<16 | patch)
    8       uint64   pid
    16      int64    cpu_frequency
    24      uint64   begin_time
    32      uint64   end_time
    40      uint64   used_memory_size        (block stream)
    48      uint64   descriptors_memory_size (descriptor stream)
    56      uint32   total_blocks_number
    60      uint32   total_descriptors_number
    64      uint32   threads_count
    68      uint16   bookmarks_count
    70      uint16   padding
    --------------------------------------------------------
    72      end of header
*/

#ifndef EASY_PROFILER_TESTS_PROF_IMAGE_H
#define EASY_PROFILER_TESTS_PROF_IMAGE_H

#include "support/prof_fixture.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace easy_test {

struct ProfHeader
{
    static const size_t Size = 72;

    static const size_t OffsetSignature             = 0;
    static const size_t OffsetVersion               = 4;
    static const size_t OffsetPid                   = 8;
    static const size_t OffsetCpuFrequency          = 16;
    static const size_t OffsetBeginTime             = 24;
    static const size_t OffsetEndTime               = 32;
    static const size_t OffsetUsedMemorySize        = 40;
    static const size_t OffsetDescriptorsMemorySize = 48;
    static const size_t OffsetTotalBlocks           = 56;
    static const size_t OffsetTotalDescriptors      = 60;
    static const size_t OffsetThreadsCount          = 64;
    static const size_t OffsetBookmarksCount        = 68;
};

/** A byte image of a .prof file that can be edited and written back to disk. */
class ProfImage
{
public:
    ProfImage() = default;

    bool loadFrom(const std::string& path)
    {
        std::ifstream file(path.c_str(), std::ios::binary);
        if (!file.good())
            return false;
        m_bytes.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        return !m_bytes.empty();
    }

    /** Take a valid dump of the current profiler state and load its image. */
    template <class F>
    bool captureFrom(F&& fn)
    {
        drainPending();
        TempFile file("builder");
        profiler::setEnabled(true);
        fn();
        profiler::setEnabled(false);
        if (profiler::dumpBlocksToFile(file.c_str()) == 0)
            return false;
        return loadFrom(file.path());
    }

    void writeTo(const std::string& path) const
    {
        std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);
        if (!m_bytes.empty())
            file.write(m_bytes.data(), static_cast<std::streamsize>(m_bytes.size()));
    }

    size_t size() const { return m_bytes.size(); }
    bool empty() const { return m_bytes.empty(); }

    void truncateTo(size_t newSize)
    {
        if (newSize < m_bytes.size())
            m_bytes.resize(newSize);
    }

    template <class T>
    T read(size_t offset) const
    {
        T value = T();
        if (offset + sizeof(T) <= m_bytes.size())
            std::memcpy(&value, m_bytes.data() + offset, sizeof(T));
        return value;
    }

    template <class T>
    void write(size_t offset, T value)
    {
        if (offset + sizeof(T) <= m_bytes.size())
            std::memcpy(m_bytes.data() + offset, &value, sizeof(T));
    }

    uint32_t signature() const               { return read<uint32_t>(ProfHeader::OffsetSignature); }
    uint32_t version() const                 { return read<uint32_t>(ProfHeader::OffsetVersion); }
    uint64_t descriptorsMemorySize() const   { return read<uint64_t>(ProfHeader::OffsetDescriptorsMemorySize); }
    uint64_t usedMemorySize() const          { return read<uint64_t>(ProfHeader::OffsetUsedMemorySize); }
    uint32_t totalBlocks() const             { return read<uint32_t>(ProfHeader::OffsetTotalBlocks); }
    uint32_t totalDescriptors() const        { return read<uint32_t>(ProfHeader::OffsetTotalDescriptors); }
    uint32_t threadsCount() const            { return read<uint32_t>(ProfHeader::OffsetThreadsCount); }

    void setSignature(uint32_t v)             { write<uint32_t>(ProfHeader::OffsetSignature, v); }
    void setVersion(uint32_t v)               { write<uint32_t>(ProfHeader::OffsetVersion, v); }
    void setDescriptorsMemorySize(uint64_t v) { write<uint64_t>(ProfHeader::OffsetDescriptorsMemorySize, v); }
    void setUsedMemorySize(uint64_t v)        { write<uint64_t>(ProfHeader::OffsetUsedMemorySize, v); }
    void setTotalBlocks(uint32_t v)           { write<uint32_t>(ProfHeader::OffsetTotalBlocks, v); }
    void setTotalDescriptors(uint32_t v)      { write<uint32_t>(ProfHeader::OffsetTotalDescriptors, v); }
    void setThreadsCount(uint32_t v)          { write<uint32_t>(ProfHeader::OffsetThreadsCount, v); }

private:
    std::vector<char> m_bytes;
};

/** Pack a format version the same way the profiler writes it. */
inline uint32_t makeVersion(uint8_t major, uint8_t minor, uint16_t patch)
{
    return (static_cast<uint32_t>(major) << 24) |
           (static_cast<uint32_t>(minor) << 16) |
            static_cast<uint32_t>(patch);
}

/** A small workload producing a valid .prof with predictable structure. */
inline void sampleWorkload()
{
    EASY_BLOCK("builder_outer");
    {
        EASY_BLOCK("builder_inner");
    }
}

} // namespace easy_test

#endif // EASY_PROFILER_TESTS_PROF_IMAGE_H
