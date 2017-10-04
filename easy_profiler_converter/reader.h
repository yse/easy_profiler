#ifndef EASY_PROFILER_CONVERTER____H
#define EASY_PROFILER_CONVERTER____H
///std
#include <fstream>
#include <vector>
#include <memory>
#include <sstream>
#include <unordered_map>

///this
#include <easy/easy_protocol.h>
#include <easy/reader.h>

using namespace std;

namespace profiler{

namespace reader {

typedef vector<SerializedBlockDescriptor> descriptors_t;
typedef std::unordered_map<thread_id_t, InfoBlock, ::profiler::passthrough_hash<::profiler::thread_id_t> > thread_info_blocks_tree_t;

struct BlocksTreeElement
{
    shared_ptr<InfoBlock> current_block;
    shared_ptr<BlocksTreeElement> parent;
    vector<shared_ptr<BlocksTreeElement>> children;
};

class FileReader EASY_FINAL
{
public:
    FileReader()
    { }

    ~FileReader()
    { }

    bool               open(const string& filename);
    void               close();
    const bool         is_open() const;

    void               readFile(const string& filename);

    FileHeader         getFileHeader();
    void               getInfoBlocks(std::vector<InfoBlock>& blocks);

private:
    void               prepareData();
    void               addInfoBlocks(::std::shared_ptr<BlocksTreeElement> &element, uint32_t Id);
    ifstream           m_file;
    FileHeader         fileHeader;
    ///all data from file(from fillTreesFromFile function)
    ::profiler::thread_blocks_tree_t    m_threaded_trees;
    ::profiler::SerializedData          serialized_blocks, serialized_descriptors;
    ::profiler::descriptors_list_t      m_descriptors;
    ::profiler::blocks_t                m_blocks;
    ::std::stringstream                 errorMessage;
    uint32_t                            descriptorsNumberInFile;
    uint32_t                            version;
    ///
    thread_info_blocks_tree_t           m_InfoBlocksTree;
    vector<InfoBlock>                   m_InfoBlocks;
    vector<::std::shared_ptr<BlocksTreeElement>>           m_BlocksTree;
};

} //namespace reader
} //namespace profiler
#endif
