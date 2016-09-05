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

#ifndef EASY_PROFILER____H_______
#define EASY_PROFILER____H_______

#ifdef _WIN32
#define __func__ __FUNCTION__
#endif

#if defined ( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#ifndef FULL_DISABLE_PROFILER

#include <type_traits>

#define EASY_TOKEN_JOIN(x, y) x ## y
#define EASY_TOKEN_CONCATENATE(x, y) EASY_TOKEN_JOIN(x, y)
#define EASY_UNIQUE_BLOCK(x) EASY_TOKEN_CONCATENATE(unique_profiler_mark_name_, x)
#define EASY_UNIQUE_DESC(x) EASY_TOKEN_CONCATENATE(unique_profiler_descriptor_, x)

/**
\defgroup profiler Profiler
*/

namespace profiler {
    template <const bool IS_REF> struct NameSwitch final {
        static const char* runtime_name(const char* name) { return name; }
        static const char* compiletime_name(const char*) { return ""; }
    };

    template <> struct NameSwitch<true> final {
        static const char* runtime_name(const char*) { return ""; }
        static const char* compiletime_name(const char* name) { return name; }
    };
} // END of namespace profiler.

#define EASY_COMPILETIME_NAME(name) ::profiler::NameSwitch<::std::is_reference<decltype(name)>::value>::compiletime_name(name)
#define EASY_RUNTIME_NAME(name) ::profiler::NameSwitch<::std::is_reference<decltype(name)>::value>::runtime_name(name)


