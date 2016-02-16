#include "profiler/profiler.h"

int main()
{

	//profiler::Block bl("sds");

	//profiler::registerMark(&bl);

	PROFILER_ENABLE;
	PROFILER_BEGIN_BLOCK("block1");
	PROFILER_ADD_MARK("mark1");
	PROFILER_ADD_MARK_GROUPED("mark1",1);
	PROFILER_DISABLE;
	return 0;
}
