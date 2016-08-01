#include "profiler/profiler.h"
#include "profile_manager.h"
#include <ctime>
#include <chrono>
#include <thread>

using namespace profiler;

BaseBlockData::BaseBlockData(color_t _color, block_type_t _type) :
		type(_type),
		color(_color),
		begin(0),
		end(0),
		thread_id(0)
{

}

Block::Block(const char* _name, color_t _color, block_type_t _type) :
		BaseBlockData(_color,_type),
		name(_name)
{
	tick(begin);
	if (BLOCK_TYPE_BLOCK != this->type)
	{
		end = begin;
	}
	thread_id = getCurrentThreadId();
}

inline timestamp_t getCurrentTime()
{
#ifdef WIN32
	//see https://msdn.microsoft.com/library/windows/desktop/dn553408(v=vs.85).aspx
	static LARGE_INTEGER frequency;
	static bool first=true;
	if (first)
		QueryPerformanceFrequency(&frequency);
	first = false;
	LARGE_INTEGER elapsedMicroseconds;
	if (!QueryPerformanceCounter(&elapsedMicroseconds))
		return 0;
	elapsedMicroseconds.QuadPart *= 1000000000LL;
	elapsedMicroseconds.QuadPart /= frequency.QuadPart;
	return (timestamp_t)elapsedMicroseconds.QuadPart;
#else
	std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> time_point;
	time_point = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
	return time_point.time_since_epoch().count();
#endif
}

void BaseBlockData::tick(timestamp_t& stamp)
{
	stamp = getCurrentTime();
}

Block::~Block()
{
	if (this->type == BLOCK_TYPE_BLOCK)
	{
		if (this->isCleared())//this block was cleared by END_BLOCK macros
			return;
		if (!this->isFinished())
			this->finish();

		endBlock();
	}
}