/** Macro of beginning of block with custom name and color.

\code
	#include "profiler/profiler.h"
	void foo()
	{
		// some code ...
		if(something){
			EASY_BLOCK("Calling bar()"); // Block with default color
			bar();
		}
        else{
            EASY_BLOCK("Calling baz()", profiler::colors::Red); // Red block
            baz();
        }
	}
\endcode

Block will be automatically completed by destructor.

\ingroup profiler
*/
#define EASY_BLOCK(name, ...)\
    static const ::profiler::StaticBlockDescriptor EASY_UNIQUE_DESC(__LINE__)(EASY_COMPILETIME_NAME(name), __FILE__, __LINE__,\
        ::profiler::BLOCK_TYPE_BLOCK , ## __VA_ARGS__);\
    ::profiler::Block EASY_UNIQUE_BLOCK(__LINE__)(::profiler::BLOCK_TYPE_BLOCK, EASY_UNIQUE_DESC(__LINE__).id(), EASY_RUNTIME_NAME(name));\
    ::profiler::beginBlock(EASY_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

/** Macro of beginning of block with function name and custom color.

\code
	#include "profiler/profiler.h"
	void foo(){
		EASY_FUNCTION(); // Block with name="foo" and default color
		//some code...
	}

    void bar(){
        EASY_FUNCTION(profiler::colors::Green); // Green block with name="bar"
        //some code...
    }
\endcode

Name of the block automatically created with function name.

\ingroup profiler
*/
#define EASY_FUNCTION(...)\
    static const ::profiler::StaticBlockDescriptor EASY_UNIQUE_DESC(__LINE__)(__func__, __FILE__, __LINE__,\
        ::profiler::BLOCK_TYPE_BLOCK , ## __VA_ARGS__);\
    ::profiler::Block EASY_UNIQUE_BLOCK(__LINE__)(::profiler::BLOCK_TYPE_BLOCK, EASY_UNIQUE_DESC(__LINE__).id(), "");\
    ::profiler::beginBlock(EASY_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

/** Macro of completion of last nearest open block.

\code
#include "profiler/profiler.h"
int foo()
{
    // some code ...

	int sum = 0;
	EASY_BLOCK("Calculating sum");
	for (int i = 0; i < 10; ++i){
		sum += i;
	}
	EASY_END_BLOCK;

    // some antoher code here ...

    return sum;
}
\endcode

\ingroup profiler
*/
#define EASY_END_BLOCK ::profiler::endBlock();

/** Macro of creating event with custom name and color.

Event is a block with zero duration and special type.

\warning Event ends immidiately and calling EASY_END_BLOCK after EASY_EVENT
will end previously opened EASY_BLOCK or EASY_FUNCTION.

\ingroup profiler
*/
#define EASY_EVENT(name, ...)\
    static const ::profiler::StaticBlockDescriptor EASY_UNIQUE_DESC(__LINE__)(EASY_COMPILETIME_NAME(name), __FILE__, __LINE__,\
        ::profiler::BLOCK_TYPE_EVENT , ## __VA_ARGS__);\
    ::profiler::Block EASY_UNIQUE_BLOCK(__LINE__)(::profiler::BLOCK_TYPE_EVENT, EASY_UNIQUE_DESC(__LINE__).id(), EASY_RUNTIME_NAME(name));\
    ::profiler::beginBlock(EASY_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

/** Macro enabling profiler
\ingroup profiler
*/
#define EASY_PROFILER_ENABLE ::profiler::setEnabled(true);

/** Macro disabling profiler
\ingroup profiler
*/
#define EASY_PROFILER_DISABLE ::profiler::setEnabled(false);

/** Macro of naming current thread.

If this thread has been already named then nothing changes.

\ingroup profiler
*/
#ifdef _WIN32
#define EASY_THREAD(name) ::profiler::setThreadName(name, __FILE__, __func__, __LINE__);
#else
#define EASY_THREAD(name) thread_local static const ::profiler::ThreadNameSetter EASY_TOKEN_CONCATENATE(unique_profiler_thread_name_setter_, __LINE__)(name, __FILE__, __func__, __LINE__);
#endif

/** Macro of naming main thread.

This is only for user comfort. There is no difference for EasyProfiler GUI between different threads.

\ingroup profiler
*/
#define EASY_MAIN_THREAD EASY_THREAD("Main")

#else
#define EASY_BLOCK(...)
#define EASY_FUNCTION(...)
#define EASY_END_BLOCK 
#define EASY_PROFILER_ENABLE 
#define EASY_PROFILER_DISABLE 
#define EASY_EVENT(...)
#define EASY_THREAD(...)
#define EASY_MAIN_THREAD 
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
        void        PROFILER_API beginBlock(Block& _block);
        void        PROFILER_API endBlock();
        void        PROFILER_API setEnabled(bool isEnable);
        uint32_t    PROFILER_API dumpBlocksToFile(const char* filename);
        void        PROFILER_API setThreadName(const char* name, const char* filename, const char* _funcname, int line);
        void        PROFILER_API setContextSwitchLogFilename(const char* name);
        const char* PROFILER_API getContextSwitchLogFilename();
	}

	typedef uint64_t timestamp_t;
	typedef uint32_t thread_id_t;
    typedef uint32_t  block_id_t;

    enum BlockType : uint8_t
    {
        BLOCK_TYPE_EVENT = 0,
        BLOCK_TYPE_THREAD_SIGN,
        BLOCK_TYPE_BLOCK,
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

        inline const char* name() const { return m_name; }
        inline const char* file() const { return m_filename; }
    };

    class PROFILER_API BaseBlockData
    {
        friend ::ProfileManager;

    protected:

        timestamp_t m_begin;
        timestamp_t   m_end;
        block_id_t     m_id;

    public:

        BaseBlockData(const BaseBlockData&) = default;
        BaseBlockData(timestamp_t _begin_time, block_id_t _id);

        inline timestamp_t begin() const { return m_begin; }
        inline timestamp_t end() const { return m_end; }
        inline block_id_t id() const { return m_id; }
        inline timestamp_t duration() const { return m_end - m_begin; }

        inline void setId(block_id_t _id) { m_id = _id; }

    private:

        BaseBlockData() = delete;
    };
#pragma pack(pop)

    class PROFILER_API Block final : public BaseBlockData
    {
        friend ::ProfileManager;

        const char* m_name;

    private:

        void finish();
        void finish(timestamp_t _end_time);
        inline bool isFinished() const { return m_end >= m_begin; }

    public:

        Block(Block&& that);
        Block(block_type_t _block_type, block_id_t _id, const char* _name);
        Block(timestamp_t _begin_time, block_type_t _block_type, block_id_t _id, const char* _name);
        ~Block();

        inline const char* name() const { return m_name; }
    };

    //////////////////////////////////////////////////////////////////////

    class PROFILER_API StaticBlockDescriptor final
    {
        block_id_t m_id;

    public:

        StaticBlockDescriptor(const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color = DefaultBlockColor);
        inline block_id_t id() const { return m_id; }
    };

#ifndef _WIN32
    struct PROFILER_API ThreadNameSetter final
    {
        ThreadNameSetter(const char* _name, const char* _filename, const char* _funcname, int _line)
        {
            setThreadName(_name, _filename, _funcname, _line);
        }
    };
#endif

    //////////////////////////////////////////////////////////////////////
	
} // END of namespace profiler.

#if defined ( __clang__ )
#pragma clang diagnostic pop
#endif

#endif // EASY_PROFILER____H_______
