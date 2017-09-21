#ifndef CONVERTER____H
#define CONVERTER____H

///std
#include<string>

///this
#include "reader.h"

struct IConverter
{
    virtual void convert() = 0;
};

class JSONConverter EASY_FINAL : public IConverter
{
public:
    JSONConverter(std::string file_in, std::string file_out);
    void convert() override;    
private:
    std::string m_file_in;
    std::string m_file_out;
};

#endif
