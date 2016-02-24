#include "profiler/profiler.h"

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
	thread_id = std::hash<std::thread::id>()(std::this_thread::get_id());
}

void Block::tick(timestamp_t& stamp)
{
	std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds> time_point;
	time_point = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now());
	stamp = time_point.time_since_epoch().count();
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
	else{//for mark end equal begin
		end = begin;
	}
	
}
