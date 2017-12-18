///nlohmann json
#include <iostream>

///this
#include "converter.h"

/// reader
#include "reader.h"

#include <utility>      // std::pair
#include <string>

void JSONConverter::readThreadBlocks(const profiler::reader::BlocksTreeNode &node,nlohmann::json& json)
{
    auto j_local = nlohmann::json::object();

    if(node.current_block != nullptr){
        json = {{"id",static_cast<int>(node.current_block->blockId)}};
        json["start(ns)"] = (node.current_block->beginTime);
        json["stop(ns)"] = (node.current_block->endTime);
        if(node.current_block->descriptor->compileTimeName != "")
            json["compileTimeName"] = node.current_block->descriptor->compileTimeName;
    }
    else{
        //json = {{"id",0}};
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
    j["root"] = "root";
    //json.insert(json.begin(),node);
    auto jsonObjects = nlohmann::json::array();
    for(const auto &value : blocks_tree)
    {
       // j["threads"].add() push_back({"threadId", value.first});
        //jsonObjects.insert(jsonObjects.begin(),nlohmann::json::object());// push_back(nlohmann::json::object());
        jsonObjects.push_back(nlohmann::json::object());
        jsonObjects.back() = {{"threadId", value.first}};
        //nlohmann::json jj_test = nlohmann::json::array({"threadId", value.first});
        //j["threads"].push_back(jj_test);
        //j["threads"]["threadId"] =  value.first;
        //j["threads"]["threadId"].push_back({value.first});
       // readThreadBlocks(value.second,j["threads"][j["threads"].size()-1]);
         readThreadBlocks(value.second,jsonObjects.back());

    }
    j["threads"] = jsonObjects;

     cout << nlohmann::json(j).dump(2) << endl;
    return;
}
