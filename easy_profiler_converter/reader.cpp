///std
#include <memory>
#include <algorithm>
#include <iostream>
///this
#include "reader.h"

using namespace profiler::reader;
using namespace std;

void FileReader::readFile(const string &filename)
{
    fillTreesFromFile(filename.c_str(), serialized_blocks, serialized_descriptors, m_descriptors, m_blocks,
                      m_threaded_trees, descriptorsNumberInFile, version, true, errorMessage);
    prepareData();
}

const FileReader::TreeNodes &FileReader::getBlocks()
{
    return m_BlocksTree;
}

const FileReader::Events &FileReader::getEvents()
{
    return m_events;
}

const FileReader::ContextSwitches &FileReader::getContextSwitches()
{
    return m_ContextSwitches;
}

void FileReader::prepareData()
{
    vector<uint32_t> tmp_indexes_vector;

    for(auto& kv : m_threaded_trees)
    {
        //children & events are already sorted !!!
        ::std::set_difference(kv.second.children.begin(),kv.second.children.end(),
                              kv.second.events.begin(),kv.second.events.end(),
                              ::std::back_inserter(tmp_indexes_vector));
        for(auto& value : tmp_indexes_vector)
        {
            ::std::shared_ptr<BlocksTreeNode> element = ::std::make_shared<BlocksTreeNode>();
            element->current_block = ::std::make_shared<BlockInfo>();
            element->parent = nullptr;
            element->current_block->thread_name = kv.second.thread_name;
            prepareBlocksInfo(element, value);
            m_BlocksTree.push_back(::std::move(element));
        }
        tmp_indexes_vector.clear();

        prepareEventsInfo(kv.second.events);
        prepareCSInfo(kv.second.sync);
    }
}

void FileReader::prepareBlocksInfo(::std::shared_ptr<BlocksTreeNode> &element, uint32_t Id)
{
    getBlockInfo(element->current_block, Id);

    ///block's children info
    for(auto& value : m_blocks[element->current_block->blockId].children)
    {
        ::std::shared_ptr<BlocksTreeNode> btElement = ::std::make_shared<BlocksTreeNode>();
        btElement->current_block = ::std::make_shared<BlockInfo>();
        btElement->parent = element.get();
        btElement->current_block->thread_name = element->current_block->thread_name;

        element->children.push_back(btElement);
        prepareBlocksInfo(btElement,value);
    }
}



void FileReader::prepareEventsInfo(const ::std::vector<uint32_t>& events)
{
    for(auto Id : events)
    {
        m_events.push_back(::std::make_shared<BlockInfo>());
        getBlockInfo(m_events.back(), Id);
    }
}

void FileReader::prepareCSInfo(const::std::vector<uint32_t> &cs)
{
    for(auto Id : cs)
    {
        m_ContextSwitches.push_back(::std::make_shared<ContextSwitchEvent>());
        m_ContextSwitches.back()->switchName = m_blocks[Id].cs->name();
        m_ContextSwitches.back()->targetThreadId = m_blocks[Id].cs->tid();
        m_ContextSwitches.back()->beginTime = m_blocks[Id].cs->begin();
        m_ContextSwitches.back()->endTime = m_blocks[Id].cs->end();
    }
}

void FileReader::getBlockInfo(shared_ptr<BlockInfo> &current_block, uint32_t Id)
{
    ///block info
    current_block->beginTime = m_blocks[Id].node->begin();
    current_block->endTime = m_blocks[Id].node->end();
    current_block->blockId = m_blocks[Id].node->id();
    current_block->runTimeBlockName = m_blocks[Id].node->name();

    ///descriptor
    current_block->descriptor = ::std::make_shared<BlockDescriptor>();
    current_block->descriptor->argbColor = m_descriptors.at(current_block->blockId)->color();
    current_block->descriptor->lineNumber = m_descriptors.at(current_block->blockId)->line();
    current_block->descriptor->blockId = m_descriptors.at(current_block->blockId)->id();
    current_block->descriptor->blockType = m_descriptors.at(current_block->blockId)->type();
    current_block->descriptor->status = m_descriptors.at(current_block->blockId)->status();
    current_block->descriptor->compileTimeName = m_descriptors.at(current_block->blockId)->name();
    current_block->descriptor->fileName = m_descriptors.at(current_block->blockId)->file();
}
