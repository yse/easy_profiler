#ifndef EASY_PROPROTOCOL____H
#define EASY_PROPROTOCOL____H
///C++
#include <string>
#include <vector>
///this
#include <easy/serialized_block.h>

///for actual version vistit https://github.com/yse/easy_profiler/wiki/.prof-file-format-v1.3.0

namespace profiler {

namespace reader {

#pragma pack(push, 1)

struct InfoBlock
{
    uint64_t beginTime;
    uint64_t endTime;
    uint32_t blockId;
    std::string runTimeBlockName;
};

struct ContextSwitchEvent{
    uint64_t        beginTime;
    uint64_t        endTime;
    uint64_t        targetThreadId;
    std::string     targetProcessIdName;
};

struct BlockDescriptor
{
    uint32_t blockId;
    int      lineNumber;
    uint32_t argbColor;
    uint8_t  blockType;
    uint8_t  status;
    std::string compileTimeName;
    std::string fileName;
};

struct SerializedBlocksInfo         //12
{
    uint32_t totalBlocksCount;    //4 bytes
    uint64_t totalBlocksMemory;   //8 bytes
};

struct BlocksDescriptorsInfo        //12
{
    uint32_t descriptorsCount;     //4 bytes
    uint64_t descriptorsMemory;    //8 bytes
};

struct FileHeader                                   //64
{
    uint32_t signature;                             //4
    uint32_t version;                               //4
    uint64_t processId;                            //8
    int64_t  cpuFrequency;                         //8
    uint64_t beginTime;                            //8
    uint64_t endTime;                              //8
    SerializedBlocksInfo serializedBlocksInfo;    //12
    BlocksDescriptorsInfo blocksDescriptorInfo;   //12
};

#pragma pack(pop)

struct ThreadEventsAndBlocks
{
    uint64_t threadId;
    std::string threadName;
    std::vector<ContextSwitchEvent> switchEvents;
    std::vector<InfoBlock> switchEvents;
};

} //namespace reader
} //namespace profiler


#define EASY_SHIFT_BLOCK_DESCRIPTORS sizeof(FileHeader)

#define EASY_SHIFT_BLOCK_DESCRIPTORS_INFO (sizeof(FileHeader) -\
                                           sizeof(BlocksDescriptorsInfo))

#define EASY_SHIFT_SERIALIZES_BLOCKS_INFO (sizeof(FileHeader) -\
                                           sizeof(BlocksDescriptorsInfo) -\
                                           sizeof(SerializedBlocksInfo))



#endif
