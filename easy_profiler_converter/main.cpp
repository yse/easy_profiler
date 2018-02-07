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
    std::string output_json_filename = "";
    if (argc > 1 && argv[1])
    {
        filename = argv[1];
    }
    else
    {
        std::cout << "Usage: " << argv[0] << " INPUT_PROF_FILE [OUTPUT_JSON_FILE]\n"
                                             "where:\n"
                                             "INPUT_PROF_FILE required\n"
                                             "OUTPUT_JSON_FILE optional (if not specified output will be print in stdout)\n";
        return 1;
    }
    if (argc > 2 && argv[2])
    {
        output_json_filename = argv[2];
    }

    JSONConverter js(filename, output_json_filename);
    js.convert();

    return 0;
}
