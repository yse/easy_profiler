///this
#include "converter.h"
/// reader
#include "reader.h"

#include <fstream>

void JSONConverter::readThreadBlocks(const profiler::reader::BlocksTreeNode &node,nlohmann::json& json)
{
    auto j_local = nlohmann::json::object();

    if(node.current_block != nullptr){
        json = {{"id",static_cast<int>(node.current_block->blockId)}};
        json["start"] = (node.current_block->beginTime);
        json["stop"] = (node.current_block->endTime);
        ///read data from block desciptor
        if(node.current_block->descriptor)
        {
            json["compileTimeName"] = node.current_block->descriptor->compileTimeName;

            std::stringstream stream;
            stream << "0x"
                   << std::hex << node.current_block->descriptor->argbColor;
            std::string result( stream.str() );

            json["color"] = result;
            json["blockType"] = node.current_block->descriptor->blockType;
        }
    }

    auto jsonObjects = nlohmann::json::array();

    for(const auto &value : node.children)
    {
        jsonObjects.push_back(nlohmann::json::object());
        readThreadBlocks(*value.get(),jsonObjects.back());
    }

    json["children"] = jsonObjects;
    return;
}

void JSONConverter::convert()
{
    profiler::reader::FileReader fr;
    fr.readFile(m_file_in);
    const profiler::reader::thread_blocks_tree_t &blocks_tree = fr.getBlocksTreeData();
    nlohmann::json json;
    json["version"] = fr.getVersion();
    json["timeUnit"] = "ns";
    auto jsonObjects = nlohmann::json::array();
    for(const auto &value : blocks_tree)
    {
        jsonObjects.push_back(nlohmann::json::object());
        jsonObjects.back()["threadId"] = value.first;
        jsonObjects.back()["name"] = fr.getThreadName(value.first);
        readThreadBlocks(value.second,jsonObjects.back());

    }
    json["threads"] = jsonObjects;

    if(!m_file_out.empty())
    {
        std::ofstream file(m_file_out);
        file << json;
    }
    else
    {
        ::std::cout << nlohmann::json(json).dump(2);
    }
}
