///this
#include "converter.h"
/// reader
#include "reader.h"

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

void to_json(nlohmann::json& j, const json_node_t& node) {
    j = {{"ID", node.id}};
    if (!node.child.empty())
        j.push_back({"children", node.child});
}



json_node_t& json_node_t::add(const json_node_t& node) {
    child.push_back(node);
    return child.back();
}

json_node_t& json_node_t::add(const initializer_list<json_node_t>& nodes) {
    child.insert(child.end(), nodes);
    return child.back();
}


void JSONConverter::convert()
{
    profiler::reader::FileReader fr;
    fr.readFile(m_file_in);
    const profiler::reader::thread_blocks_tree_t &blocks_tree = fr.getBlocksTreeData();
    json_node_t node_root(0);
    nlohmann::json j;
    j["version"] = fr.getVersion();
    j["timeUnit"] = "ns";
    auto jsonObjects = nlohmann::json::array();
    for(const auto &value : blocks_tree)
    {
        jsonObjects.push_back(nlohmann::json::object());
        jsonObjects.back()["threadId"] = value.first;
        jsonObjects.back()["name"] = fr.getThreadName(value.first);        
         readThreadBlocks(value.second,jsonObjects.back());

    }
    j["threads"] = jsonObjects;

     cout << nlohmann::json(j).dump(2) << endl;
    return;
}
