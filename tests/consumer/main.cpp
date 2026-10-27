// Links against an installed or unpacked easy_profiler package. Given a path,
// it also writes a profile there so the package's own command-line tools can be
// run against something this executable produced.

#include <easy/profiler.h>

#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    profiler::setEnabled(true);
    for (int i = 0; i < 16; ++i)
    {
        EASY_BLOCK("installed_consumer");
    }

    const char* version = profiler::apiVersionName();
    if (version == nullptr)
    {
        std::fprintf(stderr, "apiVersionName() returned null\n");
        return 1;
    }

#ifdef EASY_PACKAGE_VERSION
    // The CMake package metadata and the library binary must agree.
    const std::string expected = std::string("v") + EASY_PACKAGE_VERSION;
    if (expected != version)
    {
        std::fprintf(stderr, "package says %s, library says %s\n", expected.c_str(), version);
        return 1;
    }
#endif

    if (profiler::apiVersionMajor() != 3)
    {
        std::fprintf(stderr, "unexpected major version %u\n",
                     static_cast<unsigned>(profiler::apiVersionMajor()));
        return 1;
    }

    if (profiler::formatVersionMajor() != 2 || profiler::formatVersionMinor() != 1 ||
        profiler::formatVersionPatch() != 0)
    {
        std::fprintf(stderr, "unexpected wire format version %u.%u.%u\n",
                     static_cast<unsigned>(profiler::formatVersionMajor()),
                     static_cast<unsigned>(profiler::formatVersionMinor()),
                     static_cast<unsigned>(profiler::formatVersionPatch()));
        return 1;
    }

    if (argc > 1 && argv[1] != nullptr)
    {
        const uint32_t blocks = profiler::dumpBlocksToFile(argv[1]);
        if (blocks == 0)
        {
            std::fprintf(stderr, "dumpBlocksToFile(%s) wrote no blocks\n", argv[1]);
            return 1;
        }
    }

    profiler::setEnabled(false);
    return 0;
}
