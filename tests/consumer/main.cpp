#include <easy/profiler.h>

#include <cstring>

int main()
{
    profiler::setEnabled(true);
    {
        EASY_BLOCK("installed_consumer");
    }
    profiler::setEnabled(false);

    const char* version = profiler::apiVersionName();
    return version != nullptr && profiler::apiVersionMajor() == 3 ? 0 : 1;
}
