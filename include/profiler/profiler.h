/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin

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

#if defined ( WIN32 )
#define __func__ __FUNCTION__
#endif

#ifndef FULL_DISABLE_PROFILER

#define TOKEN_JOIN(x, y) x ## y
#define TOKEN_CONCATENATE(x, y) TOKEN_JOIN(x, y)
#define PROFILER_UNIQUE_BLOCK(x) TOKEN_CONCATENATE(unique_profiler_mark_name_, x)
#define PROFILER_UNIQUE_DESC(x) TOKEN_CONCATENATE(unique_profiler_descriptor_, x)

/**
\defgroup profiler Profiler
*/

/** Macro used to check compile-time strings.

Compiler automatically concatenates "A" "B" into "AB" if both A and B strings
can be identified at compile-time.

\ingroup profiler
*/
#define COMPILETIME_TEST "_compiletime_test"


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
#define PROFILER_BEGIN_BLOCK(compiletime_name)\
    static const ::profiler::StaticBlockDescriptor PROFILER_UNIQUE_DESC(__LINE__)(compiletime_name, __FILE__, __LINE__,\
        ::profiler::BLOCK_TYPE_BLOCK, ::profiler::DefaultBlockColor);\
    ::profiler::Block PROFILER_UNIQUE_BLOCK(__LINE__)(compiletime_name COMPILETIME_TEST, ::profiler::BLOCK_TYPE_BLOCK,\
        PROFILER_UNIQUE_DESC(__LINE__).id(), compiletime_name);\
    ::profiler::beginBlock(PROFILER_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

#define EASY_BLOCK(compiletime_name)\
    static const ::profiler::StaticBlockDescriptor PROFILER_UNIQUE_DESC(__LINE__)(compiletime_name, __FILE__, __LINE__,\
        ::profiler::BLOCK_TYPE_BLOCK, ::profiler::DefaultBlockColor);\
    ::profiler::Block PROFILER_UNIQUE_BLOCK(__LINE__)(compiletime_name COMPILETIME_TEST, ::profiler::BLOCK_TYPE_BLOCK,\
        PROFILER_UNIQUE_DESC(__LINE__).id());\
    ::profiler::beginBlock(PROFILER_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

#define EASY_BLOCK_DYNAMIC(compiletime_name, runtime_name)\
    static const ::profiler::StaticBlockDescriptor PROFILER_UNIQUE_DESC(__LINE__)(compiletime_name, __FILE__, __LINE__,\
        ::profiler::BLOCK_TYPE_BLOCK, ::profiler::DefaultBlockColor);\
    ::profiler::Block PROFILER_UNIQUE_BLOCK(__LINE__)(compiletime_name COMPILETIME_TEST, ::profiler::BLOCK_TYPE_BLOCK,\
        PROFILER_UNIQUE_DESC(__LINE__).id(), runtime_name);\
    ::profiler::beginBlock(PROFILER_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

/** Macro of beginning of block with custom name and custom identification

\code
	#include "profiler/profiler.h"
	void foo()
	{
		// some code ...
		if(something){
			PROFILER_BEGIN_BLOCK("Calling someThirdPartyLongFunction()", ::profiler::colors::Red);
			someThirdPartyLongFunction();
			return;
		}
	}
\endcode

Block will be automatically completed by destructor

\ingroup profiler
*/
#define PROFILER_BEGIN_BLOCK_GROUPED(compiletime_name, block_group)\
    static const ::profiler::StaticBlockDescriptor PROFILER_UNIQUE_DESC(__LINE__)(compiletime_name, __FILE__, __LINE__,\
        ::profiler::BLOCK_TYPE_BLOCK, block_group);\
    ::profiler::Block PROFILER_UNIQUE_BLOCK(__LINE__)(compiletime_name COMPILETIME_TEST, ::profiler::BLOCK_TYPE_BLOCK,\
        PROFILER_UNIQUE_DESC(__LINE__).id());\
    ::profiler::beginBlock(PROFILER_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

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
#define PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(block_color) PROFILER_BEGIN_BLOCK_GROUPED(__func__, block_color)

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
#define PROFILER_END_BLOCK ::profiler::endBlock();

#define PROFILER_ADD_EVENT(compiletime_name)	\
    static const ::profiler::StaticBlockDescriptor PROFILER_UNIQUE_DESC(__LINE__)(compiletime_name, __FILE__, __LINE__, ::profiler::BLOCK_TYPE_EVENT);\
    ::profiler::Block PROFILER_UNIQUE_BLOCK(__LINE__)(compiletime_name COMPILETIME_TEST, ::profiler::BLOCK_TYPE_EVENT,\
        PROFILER_UNIQUE_DESC(__LINE__).id());\
    ::profiler::beginBlock(PROFILER_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

#define PROFILER_ADD_EVENT_GROUPED(compiletime_name, block_group)\
    static const ::profiler::StaticBlockDescriptor PROFILER_UNIQUE_DESC(__LINE__)(compiletime_name, __FILE__, __LINE__,\
        ::profiler::BLOCK_TYPE_EVENT, block_group);\
    ::profiler::Block PROFILER_UNIQUE_BLOCK(__LINE__)(compiletime_name COMPILETIME_TEST, ::profiler::BLOCK_TYPE_EVENT,\
        PROFILER_UNIQUE_DESC(__LINE__).id());\
    ::profiler::beginBlock(PROFILER_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

/** Macro enabling profiler
\ingroup profiler
*/
#define PROFILER_ENABLE ::profiler::setEnabled(true);

/** Macro disabling profiler
\ingroup profiler
*/
#define PROFILER_DISABLE ::profiler::setEnabled(false);

#ifdef WIN32
#define PROFILER_SET_THREAD_NAME(name) ::profiler::setThreadName(name);
#else
#define PROFILER_SET_THREAD_NAME(name) thread_local static const ::profiler::ThreadNameSetter TOKEN_CONCATENATE(unique_profiler_thread_name_setter_, __LINE__)(name);
#endif

#define PROFILER_SET_MAIN_THREAD PROFILER_SET_THREAD_NAME("Main")

#else
#define PROFILER_BEGIN_BLOCK(name)
#define PROFILER_BEGIN_BLOCK_GROUPED(name, block_group)
#define PROFILER_BEGIN_FUNCTION_BLOCK 
#define PROFILER_BEGIN_FUNCTION_BLOCK_GROUPED(block_group) 
#define PROFILER_END_BLOCK 
#define PROFILER_ENABLE 
#define PROFILER_DISABLE 
#define PROFILER_ADD_EVENT(name)
#define PROFILER_ADD_EVENT_GROUPED(name, block_group)
#define PROFILER_SET_THREAD_NAME(name)
#define PROFILER_SET_MAIN_THREAD 
#endif

#include <stdint.h>
#include <cstddef>
#include "profiler/profiler_colors.h"

#ifdef _WIN32
#ifdef	_BUILD_PROFILER
#define  PROFILER_API		__declspec(dllexport)
#else
#define  PROFILER_API		__declspec(dllimport)
#endif
#else
#define  PROFILER_API
#endif

class ProfileManager;
class ThreadStorage;

namespace profiler {

	class Block;
    class StaticBlockDescriptor;
	
	extern "C"{
		void PROFILER_API beginBlock(Block& _block);
		void PROFILER_API endBlock();
		void PROFILER_API setEnabled(bool isEnable);
		unsigned int PROFILER_API dumpBlocksToFile(const char* filename);
		void PROFILER_API setThreadName(const char* name);
	}

	typedef uint64_t timestamp_t;
	typedef uint32_t thread_id_t;
    typedef uint32_t  block_id_t;

    enum BlockType : uint8_t
    {
        BLOCK_TYPE_EVENT = 0,
        BLOCK_TYPE_BLOCK,
        BLOCK_TYPE_THREAD_SIGN,
        BLOCK_TYPE_CONTEXT_SWITCH,

        BLOCK_TYPES_NUMBER
    };
    typedef BlockType block_type_t;
		
#pragma pack(push,1)
    class PROFILER_API BaseBlockDescriptor
    {
    protected:

        int           m_line; ///< Line number in the source file
        block_type_t  m_type; ///< Type of the block (See BlockType)
        color_t      m_color; ///< Color of the block packed into 1-byte structure

        BaseBlockDescriptor(int _line, block_type_t _block_type, color_t _color);

    public:

        inline int line() const { return m_line; }
        inline block_type_t type() const { return m_type; }
        inline color_t color() const { return m_color; }
        inline rgb32_t rgb() const { return ::profiler::colors::convert_to_rgb(m_color); }
    };

    class PROFILER_API BlockDescriptor final : public BaseBlockDescriptor
    {
        const char*     m_name; ///< Static name of all blocks of the same type (blocks can have dynamic name) which is, in pair with descriptor id, a unique block identifier
        const char* m_filename; ///< Source file name where this block is declared

    public:

        BlockDescriptor(uint64_t& _used_mem, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color);

        const char* name() const { return m_name; }
        const char* file() const { return m_filename; }
    };

    class PROFILER_API BaseBlockData
    {
    protected:

        timestamp_t m_begin;
        timestamp_t   m_end;
        block_id_t     m_id;

    public:

        BaseBlockData(timestamp_t _begin_time, block_id_t _id);

        inline timestamp_t begin() const { return m_begin; }
        inline timestamp_t end() const { return m_end; }
        inline block_id_t id() const { return m_id; }
        timestamp_t duration() const { return m_end - m_begin; }
    };
#pragma pack(pop)

    class PROFILER_API Block final : public BaseBlockData
    {
        friend ::ProfileManager;

        const char* m_name;

    private:

        void finish();
        inline bool isFinished() const { return m_end >= m_begin; }

    public:

        Block(const char*, block_type_t _block_type, block_id_t _id, const char* _name = "");
        ~Block();

        inline const char* name() const { return m_name; }
    };

    //////////////////////////////////////////////////////////////////////

    class PROFILER_API StaticBlockDescriptor final
    {
        block_id_t m_id;

    public:

        StaticBlockDescriptor(const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color = colors::Random);
        block_id_t id() const { return m_id; }
    };

#ifndef WIN32
    struct PROFILER_API ThreadNameSetter final
    {
        ThreadNameSetter(const char* _name)
        {
            setThreadName(_name);
        }
    };
#endif

    //////////////////////////////////////////////////////////////////////
	
} // END of namespace profiler.

#endif
