#ifndef CONVERTER_H
#define CONVERTER_H

///std
#include<string>

///this
#include "reader.h"

///nlohmann json
#include "include/json.hpp"

using namespace std;

struct json_node_t {
    int id;
    std::vector<json_node_t> child;
    json_node_t& add(const json_node_t& node);
    json_node_t& add(const std::initializer_list<json_node_t>& nodes);
    json_node_t(int node_id,std::initializer_list<json_node_t> node_children= std::initializer_list<json_node_t>()) : id(node_id), child(node_children) {
    }
};

class JSONConverter EASY_FINAL
{
public:
    JSONConverter(std::string file_in,
                  std::string file_out):
                  m_file_in(file_in),
                  m_file_out(file_out)
    {}

    ~JSONConverter()
    {
    }
    void convert();
private:
    void readThreadBlocks(const profiler::reader::BlocksTreeNode &node, nlohmann::json &json);
private:
    std::string m_file_in;
    std::string m_file_out;
    nlohmann::json json;
};

#endif
