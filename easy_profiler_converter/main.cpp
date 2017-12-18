///std
#include <iostream>
#include <memory>

///this
//#include "reader.h"
#include "converter.h"

using namespace profiler::reader;
using namespace std;

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
//    FileReader fr;
//    fr.readFile(filename);

//    const profiler::reader::thread_blocks_tree_t &blocks_tree = fr.getBlocksTreeData();
    JSONConverter js(filename,"");
    js.convert();

    return 0;
}
