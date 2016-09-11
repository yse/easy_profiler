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

#include "profiler/profiler_aux.h"

#if defined ( __clang__ )
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#ifndef FULL_DISABLE_PROFILER

/**
\defgroup profiler Profiler
*/

/** If != 0 then EasyProfiler will measure time for blocks storage expansion.
If 0 then EasyProfiler will be compiled without blocks of code responsible
for measuring these events.

These are "EasyProfiler.ExpandStorage" blocks on a diagram.

\ingroup profiler
*/
# define EASY_MEASURE_STORAGE_EXPAND 0

/** If true then "EasyProfiler.ExpandStorage" events are enabled by default and will be
writed to output file or translated over the net.
If false then you need to enable these events via GUI if you'll want to see them.

\ingroup profiler
*/
# define EASY_STORAGE_EXPAND_ENABLED true

/** Macro of beginning of block with custom name and color.

\code
    #include "profiler/profiler.h"
    void foo()
    {
        // some code ...

        EASY_BLOCK("Check something", profiler::DISABLED); // Disabled block (There is possibility to enable this block later via GUI)
        if(something){
            EASY_BLOCK("Calling bar()"); // Block with default color
            bar();
        }
        else{
            EASY_BLOCK("Calling baz()", profiler::colors::Red); // Red block
            baz();
        }
        EASY_END_BLOCK; // End of "Check something" block (Even if "Check something" is disabled, this EASY_END_BLOCK will not end any other block).

        EASY_BLOCK("Some another block", profiler::colors::Blue, profiler::DISABLED); // Disabled block with Blue color
        // some another code...
    }
\endcode

Block will be automatically completed by destructor.

\ingroup profiler
*/
# define EASY_BLOCK(name, ...)\
    static const ::profiler::BlockDescRef EASY_UNIQUE_DESC(__LINE__) = ::profiler::registerDescription(::profiler::extract_enable_flag(__VA_ARGS__),\
        EASY_COMPILETIME_NAME(name), __FILE__, __LINE__, ::profiler::BLOCK_TYPE_BLOCK, ::profiler::extract_color(__VA_ARGS__));\
    ::profiler::Block EASY_UNIQUE_BLOCK(__LINE__)(EASY_UNIQUE_DESC(__LINE__), EASY_RUNTIME_NAME(name));\
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

    void baz(){
        EASY_FUNCTION(profiler::DISABLED); // Disabled block with name="baz" and default color (There is possibility to enable this block later via GUI)
        // som code...
    }
\endcode

Name of the block automatically created with function name.

\ingroup profiler
*/
# define EASY_FUNCTION(...)\
    static const ::profiler::BlockDescRef EASY_UNIQUE_DESC(__LINE__) = ::profiler::registerDescription(::profiler::extract_enable_flag(__VA_ARGS__),\
        __func__, __FILE__, __LINE__, ::profiler::BLOCK_TYPE_BLOCK, ::profiler::extract_color(__VA_ARGS__));\
    ::profiler::Block EASY_UNIQUE_BLOCK(__LINE__)(EASY_UNIQUE_DESC(__LINE__), "");\
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
# define EASY_END_BLOCK ::profiler::endBlock();

/** Macro of creating event with custom name and color.

Event is a block with zero duration and special type.

\warning Event ends immidiately and calling EASY_END_BLOCK after EASY_EVENT
will end previously opened EASY_BLOCK or EASY_FUNCTION.

\ingroup profiler
*/
# define EASY_EVENT(name, ...)\
    static const ::profiler::BlockDescRef EASY_UNIQUE_DESC(__LINE__) = \
        ::profiler::registerDescription(::profiler::extract_enable_flag(__VA_ARGS__), EASY_COMPILETIME_NAME(name), __FILE__, __LINE__,\
            ::profiler::BLOCK_TYPE_EVENT, ::profiler::extract_color(__VA_ARGS__));\
    ::profiler::storeBlock(EASY_UNIQUE_DESC(__LINE__), EASY_RUNTIME_NAME(name));

/** Macro enabling profiler

\ingroup profiler
*/
# define EASY_PROFILER_ENABLE ::profiler::setEnabled(true);

/** Macro disabling profiler

\ingroup profiler
*/
# define EASY_PROFILER_DISABLE ::profiler::setEnabled(false);

/** Macro of naming current thread.

If this thread has been already named then nothing changes.

\ingroup profiler
*/
# define EASY_THREAD(name)\
    EASY_THREAD_LOCAL static const char* EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__) = nullptr;\
    if (EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__) == nullptr)\
        EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__) = ::profiler::setThreadName(name, __FILE__, __func__, __LINE__);

/** Macro of naming main thread.

This is only for user comfort. There is no difference for EasyProfiler GUI between different threads.

\ingroup profiler
*/
# define EASY_MAIN_THREAD EASY_THREAD("Main")

