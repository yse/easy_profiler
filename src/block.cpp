#include "profiler/profiler.h"
#include "profile_manager.h"
#include <ctime>
#include <chrono>
#include <thread>

using namespace profiler;

#ifdef _WIN32
decltype(LARGE_INTEGER::QuadPart) CPU_FREQUENCY = ([](){ LARGE_INTEGER freq; QueryPerformanceFrequency(&freq); return freq.QuadPart; })();
#endif

inline timestamp_t getCurrentTime()
{
#ifdef _WIN32
	//see https://msdn.microsoft.com/library/windows/desktop/dn553408(v=vs.85).aspx
	LARGE_INTEGER elapsedMicroseconds;
	if (!QueryPerformanceCounter(&elapsedMicroseconds))
		return 0;
    //elapsedMicroseconds.QuadPart *= 1000000000LL;
    //elapsedMicroseconds.QuadPart /= CPU_FREQUENCY;
	return (timestamp_t)elapsedMicroseconds.QuadPart;
#else
	std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> time_point;
	time_point = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
	return time_point.time_since_epoch().count();
#endif
}

BaseBlockData::BaseBlockData(timestamp_t _begin_time, block_id_t _descriptor_id)
    : m_begin(_begin_time)
    , m_end(0)
    , m_id(_descriptor_id)
{

}

Block::Block(Block&& that)
    : BaseBlockData(that.m_begin, that.m_id)
    , m_name(that.m_name)
{
    m_end = that.m_end;
}

Block::Block(block_type_t _block_type, block_id_t _descriptor_id, const char* _name)
    : Block(getCurrentTime(), _block_type, _descriptor_id, _name)
{
}

Block::Block(timestamp_t _begin_time, block_type_t _block_type, block_id_t _descriptor_id, const char* _name)
    : BaseBlockData(_begin_time, _descriptor_id)
    , m_name(_name)
{
    if (static_cast<uint8_t>(_block_type) < BLOCK_TYPE_BLOCK)
    {
        m_end = m_begin;
    }
}

void Block::finish()
{
    m_end = getCurrentTime();
}

void Block::finish(timestamp_t _end_time)
{
    m_end = _end_time;
}

Block::~Block()
{
    if (!isFinished())
        ::profiler::endBlock();
}
