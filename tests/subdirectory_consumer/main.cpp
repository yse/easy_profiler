#include <easy/profiler.h>

int main()
{
    return profiler::apiVersionMajor() == 3 ? 0 : 1;
}
