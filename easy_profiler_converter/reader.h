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

struct BlocksTreeNode
{
    shared_ptr<InfoBlock> current_block;
    shared_ptr<BlocksTreeNode> parent;
    vector<shared_ptr<BlocksTreeNode>> children;
};

class FileReader EASY_FINAL
{
public:
    typedef ::std::vector<shared_ptr<BlocksTreeNode> >  TreeNodes;

    FileReader()
    { }
    ~FileReader()
    { }

    ///call fillTreesFromFile(...) function
    void               readFile(const string& filename);
    const TreeNodes &getInfoBlocks();
protected:
    /*! Operate with data after fillTreesFromFile(...) function call*/
    void               prepareData();

    /*! Organize all InfoBlocks to tree with input BlocksTreeElement as a root
    \param &element root element where we shall insert other elements
    \param Id block id in a common set of blocks
    */
    void               makeInfoBlocksTree(::std::shared_ptr<BlocksTreeNode> &element, uint32_t Id);

private:
    ///all data from file(from fillTreesFromFile function)
    ::profiler::thread_blocks_tree_t                       m_threaded_trees;
    ::profiler::SerializedData                             serialized_blocks, serialized_descriptors;
    ::profiler::descriptors_list_t                         m_descriptors;
    ::profiler::blocks_t                                   m_blocks;
    ::std::stringstream                                    errorMessage;
    uint32_t                                               descriptorsNumberInFile;
    uint32_t                                               version;
    ///
    TreeNodes              m_BlocksTree;
};

} //namespace reader
} //namespace profiler
#endif
