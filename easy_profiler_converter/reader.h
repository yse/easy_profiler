#ifndef EASY_PROFILER_CONVERTER____H
#define EASY_PROFILER_CONVERTER____H

///this
#include <easy/serialized_block.h>
#include <easy/easy_protocol.h>

using namespace std;

namespace profiler{

namespace reader {

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

} //namespace reader
} //namespace profiler
#endif
