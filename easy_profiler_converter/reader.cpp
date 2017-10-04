///std
#include <memory>
#include <algorithm>
#include <iostream>
///this
#include "reader.h"

using namespace profiler::reader;

FileHeader FileReader::getFileHeader()
{
    FileHeader fh;
    //    if(is_open())
    //    {
    //        m_file.seekg(0, ios_base::beg);
    //        m_file.read((char*)&fh, sizeof(fh));
    //    }
    return fh;
}

//void FileReader::getSerializedBlockDescriptors(descriptors_t& descriptors)
//{
//    if(is_open())
//    {
//        descriptors.clear();
//        //get block descriptors count
//        BlockDescriptorsInfo bdi = getBlockDescriptorsInfo();
//        descriptors.reserve(bdi.descriptors_count);

//        m_file.seekg(0);
//        m_file.seekg(EASY_SHIFT_BLOCK_DESCRIPTORS,ios::cur);
//        while(descriptors.size() < bdi.descriptors_count)
//        {
//            uint16_t size;
//            m_file.read((char*)&size,sizeof(size));
//            char* data;
//            m_file.read(data, size);
//            descriptors.push_back(reinterpret_cast<::profiler::SerializedBlockDescriptor*>(data));
//        }
//    }
//}

//void FileReader::getThreadEvents(profiler::thread_blocks_tree_t &threaded_trees)
//{
//    if(is_open())
//    {
//        threaded_trees.clear();
//        m_file.seekg(0);
//    }
//}


//uint16_t FileReader::getShiftThreadEvents()
//{
//    uint16_t shift_size = 0;
//    if(is_open())
//    {
//        //get block descriptors count
//        BlockDescriptorsInfo bdi = getBlockDescriptorsInfo();

//        m_file.seekg(0);
//        m_file.seekg(EASY_SHIFT_BLOCK_DESCRIPTORS,ios::cur);
//        for(auto i = 0; i < bdi.descriptors_count; i++)
//        {
//            uint16_t size;
//            m_file.read((char*)&size,sizeof(size));
//            shift_size += sizeof(uint16_t);
//            shift_size += size;
//            m_file.seekg(size,ios::cur);
//        }
//    }
//    return shift_size;
//}

//BlockDescriptorsInfo FileReader::getBlockDescriptorsInfo()
//{
//    BlockDescriptorsInfo bdi;
//    if(is_open())
//    {
//        m_file.seekg(0);
//        m_file.seekg(EASY_SHIFT_BLOCK_DESCRIPTORS_INFO);
//        m_file.read((char*)&bdi, sizeof(bdi));
//    }
//    return bdi;
//}

//bool FileReader::open(const string &filename)
//{

////    auto blocks_counter = fillTreesFromFile(filename.c_str(), serialized_blocks, serialized_descriptors, descriptors, blocks,
////                                            threaded_trees, descriptorsNumberInFile, version, true, errorMessage);
//    m_file.open(filename,ifstream::binary);
//    return m_file.good();
//}

//void FileReader::close()
//{
//    m_file.close();
//}

//const bool FileReader::is_open() const
//{
//    return m_file.is_open() && m_file.good();
//}

void FileReader::readFile(const string &filename)
{
    fillTreesFromFile(filename.c_str(), serialized_blocks, serialized_descriptors, m_descriptors, m_blocks,
                      m_threaded_trees, descriptorsNumberInFile, version, true, errorMessage);
    prepareData();
}

void FileReader::getInfoBlocks(std::vector<InfoBlock> &blocks)
{
    auto itr = m_blocks.begin();
    for(itr;itr != m_blocks.end();itr++)
    {
        int f=0;
        //        InfoBlock block;
        //        block.beginTime = itr->
    }
}

void FileReader::prepareData()
{
    vector<uint32_t> indexes_vector;

    for(auto& kv : m_threaded_trees)
    {
        sort(kv.second.children.begin(),kv.second.children.end());
        sort(kv.second.events.begin(),kv.second.events.end());
        ::std::set_difference(kv.second.children.begin(),kv.second.children.end(),
                              kv.second.events.begin(),kv.second.events.end(),
                              ::std::back_inserter(indexes_vector));
        for(auto& value : indexes_vector)
        {
            ::std::shared_ptr<BlocksTreeElement> element = ::std::make_shared<BlocksTreeElement>();
            element->current_block = ::std::make_shared<InfoBlock>();
            element->parent = nullptr;
            element->current_block->thread_name = kv.second.thread_name;
            addInfoBlocks(element, value);
            m_BlocksTree.push_back(::std::move(element));
        }
        indexes_vector.clear();
    }
}

void FileReader::addInfoBlocks(::std::shared_ptr<BlocksTreeElement>& element,uint32_t Id)
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
        ::std::shared_ptr<BlocksTreeElement> btElement = ::std::make_shared<BlocksTreeElement>();
        btElement->current_block = ::std::make_shared<InfoBlock>();
        btElement->parent = element;
        btElement->current_block->thread_name = element->current_block->thread_name;

        element->children.push_back(btElement);
        addInfoBlocks(btElement,value);
    }
}
