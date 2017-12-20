#ifndef EASY_PROFILER_READER_H
#define EASY_PROFILER_READER_H

///std
#include <fstream>
#include <vector>
#include <memory>
#include <sstream>
#include <string>
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
typedef ::std::unordered_map<::profiler::thread_id_t, ::std::string> thread_names_t;
typedef ::std::vector<::std::shared_ptr<ContextSwitchEvent> >  context_switches_t;

class FileReader EASY_FINAL
{
public:


    FileReader()
    { }

    ~FileReader()
    { }

    /*-----------------------------------------------------------------*/
    ///initial read file with RAW data
    ::profiler::block_index_t readFile(const ::std::string& filename);
    /*-----------------------------------------------------------------*/
    ///get blocks tree
    const thread_blocks_tree_t& getBlocksTreeData();
    /*-----------------------------------------------------------------*/
    /*! get thread name by Id
    \param threadId thread Id
    \return Name of thread
    */
    const std::string &getThreadName(uint64_t threadId);
    /*-----------------------------------------------------------------*/
    /*! get file version
    \return data file version
    */
    uint32_t getVersion();
    /*-----------------------------------------------------------------*/
    ///get sontext switches
    const context_switches_t& getContextSwitches();
    /*-----------------------------------------------------------------*/

private:
    ///serialized raw data
    ::profiler::SerializedData                             serialized_blocks, serialized_descriptors;
    ///error log stream
    ::std::stringstream                                    errorMessage;
    ///thread's blocks
    thread_blocks_tree_t                                   m_BlocksTree;
    ///[thread_id, thread_name]
    thread_names_t                                         m_threadNames;
    ///context switches info
    context_switches_t                                     m_ContextSwitches;
    std::vector<std::shared_ptr<BlockDescriptor>>          m_BlockDescriptors;
    ///data file version
    uint32_t                                               m_version;
};

} //namespace reader
} //namespace profiler
#endif //EASY_PROFILER_READER_H
