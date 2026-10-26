#include <easy/reader.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace {

int hexDigit(uint8_t value)
{
    if (value >= '0' && value <= '9')
        return value - '0';
    if (value >= 'a' && value <= 'f')
        return value - 'a' + 10;
    if (value >= 'A' && value <= 'F')
        return value - 'A' + 10;
    return -1;
}

bool decodeHexCorpus(const uint8_t* data, size_t size, std::vector<char>& output)
{
    int high = -1;
    bool sawDigit = false;
    for (size_t i = 0; i < size; ++i)
    {
        const uint8_t value = data[i];
        if (value == ' ' || value == '\t' || value == '\r' || value == '\n')
            continue;

        const int digit = hexDigit(value);
        if (digit < 0)
            return false;
        sawDigit = true;
        if (high < 0)
            high = digit;
        else
        {
            output.push_back(static_cast<char>((high << 4) | digit));
            high = -1;
        }
    }
    return sawDigit && high < 0;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::vector<char> decoded;
    const bool isHexFixture = decodeHexCorpus(data, size, decoded);
    const std::string bytes = isHexFixture
        ? std::string(decoded.begin(), decoded.end())
        : (size == 0 ? std::string()
                     : std::string(reinterpret_cast<const char*>(data), size));
    std::istringstream input(bytes, std::ios::in | std::ios::binary);
    std::ostringstream log;
    std::atomic<int> progress(0);

    profiler::BeginEndTime beginEnd = {0, 0};
    profiler::SerializedData serializedBlocks;
    profiler::SerializedData serializedDescriptors;
    profiler::descriptors_list_t descriptors;
    profiler::blocks_t blocks;
    profiler::thread_blocks_tree_t trees;
    profiler::bookmarks_t bookmarks;
    uint32_t descriptorsCount = 0;
    uint32_t version = 0;
    profiler::processid_t pid = 0;

    fillTreesFromStream(progress, input, beginEnd, serializedBlocks,
                        serializedDescriptors, descriptors, blocks, trees,
                        bookmarks, descriptorsCount, version, pid, false, log);
    return 0;
}
