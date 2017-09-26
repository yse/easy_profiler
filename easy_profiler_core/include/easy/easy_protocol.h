#ifndef EASY_PROPROTOCOL____H
#define EASY_PROPROTOCOL____H

///this
#include <easy/serialized_block.h>

///for actual version vistit https://github.com/yse/easy_profiler/wiki/.prof-file-format-v1.3.0

namespace profiler {

namespace reader {

#pragma pack(push, 1)
struct SerializedBlocksInfo         //12
{
    uint32_t total_blocks_count;    //4 bytes
    uint64_t total_blocks_memory;   //8 bytes
};

struct BlocksDescriptorsInfo        //12
{
    uint32_t descriptors_count;     //4 bytes
    uint64_t descriptors_memory;    //8 bytes
};

struct FileHeader                                   //64
{
    uint32_t signature;                             //4
    uint32_t version;                               //4
    uint64_t process_id;                            //8
    int64_t  cpu_frequency;                         //8
    uint64_t begin_time;                            //8
    uint64_t end_time;                              //8
    SerializedBlocksInfo serialized_blocks_info;    //12
    BlocksDescriptorsInfo blocks_descriptor_info;   //12
};

struct ThreadEvents
{

};

#pragma pack(pop)

struct SerializedCSwitch{
    uint64_t begin_time;
    uint64_t end_time;
    uint64_t target_thread_id;
    ///target process ID + name
    char*    data;
};

struct BlocksDescriptor
{
    ::profiler::SerializedBlockDescriptor* descriptor;
    uint16_t size;
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
