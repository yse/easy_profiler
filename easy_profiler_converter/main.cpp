///std
#include <iostream>
#include <memory>
///this
#include "reader.h"

using namespace profiler::reader;

void test(std::string filename, vector<::std::shared_ptr<BlocksTreeNode> >& blocks)
{
    FileReader fr;
    fr.readFile(filename);
    std::vector<InfoBlock> v_blocks;
    blocks = fr.getInfoBlocks();
}

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
    vector<::std::shared_ptr<BlocksTreeNode> > blocks;
    test(filename,blocks);
    return 0;
}
