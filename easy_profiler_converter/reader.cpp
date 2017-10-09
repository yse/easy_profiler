///std
#include <memory>
#include <algorithm>
#include <iostream>
///this
#include "reader.h"

using namespace profiler::reader;

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
    ///todo
}

const FileReader::Events &FileReader::getEventsByThreadName(::std::string thread_name)
{
    ///todo
}

void FileReader::prepareData()
{
    vector<uint32_t> tmp_indexes_vector;

    for(auto& kv : m_threaded_trees)
    {
        sort(kv.second.children.begin(),kv.second.children.end());
        sort(kv.second.events.begin(),kv.second.events.end());
        ::std::set_difference(kv.second.children.begin(),kv.second.children.end(),
                              kv.second.events.begin(),kv.second.events.end(),
                              ::std::back_inserter(tmp_indexes_vector));
        for(auto& value : tmp_indexes_vector)
        {
            ::std::shared_ptr<BlocksTreeNode> element = ::std::make_shared<BlocksTreeNode>();
            element->current_block = ::std::make_shared<InfoBlock>();
            element->parent = nullptr;
            element->current_block->thread_name = kv.second.thread_name;
            makeInfoBlocksTree(element, value);
            m_BlocksTree.push_back(::std::move(element));
        }
        tmp_indexes_vector.clear();
        makeEventsVector(kv.second.events);
    }
}

void FileReader::makeInfoBlocksTree(::std::shared_ptr<BlocksTreeNode> &element, uint32_t Id)
{
    ///block info
    element->current_block->beginTime = m_blocks[Id].node->begin();
    element->current_block->endTime = m_blocks[Id].node->end();
    element->current_block->blockId = m_blocks[Id].node->id();
    element->current_block->runTimeBlockName = m_blocks[Id].node->name();

    ///descriptor
    element->current_block->descriptor = ::std::make_shared<BlockDescriptor>();
    element->current_block->descriptor->argbColor = m_descriptors[element->current_block->blockId]->color();
    element->current_block->descriptor->lineNumber = m_descriptors[element->current_block->blockId]->line();
    element->current_block->descriptor->blockId = m_descriptors[element->current_block->blockId]->id();
    element->current_block->descriptor->blockType = m_descriptors[element->current_block->blockId]->type();
    element->current_block->descriptor->status = m_descriptors[element->current_block->blockId]->status();
    element->current_block->descriptor->compileTimeName = m_descriptors[element->current_block->blockId]->name();
    element->current_block->descriptor->fileName = m_descriptors[element->current_block->blockId]->file();

    ///children block info
    for(auto& value : m_blocks[element->current_block->blockId].children)
    {
        ::std::shared_ptr<BlocksTreeNode> btElement = ::std::make_shared<BlocksTreeNode>();
        btElement->current_block = ::std::make_shared<InfoBlock>();
        btElement->parent = element;
        btElement->current_block->thread_name = element->current_block->thread_name;

        element->children.push_back(btElement);
        makeInfoBlocksTree(btElement,value);
    }
}

void FileReader::makeEventsVector(const ::std::vector<uint32_t>& events)
{
    for(auto Id : events)
    {
        m_events.push_back(::std::make_shared<InfoEvent>());
        m_events.back()->beginTime = m_blocks[Id].cs->begin();
        m_events.back()->endTime = m_blocks[Id].cs->end();
        //m_events.back()->blockId = m_blocks[Id].node->id();
    }
}
