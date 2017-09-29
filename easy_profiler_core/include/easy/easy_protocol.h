#ifndef EASY_PROPROTOCOL____H
#define EASY_PROPROTOCOL____H
///C++
#include <string>
#include <vector>
#include <memory>
///this
#include <easy/serialized_block.h>

///for actual version vistit https://github.com/yse/easy_profiler/wiki/.prof-file-format-v1.3.0

namespace profiler {

namespace reader {

struct BlockDescriptor;

struct BlocksInfo         //12
{
    uint32_t totalBlocksCount;    //4 bytes
    uint64_t totalBlocksMemory;   //8 bytes
};

struct DescriptorsInfo        //12
{
    uint32_t allDescriptorsCount;     //4 bytes
    uint64_t allDescriptorsMemory;    //8 bytes
};

struct FileHeader                                   //64
{
    uint32_t                signature;                             //4
    uint32_t                version;                               //4
    uint64_t                processId;                            //8
    int64_t                 cpuFrequency;                         //8
    uint64_t                beginTime;                            //8
    uint64_t                endTime;                              //8
    BlocksInfo              serializedBlocksInfo;    //12
    DescriptorsInfo    blocksDescriptorInfo;   //12
};

struct InfoBlock
{
    uint64_t                         beginTime;
    uint64_t                         endTime;
    uint32_t                         blockId;
    std::string                      runTimeBlockName;
    std::shared_ptr<BlockDescriptor> descriptor;
};

struct ContextSwitchEvent{
    uint64_t                            beginTime;
    uint64_t                            endTime;
    uint64_t                            targetThreadId;
    std::string                         targetProcessIdName;
    std::shared_ptr<BlockDescriptor>    descriptor;
};

struct BlockDescriptor
{
    uint32_t        blockId;
    int             lineNumber;
    uint32_t        argbColor;
    uint8_t         blockType;
    uint8_t         status;
    std::string     compileTimeName;
    std::string     fileName;
};

struct ThreadEventsAndBlocks
{
    uint64_t                            threadId;
    std::string                         threadName;
    std::vector<ContextSwitchEvent>     switchEvents;
    std::vector<InfoBlock>              infoBlocks;
};

} //namespace reader
} //namespace profiler

#endif