#else
# define EASY_MEASURE_STORAGE_EXPAND 0
# define EASY_STORAGE_EXPAND_ENABLED false
# define EASY_BLOCK(...)
# define EASY_FUNCTION(...)
# define EASY_END_BLOCK 
# define EASY_PROFILER_ENABLE 
# define EASY_PROFILER_DISABLE 
# define EASY_EVENT(...)
# define EASY_THREAD(...)
# define EASY_MAIN_THREAD 
#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ProfileManager;
class ThreadStorage;

namespace profiler {

    //////////////////////////////////////////////////////////////////////
    // Core types

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

    //***********************************************

#pragma pack(push,1)
    class PROFILER_API BaseBlockDescriptor
    {
        friend ::ProfileManager;

    protected:

        block_id_t      m_id; ///< This descriptor id (We can afford this spending because there are much more blocks than descriptors)
        int           m_line; ///< Line number in the source file
        color_t      m_color; ///< Color of the block packed into 1-byte structure
        block_type_t  m_type; ///< Type of the block (See BlockType)
        bool       m_enabled; ///< If false then blocks with such id() will not be stored by profiler during profile session

        BaseBlockDescriptor(block_id_t _id, bool _enabled, int _line, block_type_t _block_type, color_t _color);

    public:

        inline block_id_t id() const { return m_id; }
        inline int line() const { return m_line; }
        inline color_t color() const { return m_color; }
        inline block_type_t type() const { return m_type; }
        inline bool enabled() const { return m_enabled; }

    }; // END of class BaseBlockDescriptor.

    //***********************************************

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

    }; // END of class BaseBlockData.
#pragma pack(pop)

    //***********************************************

    class PROFILER_API BlockDescriptor final : public BaseBlockDescriptor
    {
        friend ::ProfileManager;

        const char*     m_name; ///< Static name of all blocks of the same type (blocks can have dynamic name) which is, in pair with descriptor id, a unique block identifier
        const char* m_filename; ///< Source file name where this block is declared

        BlockDescriptor(uint64_t& _used_mem, bool _enabled, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color);
        void setId(block_id_t _id);

    public:

        BlockDescriptor(uint64_t& _used_mem, block_id_t _id, bool _enabled, const char* _name, const char* _filename, int _line, block_type_t _block_type, color_t _color);

        inline const char* name() const {
            return m_name;
        }

        inline const char* file() const {
            return m_filename;
        }

    }; // END of class BlockDescriptor.

    //***********************************************

    class PROFILER_API Block final : public BaseBlockData
    {
        friend ::ProfileManager;
        friend ::ThreadStorage;

        const char* m_name;
        bool     m_enabled;

    private:

        void start();
        void start(timestamp_t _time);
        void finish();
        void finish(timestamp_t _time);
        inline bool finished() const { return m_end >= m_begin; }
        inline bool enabled() const { return m_enabled; }

    public:

        Block(Block&& that);
        Block(const BaseBlockDescriptor& _desc, const char* _runtimeName);
        Block(timestamp_t _begin_time, block_id_t _id, const char* _runtimeName);
        ~Block();

        inline const char* name() const { return m_name; }

    }; // END of class Block.

    //***********************************************

    class PROFILER_API BlockDescRef final
    {
        const BaseBlockDescriptor& m_desc;

    public:

        BlockDescRef(const BaseBlockDescriptor& _desc) : m_desc(_desc) { }
        inline operator const BaseBlockDescriptor& () const { return m_desc; }
        ~BlockDescRef();

    private:

        BlockDescRef() = delete;
        BlockDescRef(const BlockDescRef&) = delete;
        BlockDescRef& operator = (const BlockDescRef&) = delete;

    }; // END of class BlockDescRef.

    //////////////////////////////////////////////////////////////////////
    // Core API

    extern "C" {
        PROFILER_API const BaseBlockDescriptor& registerDescription(bool _enabled, const char* _compiletimeName, const char* _filename, int _line, block_type_t _block_type, color_t _color);
        PROFILER_API void        storeBlock(const BaseBlockDescriptor& _desc, const char* _runtimeName);
        PROFILER_API void        beginBlock(Block& _block);
        PROFILER_API void        endBlock();
        PROFILER_API void        setEnabled(bool _isEnable);
        PROFILER_API uint32_t    dumpBlocksToFile(const char* _filename);
        PROFILER_API const char* setThreadName(const char* _name, const char* _filename, const char* _funcname, int _line);
        PROFILER_API void        setContextSwitchLogFilename(const char* _name);
        PROFILER_API const char* getContextSwitchLogFilename();
    }

    inline void setEnabled(::profiler::EasyEnableFlag _isEnable) {
        setEnabled(_isEnable == ::profiler::ENABLED);
    }

    //////////////////////////////////////////////////////////////////////

} // END of namespace profiler.

#if defined ( __clang__ )
# pragma clang diagnostic pop
#endif

#endif // EASY_PROFILER____H_______
