///std
#include <iostream>
#include <memory>
///this
#include "reader.h"

using namespace profiler::reader;

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

//    IReader *reader = new SimpleReader(filename);
//    BlocksDescriptors bdsc = reader->getBlockDescriptors();
//    std::cout << bdsc.size << endl;
    FileReader fr;
    fr.readFile(filename);
    std::vector<InfoBlock> v_blocks;
    //fr.getInfoBlocks(v_blocks);

    cout << endl;
    cout << "=====================";
    //cout << EASY_SHIFT_BLOCK_DESCRIPTORS_INFO;
    cout << "=====================";
    cout << endl;

    //vector<std::unique_ptr<profiler::SerializedBlockDescriptor>> vv = fr.getSerializedBlockDescriptors();
    //BlocksDescriptorsInfo bdi = fr.getBlockDescriptorsInfo();

    cout << endl;
    cout << "=====================";
    //cout << "SerializedBlockDescriptor count: " << vv.size();
    cout << "=====================";
    cout << endl;


    return 0;
}
