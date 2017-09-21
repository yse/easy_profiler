#ifndef EASY_PROFILER_CONVERTER____H
#define EASY_PROFILER_CONVERTER____H

///this
#include <easy/serialized_block.h>

using namespace std;

namespace reader {



#pragma pack(push, 1)
struct SerializedBlocksInfo
{
    uint32_t total_blocks_count;
    uint64_t total_blocks_memory;
};

struct BlocksDescriptorsInfo
{
    uint32_t descriptors_count;
    uint64_t descriptors_memory;
};

struct FileHeader
{
  uint32_t signature;
  uint32_t version;
  uint64_t process_id;
  int64_t  cpu_frequency;
  uint64_t begin_time;
  uint64_t end_time;
  SerializedBlocksInfo serialized_blocks_info;
  BlocksDescriptorsInfo blocks_descriptor_info;
};

struct BlocksDescriptors
{
    uint16_t size;
    ::profiler::SerializedBlockDescriptor* descriptor;
};

#pragma pack(pop)



struct IReader
{
    virtual FileHeader         getFileHeader() = 0;
    virtual BlocksDescriptors  getBlockDescriptors() = 0;
};

class SimpleReader EASY_FINAL : public IReader
{
public:
    SimpleReader(std::string inputFilePath): inputFilePath("")
    { }

    FileHeader         getFileHeader() override;
    BlocksDescriptors  getBlockDescriptors() override;
private:
    std::string        inputFilePath;
    FileHeader         fileHeader;
    BlocksDescriptors  blocksDescriptors;
};

} //reader

#endif
