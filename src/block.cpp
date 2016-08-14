#include "profiler/profiler.h"
#include "profile_manager.h"
#include <ctime>
#include <chrono>
#include <thread>

using namespace profiler;

#ifdef WIN32
struct ProfPerformanceFrequency {
    LARGE_INTEGER frequency;
    ProfPerformanceFrequency() { QueryPerformanceFrequency(&frequency); }
} const WINDOWS_CPU_INFO;
#endif

BaseBlockData::BaseBlockData(source_id_t _source_id, color_t _color, block_type_t _type, thread_id_t _thread_id)
    : begin(0)
    , end(0)
    , thread_id(_thread_id)
#ifndef EASY_USE_OLD_FILE_FORMAT
    , source_id(_source_id)
#endif
    , type(_type)
    , color(_color)
{

}

Block::Block(const char* _name, color_t _color, block_type_t _type, source_id_t _source_id)
    : Block(_name, getCurrentThreadId(), _color, _type, _source_id)
{
}

Block::Block(const char* _name, thread_id_t _thread_id, color_t _color, block_type_t _type, source_id_t _source_id)
    : BaseBlockData(_source_id, _color, _type, _thread_id)
    , m_name(_name)
{
    tick(begin);
    if (BLOCK_TYPE_BLOCK != this->type)
    {
        end = begin;
    }
}

inline timestamp_t getCurrentTime()
{
#ifdef WIN32
	//see https://msdn.microsoft.com/library/windows/desktop/dn553408(v=vs.85).aspx
	LARGE_INTEGER elapsedMicroseconds;
	if (!QueryPerformanceCounter(&elapsedMicroseconds))
		return 0;
	elapsedMicroseconds.QuadPart *= 1000000000LL;
    elapsedMicroseconds.QuadPart /= WINDOWS_CPU_INFO.frequency.QuadPart;
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
