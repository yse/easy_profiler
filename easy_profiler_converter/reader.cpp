///C++
#include <fstream>

///this
#include "reader.h"

using namespace reader;

FileHeader SimpleReader::getFileHeader()
{
    FileHeader fh;
    ::std::ifstream in(inputFilePath, ::std::ifstream::binary);
    in.read((char*)&fh, sizeof(fh));
    return fh;
}

BlocksDescriptors SimpleReader::getBlockDescriptors()
{
    BlocksDescriptors bd;
    ::std::ifstream in(inputFilePath, ::std::ifstream::binary);
    in.seekg(sizeof(FileHeader),in.end);
    in.read((char*)&bd, sizeof(bd));
    return bd;
}
