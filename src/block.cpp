#include "profiler/profiler.h"

#include <ctime>
#include <chrono>
#include <thread>

using namespace profiler;

const int NAME_LENGTH = 64;

const unsigned char MARK_TYPE_LOCAL_EVENT = 1;
const unsigned char MARK_TYPE_GLOBAL_EVENT = 2;
const unsigned char MARK_TYPE_BEGIN = 3;
const unsigned char MARK_TYPE_END = 4;

Mark::Mark(const char* _name, color_t _color) :
begin(0),
color(_color),
thread_id(0),
name(_name),
type(0)
{
	tick(begin);
	thread_id = std::this_thread::get_id().hash();
}

void Mark::tick(timestamp_t& stamp)
{
	std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> time_point;
	time_point = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
	stamp = time_point.time_since_epoch().count();
}

Block::Block(const char* _name, color_t _color) :
Mark(_name, _color), 
end(begin)
{

}

Block::~Block()
{
	tick(end);
}