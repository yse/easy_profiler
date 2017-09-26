#ifndef CONVERTER____H
#define CONVERTER____H

///std
#include<string>

///this
#include "reader.h"

class IConverter
{
public:
    virtual void convert() = 0;
    virtual ~IConverter();
};

class JSONConverter EASY_FINAL : public IConverter
{
public:
    JSONConverter(std::string file_in,
                  std::string file_out):
                  m_file_in(file_in),
                  m_file_out(file_out)
    {}

    ~JSONConverter();
    void convert() override;    
private:
    std::string m_file_in;
    std::string m_file_out;
};

#endif
