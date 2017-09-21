///std
#include <iostream>
///this
#include "reader.h"

using namespace reader;

int main(int argc, char* argv[])
{
//    FileHeader fh;
//    std::cout << sizeof(fh);
//    ::std::ifstream in("test.prof", ::std::ios::binary);
//    in.read((char*)&fh, sizeof(fh));
//    std::cout << fh.begin_time;

    std::string filename;
    if (argc > 1 && argv[1])
    {
        filename = argv[1];
    }
    else
    {
        std::cout << "prof file path: ";
        std::getline(std::cin, filename);
    }

    IReader *reader = new SimpleReader(filename);
    BlocksDescriptors bdsc = reader->getBlockDescriptors();
    std::cout << bdsc.size << endl;
    return 0;
}
