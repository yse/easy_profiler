#include <functional>
///this
#include "reader.h"

using namespace profiler::reader;

FileHeader FileReader::getFileHeader()
{
    FileHeader fh;
    if(is_open())
    {
        m_file.seekg(0, ios_base::beg);
        m_file.read((char*)&fh, sizeof(fh));
    }
    return fh;
}

void FileReader::getSerializedBlockDescriptors(descriptors_t& descriptors)
{
    if(is_open())
    {
        descriptors.clear();
        //get block descriptors count
        BlocksDescriptorsInfo bdi = getBlockDescriptorsInfo();
        descriptors.reserve(bdi.descriptors_count);

        m_file.seekg(0);
        m_file.seekg(EASY_SHIFT_BLOCK_DESCRIPTORS,ios::cur);
        while(descriptors.size() < bdi.descriptors_count)
        {
            uint16_t size;
            m_file.read((char*)&size,sizeof(size));
            char* data;
            m_file.read(data, size);
            descriptors.push_back(reinterpret_cast<::profiler::SerializedBlockDescriptor*>(data));
        }
    }
}

void FileReader::getThreadEvents(profiler::thread_blocks_tree_t &threaded_trees)
{
    if(is_open())
    {
        threaded_trees.clear();
        m_file.seekg(0);
    }
}

uint16_t FileReader::getShiftThreadEvents()
{
    uint16_t shift_size = 0;
    if(is_open())
    {
        //get block descriptors count
        BlocksDescriptorsInfo bdi = getBlockDescriptorsInfo();

        m_file.seekg(0);
        m_file.seekg(EASY_SHIFT_BLOCK_DESCRIPTORS,ios::cur);
        for(auto i = 0; i < bdi.descriptors_count; i++)
        {
            uint16_t size;
            m_file.read((char*)&size,sizeof(size));
            shift_size += sizeof(uint16_t);
            shift_size += size;
            m_file.seekg(size,ios::cur);
        }
    }
    return shift_size;
}

BlocksDescriptorsInfo FileReader::getBlockDescriptorsInfo()
{
    BlocksDescriptorsInfo bdi;
    if(is_open())
    {
        m_file.seekg(0);
        m_file.seekg(EASY_SHIFT_BLOCK_DESCRIPTORS_INFO);
        m_file.read((char*)&bdi, sizeof(bdi));
    }
    return bdi;
}

bool FileReader::open(const string &filename)
{
    m_file.open(filename,ifstream::binary);
    return m_file.good();
}

void FileReader::close()
{
    m_file.close();
}

const bool FileReader::is_open() const
{
    return m_file.is_open() && m_file.good();
}

SerializedBlocksInfo FileReader::getSerializedBlocksInfo()
{
    SerializedBlocksInfo sbi;
    if(is_open())
    {
        m_file.seekg(0, ios_base::beg);
        m_file.seekg(EASY_SHIFT_SERIALIZES_BLOCKS_INFO,m_file.end);
        m_file.read((char*)&sbi, sizeof(sbi));
    }
    return sbi;
}

