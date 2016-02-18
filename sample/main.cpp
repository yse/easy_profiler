#include "profiler/profiler.h"
#include <chrono>
#include <thread>

void shortTimeFunction(){
	PROFILER_BEGIN_FUNCTION_BLOCK;
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void longTimeFunction(){
	PROFILER_BEGIN_FUNCTION_BLOCK;
	std::this_thread::sleep_for(std::chrono::milliseconds(5));
}



int main()
{

	//profiler::Block bl("sds");

	//profiler::registerMark(&bl);

	
	PROFILER_ENABLE;
	longTimeFunction();
	shortTimeFunction();

	PROFILER_BEGIN_BLOCK("block1");
	PROFILER_BEGIN_BLOCK("block2");
	PROFILER_BEGIN_BLOCK("block3");
	PROFILER_BEGIN_BLOCK("block4");
	PROFILER_END_BLOCK;
	//PROFILER_END_BLOCK;
	PROFILER_ADD_MARK("mark1");
	PROFILER_ADD_MARK_GROUPED("mark1",1);
	//PROFILER_DISABLE;
	return 0;
}
