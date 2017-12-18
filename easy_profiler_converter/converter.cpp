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
    }
    else{
        json = {{"id",0}};
    }

    auto jsonObjects = nlohmann::json::array();

    for(const auto &value : node.children)
    {
        jsonObjects.push_back(nlohmann::json::object());
        readThreadBlocks(*value.get(),jsonObjects.back());
    }

    json["children_list"] = jsonObjects;
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
//    auto iter = blocks_tree.begin();
    //json.push_back("root");

//    auto jsonObjects = nlohmann::json::array();
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
//    // create an empty structure (null)
//    nlohmann::json j;

//    // add a number that is stored as double (note the implicit conversion of j to an object)
//    j["pi"] = 3.141;

//    // add a Boolean that is stored as bool
//    j["happy"] = true;

//    // add a string that is stored as std::string
//    j["name"] = "Niels";

//    // add another null object by passing nullptr
//    j["nothing"] = nullptr;

//    // add an object inside the object
//    j["answer"]["everything"] = 42;

//    // add an array that is stored as std::vector (using an initializer list)
//    j["list"] = { 1 };
//    j["list"].insert(j["list"].end(),{2});



//    // add another object (using an initializer list of pairs)
//    j["object"] = { {"currency", "USD"}, {"value", 42.99} };

// cout << nlohmann::json(j).dump(2) << endl;
//    return;


//    profiler::reader::FileReader fr;
//    fr.readFile(m_file_in);
//    const profiler::reader::thread_blocks_tree_t &blocks_tree = fr.getBlocksTreeData();
////    auto iter = blocks_tree.begin();
//    //json.push_back("root");

////    auto jsonObjects = nlohmann::json::array();
//    json_node_t node_root(0);
//    //json.insert(json.begin(),node);
//    for(const auto &value : blocks_tree)
//    {
//  //      jsonObjects.insert(json.end(),value.first);
//      //  std::cout << value.first << std::endl;
//        json_node_t node_tmp(value.first);
//        //node_tmp.add(value.first);
//        readThreadBlocks(value.second,node_tmp);
//        node_root.add(node_tmp);
//        //json.insert(json.end(),node_tmp);
//    }

//     cout << nlohmann::json(node_root).dump(2) << endl;
////    std::string s = json.dump();
////    std::cout << s.c_str();
//    int val = 0;

}


//    json_node_t node_a = {1, {{2, {}}, {3, {{5, {}}, {6, {}}}}, {4, {}}}};

//    json_node_t node_b = {1, {2, {3, {5, 6}}, 4}};

//    json_node_t node_c(1);

//    cout << nlohmann::json(node_a).dump(2) << endl << endl;

//      cout << nlohmann::json(node_b).dump(2) << endl << endl;
//      cout << nlohmann::json(node_c).dump(2) << endl;

//       node_c.add(2);
//       node_c.add(3).add({5, 6});
//       node_c.add(4);

//       cout << nlohmann::json(node_c).dump() << endl;



void json_thread_t::add(const json_node_t &_node)
{
    node.add(node);
}

void json_thread_t::add(const std::initializer_list<json_node_t> &nodes)
{
//    node. add(node);
//    child.insert(child.end(), nodes);
//    return child.back();
}

//json_node_t& json_node_t::add(const json_node_t& node) {
//    child.push_back(node);
//    return child.back();
//}
