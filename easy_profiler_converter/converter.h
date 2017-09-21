#ifndef CONVERTER____H
#define CONVERTER____H

///std
#include<string>

struct IConverter
{
    virtual void convert() = 0;
};

class JSONConverter : public IConverter
{
public:
    JSONConverter(std::string file_in, std::string file_out);
    void convert() override;    
private:
    std::string m_file_in;
    std::string m_file_out;
};

#endif
