#include <easy/profiler.h>

int main()
{
    profiler::setEnabled(true);
    {
        EASY_BLOCK("bazel_smoke");
    }
    profiler::setEnabled(false);

    return profiler::apiVersionMajor() == 3 && profiler::apiVersionMinor() == 0
        ? 0
        : 1;
}
