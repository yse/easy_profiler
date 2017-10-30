#ifndef EASY_PROFILER_CONVERTER_H
#define EASY_PROFILER_CONVERTER_H

///std
#include <fstream>
#include <vector>
#include <memory>
#include <sstream>
#include <unordered_map>

///this
#include <easy/easy_protocol.h>
#include <easy/reader.h>

namespace profiler{

namespace reader {

class BlocksTreeNode
{
public:
    ::std::shared_ptr<BlockInfo> current_block;
    BlocksTreeNode* parent;
    ::std::vector<::std::shared_ptr<BlocksTreeNode>> children;

    BlocksTreeNode(BlocksTreeNode&& other)
       : current_block(::std::move(other.current_block)),
         parent(other.parent),
         children(::std::move(other.children))
    {
    }
    BlocksTreeNode():current_block(nullptr),
                     parent(nullptr)
    {
    }
};

typedef ::std::unordered_map<::profiler::thread_id_t, BlocksTreeNode, ::profiler::passthrough_hash<::profiler::thread_id_t> > thread_blocks_tree_t;

class FileReader EASY_FINAL
{
public:
    typedef ::std::vector<::std::shared_ptr<BlocksTreeNode> >      TreeNodes;
    //typedef ::std::unordered_map<::profiler::thread_id_t, ::profiler::BlocksTreeRoot, ::profiler::passthrough_hash<::profiler::thread_id_t> > thread_blocks_tree_t;


    typedef ::std::vector<::std::shared_ptr<ContextSwitchEvent> >  ContextSwitches;
    typedef ::std::vector<::std::shared_ptr<BlockInfo> >           Events;

    FileReader()
    { }

    ~FileReader()
    { }

    void                        readFile(const ::std::string& filename);
    const TreeNodes&            getBlocks();
    ///todo this func. interface depends on data struct
///    const TreeNodes&            getBlocksByThreadName();
    const Events&               getEvents();
    ///todo this func. interface depends on data struct
///    const Events&               getEventsByThreadName(::std::string thread_name);
    const ContextSwitches&      getContextSwitches();
    ///get blocks tree
    const thread_blocks_tree_t&            getBlocksTreeData();

private:
    /*! Operate with data after fillTreesFromFile(...) function call*/
    void               prepareData();

    /*! Organize all InfoBlocks to tree with input BlocksTreeElement as a root
    \param &element root element where we shall insert other elements
    \param Id block id in a common set of blocks
    */
    void               prepareBlocksInfo(::std::shared_ptr<BlocksTreeNode> &element, uint32_t Id);
    void               prepareEventsInfo(const::std::vector<uint32_t> &events);
    void               prepareCSInfo(const::std::vector<uint32_t> &cs);
    void               getBlockInfo(::std::shared_ptr<BlockInfo> &current_block, uint32_t Id);
    ::profiler::block_index_t parseLogInfo(const std::string &filename,
                                           ::std::stringstream& _log);


    ///all data from file(from fillTreesFromFile function)
//    ::profiler::thread_blocks_tree_t                       m_threaded_trees;
//    ::profiler::SerializedData                             serialized_blocks, serialized_descriptors;
//    ::profiler::descriptors_list_t                         m_descriptors;
//    ::profiler::blocks_t                                   m_blocks;
//    ::std::stringstream                                    errorMessage;
//    uint32_t                                               descriptorsNumberInFile;
//    uint32_t                                               version;
    ///
    ::profiler::SerializedData                             serialized_blocks, serialized_descriptors;
    ::std::stringstream                                    errorMessage;
    TreeNodes                                              m_BlocksTree;
    thread_blocks_tree_t                                   m_BlocksTree2;
    ContextSwitches                                        m_ContextSwitches;
    Events                                                 m_events;
    std::vector<std::shared_ptr<BlockDescriptor>>          m_BlockDescriptors;
    uint32_t                                               m_version;
};

} //namespace reader
} //namespace profiler
#endif
