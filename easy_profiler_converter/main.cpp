///std
#include <iostream>
#include <memory>

///this
//#include "reader.h"
#include "converter.h"

using namespace profiler::reader;

int main(int argc, char* argv[])
{
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
    ///get RAW data
    //    FileReader fr;
    //    fr.readFile(filename);

    //    const profiler::reader::thread_blocks_tree_t &blocks_tree = fr.getBlocksTreeData();
    ///end get RAW data
    JSONConverter js(filename,"some_test.json");
    js.convert();

    return 0;
}
