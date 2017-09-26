#ifndef EASY_PROFILER_CONVERTER____H
#define EASY_PROFILER_CONVERTER____H
///std
#include <fstream>
#include <vector>
#include <memory>

///this
#include <easy/easy_protocol.h>
#include <easy/reader.h>


using namespace std;

namespace profiler{

namespace reader {

typedef vector<SerializedBlockDescriptor*> descriptors_t;

class FileReader EASY_FINAL
{
public:
    FileReader()
    { }

    ~FileReader()
    {
        close();
    }

    bool               open(const string& filename);
    void               close();
    const bool         is_open() const;

    SerializedBlocksInfo    getSerializedBlocksInfo();
    BlocksDescriptorsInfo   getBlockDescriptorsInfo();
    FileHeader              getFileHeader();
    void                    getSerializedBlockDescriptors(descriptors_t& descriptors);
    void                    getThreadEvents(thread_blocks_tree_t& threaded_trees);
private:
    uint16_t getShiftThreadEvents();
    ifstream           m_file;
    FileHeader         fileHeader;
};

} //namespace reader
} //namespace profiler
#endif
