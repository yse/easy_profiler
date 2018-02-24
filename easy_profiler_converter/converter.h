#ifndef EASY_PROFILER_CONVERTER_H
#define EASY_PROFILER_CONVERTER_H

///std
#include<string>

///this
#include "reader.h"

///nlohmann json
#include "include/json.hpp"

class JSONConverter EASY_FINAL
{
public:
    JSONConverter(const ::std::string &file_in,
                  const ::std::string &file_out):
        m_file_in(file_in),
        m_file_out(file_out)
    {}

    ~JSONConverter()
    {
    }
    void convert();
private:
    void readThreadBlocks(const profiler::reader::BlocksTreeNode &node, nlohmann::json &json);

    ::std::string m_file_in;
    ::std::string m_file_out;
    nlohmann::json json;
};

#endif //EASY_PROFILER_CONVERTER_H
