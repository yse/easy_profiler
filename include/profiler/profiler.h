/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev

This program is free software : you can redistribute it and / or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef ____PROFILER____H_______
#define ____PROFILER____H_______

#define TOKEN_JOIN(x, y) x ## y
#define TOKEN_CONCATENATE(x, y) TOKEN_JOIN(x, y)

#if defined ( WIN32 )
#define __func__ __FUNCTION__
#endif

#ifndef FULL_DISABLE_PROFILER
#define PROFILER_ADD_MARK(name)	profiler::Block TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__)(name,0,profiler::BLOCK_TYPE_MARK);\
									profiler::beginBlock(&TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__));

#define PROFILER_ADD_MARK_GROUPED(name,block_group)	profiler::Block TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__)(name,block_group,profiler::BLOCK_TYPE_MARK);\
														profiler::beginBlock(&TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__));

#define PROFILER_BEGIN_BLOCK(name)	profiler::Block TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__)(name,0,profiler::BLOCK_TYPE_BLOCK);\
									profiler::beginBlock(&TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__));

#define PROFILER_BEGIN_BLOCK_GROUPED(name,block_group)	profiler::Block TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__)(name,block_group,profiler::BLOCK_TYPE_BLOCK);\
														profiler::beginBlock(&TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__));

#define PROFILER_BEGIN_FUNCTION_BLOCK PROFILER_BEGIN_BLOCK(__func__)

#define PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(block_group) PROFILER_BEGIN_BLOCK_GROUPED(__func__,block_group)

#define PROFILER_END_BLOCK profiler::endBlock();

#define PROFILER_ENABLE profiler::setEnabled(true);

#define PROFILER_DISABLE profiler::setEnabled(false);

#else
#define PROFILER_ADD_MARK(name)
#define PROFILER_ADD_MARK_GROUPED(name,block_group)
#define PROFILER_BEGIN_BLOCK(name)
#define PROFILER_BEGIN_BLOCK_GROUPED(name,block_group)
#define PROFILER_BEGIN_FUNCTION_BLOCK PROFILER_BEGIN_BLOCK(__func__)
#define PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(block_group) PROFILER_BEGIN_BLOCK_GROUPED(__func__,block_group)
#define PROFILER_END_BLOCK profiler::endBlock();
#define PROFILER_ENABLE profiler::setEnabled(true);
#define PROFILER_DISABLE profiler::setEnabled(false);
#endif

#include <stdint.h>
#include <cstddef>

#ifdef _WIN32
#ifdef	_BUILD_PROFILER
#define  PROFILER_API		__declspec(dllexport)
#else
#define  PROFILER_API		__declspec(dllimport)
#endif
#else
#define  PROFILER_API
#endif

namespace profiler
{
	class Block;
	
	extern "C"{
		void PROFILER_API beginBlock(Block* _block);
		void PROFILER_API endBlock();
		void PROFILER_API setEnabled(bool isEnable);
	}

	typedef uint8_t block_type_t;
	typedef uint64_t timestamp_t;
	typedef uint16_t color_t;
	typedef uint32_t thread_id_t;
	
	const block_type_t BLOCK_TYPE_MARK = 1;
	const block_type_t BLOCK_TYPE_BLOCK = 2;

#pragma pack(push,1)
	struct PROFILER_API BaseBlockData
	{
		block_type_t type;
		color_t color;
		timestamp_t begin;
		timestamp_t end;
		thread_id_t thread_id;

		BaseBlockData(color_t _color, block_type_t _type);

		timestamp_t duration() const { return (end - begin); }
	};
#pragma pack(pop)

	class PROFILER_API Block : public BaseBlockData
	{
			const char *name;
			void tick(timestamp_t& stamp);
			
		public:

			Block(const char* _name, color_t _color = 0, block_type_t _type = BLOCK_TYPE_MARK);

			inline unsigned char getType() const { return type; }
			inline color_t getColor() const { return color; }
			inline timestamp_t getBegin() const { return begin; }
			inline size_t getThreadId() const { return thread_id; }
			inline const char* getName() const { return name; }

			inline timestamp_t getEnd() const { return end; }
			inline bool isFinished() const { return end != 0; }
			inline bool isCleared() const { return end >= begin; }
			inline void finish(){ tick(end); }

			~Block();
	};

	class PROFILER_API SerilizedBlock
	{
		uint16_t m_size;
		char* m_data;
	public:
		SerilizedBlock(profiler::Block* block);
		SerilizedBlock(uint16_t _size, const char* _data);
		SerilizedBlock(SerilizedBlock&& that);
		~SerilizedBlock();

		const char* const data() const { return m_data; }
		uint16_t size() const { return m_size; }

		const BaseBlockData * block();
		const char* getBlockName();
	};

	
}

#endif
