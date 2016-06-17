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

/**
\defgroup profiler Profiler
*/

/** Macro of beginning of block with custom name and default identification

\code
	#include "profiler/profiler.h"
	void foo()
	{
		// some code ...
		if(something){
			PROFILER_BEGIN_BLOCK("Calling someThirdPartyLongFunction()");
			someThirdPartyLongFunction();
			return;
		}
	}
\endcode

Block will be automatically completed by destructor

\ingroup profiler
*/
#define PROFILER_BEGIN_BLOCK(name)	profiler::Block TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__)(name,0,profiler::BLOCK_TYPE_BLOCK);\
									profiler::beginBlock(&TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__));

/** Macro of beginning of block with custom name and custom identification

\code
	#include "profiler/profiler.h"
	void foo()
	{
		// some code ...
		if(something){
			PROFILER_BEGIN_BLOCK("Calling someThirdPartyLongFunction()",profiler::colors::Red);
			someThirdPartyLongFunction();
			return;
		}
	}
\endcode

Block will be automatically completed by destructor

\ingroup profiler
*/
#define PROFILER_BEGIN_BLOCK_GROUPED(name,block_group)	profiler::Block TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__)(name,block_group,profiler::BLOCK_TYPE_BLOCK);\
														profiler::beginBlock(&TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__));

/** Macro of beginning of function block with default identification

\code
	#include "profiler/profiler.h"
	void foo()
	{
		PROFILER_BEGIN_FUNCTION_BLOCK;
		//some code...
	}
\endcode

Name of block automatically created with function name

\ingroup profiler
*/
#define PROFILER_BEGIN_FUNCTION_BLOCK PROFILER_BEGIN_BLOCK(__func__)

/** Macro of beginning of function block with custom identification

\code
	#include "profiler/profiler.h"
	void foo()
	{
		PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(profiler::colors::Red);
		//some code...
	}
\endcode

Name of block automatically created with function name

\ingroup profiler
*/
#define PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(block_color) PROFILER_BEGIN_BLOCK_GROUPED(__func__,block_color)

/** Macro of completion of last nearest open block

\code
#include "profiler/profiler.h"
void foo()
{
// some code ...
	int sum = 0;
	PROFILER_BEGIN_BLOCK("Calculating summ");
	for(int i = 0; i < 10; i++){
		sum += i;
	}
	PROFILER_END_BLOCK;
}
\endcode

\ingroup profiler
*/
#define PROFILER_END_BLOCK profiler::endBlock();

#define PROFILER_ADD_EVENT(name)	profiler::Block TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__)(name,0,profiler::BLOCK_TYPE_EVENT);\
									profiler::beginBlock(&TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__));

#define PROFILER_ADD_EVENT_GROUPED(name,block_group)	profiler::Block TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__)(name,block_group,profiler::BLOCK_TYPE_EVENT);\
														profiler::beginBlock(&TOKEN_CONCATENATE(unique_profiler_mark_name_,__LINE__));

/** Macro enabling profiler
\ingroup profiler
*/
#define PROFILER_ENABLE profiler::setEnabled(true);

/** Macro disabling profiler
\ingroup profiler
*/
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
	typedef uint8_t color_t; //RRR-GGG-BB

	namespace colors{

		const color_t Black        = 0x00;
		const color_t Lightgray    = 0x6E;
		const color_t Darkgray     = 0x25;
		const color_t White        = 0xFF;
		const color_t Red          = 0xE0;
		const color_t Green        = 0x1C;
		const color_t Blue         = 0x03;
		const color_t Magenta      = (Red   | Blue);
		const color_t Cyan         = (Green | Blue);
		const color_t Yellow       = (Red   | Green);
		const color_t Lightred     = 0x60;
		const color_t Lightgreen   = 0x0C;
		const color_t Lightblue    = 0x01;
		const color_t Lightmagenta = (Lightred   | Lightblue);
		const color_t Lightcyan    = (Lightgreen | Lightblue);
		const color_t Lightyellow  = (Lightred   | Lightgreen);
		const color_t Navy         = 0x02;
		const color_t Teal         = 0x12;
		const color_t Maroon       = 0x80;
		const color_t Purple       = 0x82;
		const color_t Olive        = 0x90;
		const color_t Grey         = 0x92;
		const color_t Silver       = 0xDB;

		inline int get_red(color_t color){
			return (color >> 5) * 0x20;
		}
		inline int get_green(color_t color){
			return ((color & 0x1C) >> 2) * 0x20;
		}
		inline int get_blue(color_t color){
			return (color & 3) * 0x40;
		}

		inline int convert_to_rgb(color_t color){
			return ((get_red(color) & 0xFF) << 16) | ((get_green(color) & 0xFF) << 8) | (get_blue(color) & 0xFF);
		}

	}


	class Block;
	
	extern "C"{
		void PROFILER_API beginBlock(Block* _block);
		void PROFILER_API endBlock();
		void PROFILER_API setEnabled(bool isEnable);
	}

	typedef uint8_t block_type_t;
	typedef uint64_t timestamp_t;
	typedef uint32_t thread_id_t;

	const block_type_t BLOCK_TYPE_EVENT = 1;
	const block_type_t BLOCK_TYPE_BLOCK = 2;
		
#pragma pack(push,1)
	class PROFILER_API BaseBlockData
	{
	protected:

		block_type_t type;
		color_t color;
		timestamp_t begin;
		timestamp_t end;
		thread_id_t thread_id;

		void tick(timestamp_t& stamp);
	public:

		BaseBlockData(color_t _color, block_type_t _type);

		inline unsigned char getType() const { return type; }
		inline color_t getColor() const { return color; }
		inline timestamp_t getBegin() const { return begin; }
		inline size_t getThreadId() const { return thread_id; }

		inline timestamp_t getEnd() const { return end; }
		inline bool isFinished() const { return end != 0; }
		inline bool isCleared() const { return end >= begin; }
		inline void finish(){ tick(end); }
		timestamp_t duration() const { return (end - begin); }

        inline bool startsEarlierThan(const BaseBlockData& another) const {return this->begin < another.begin;}
        inline bool endsEarlierThan(const BaseBlockData& another) const {return this->end < another.end;}
        inline bool startsLaterThan(const BaseBlockData& another) const {return !this->startsEarlierThan(another);}
        inline bool endsLaterThan(const BaseBlockData& another) const {return !this->endsEarlierThan(another);}
	};
#pragma pack(pop)

	class PROFILER_API Block : public BaseBlockData
	{
		const char *name;		
	public:

		Block(const char* _name, color_t _color = 0, block_type_t _type = BLOCK_TYPE_EVENT);
		~Block();

		inline const char* getName() const { return name; }		
	};

	class PROFILER_API SerilizedBlock
	{
		uint16_t m_size;
		char* m_data;
	public:
		SerilizedBlock(profiler::Block* block);
		SerilizedBlock(uint16_t _size, const char* _data);
		SerilizedBlock(SerilizedBlock&& that);
		SerilizedBlock(const SerilizedBlock& other);
		~SerilizedBlock();

		const char* const data() const { return m_data; }
		uint16_t size() const { return m_size; }

		const BaseBlockData * block();
		const char* getBlockName();
	};

	
}

#endif
