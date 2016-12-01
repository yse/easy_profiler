/**
Lightweight profiler library for c++
Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


GNU General Public License Usage
Alternatively, this file may be used under the terms of the GNU
General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.If not, see <http://www.gnu.org/licenses/>.
**/

#ifndef EASY_PROFILER____H_______
#define EASY_PROFILER____H_______

#include "easy/profiler_aux.h"

#if defined ( __clang__ )
# pragma clang diagnostic push
# pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#ifdef BUILD_WITH_EASY_PROFILER

/**
\defgroup profiler EasyProfiler
*/

// EasyProfiler core API:

/** Macro for beginning of a block with custom name and color.

\code
    #include "easy/profiler.h"
    void foo()
    {
        // some code ...

        EASY_BLOCK("Check something", profiler::OFF); // Disabled block (There is possibility to enable this block later via GUI)
        if(something){
            EASY_BLOCK("Calling bar()"); // Block with default color
            bar();
        }
        else{
            EASY_BLOCK("Calling baz()", profiler::colors::Red); // Red block
            baz();
        }
        EASY_END_BLOCK; // End of "Check something" block (Even if "Check something" is disabled, this EASY_END_BLOCK will not end any other block).

        EASY_BLOCK("Some another block", profiler::colors::Blue, profiler::ON_WITHOUT_CHILDREN); // Block with Blue color without
        // some another code...
        EASY_BLOCK("Calculate sum"); // This block will not be profiled because it's parent is ON_WITHOUT_CHILDREN
        int sum = 0;
        for (int i = 0; i < 10; ++i)
            sum += i;
        EASY_END_BLOCK; // End of "Calculate sum" block
    }
\endcode

Block will be automatically completed by destructor.

\ingroup profiler
*/
# define EASY_BLOCK(name, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(::profiler::extract_enable_flag(__VA_ARGS__),\
        EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name), __FILE__, __LINE__, ::profiler::BLOCK_TYPE_BLOCK, ::profiler::extract_color(__VA_ARGS__)));\
    ::profiler::Block EASY_UNIQUE_BLOCK(__LINE__)(EASY_UNIQUE_DESC(__LINE__), EASY_RUNTIME_NAME(name));\
    ::profiler::beginBlock(EASY_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

/** Macro for beginning of a block with function name and custom color.

\code
    #include "easy/profiler.h"
    void foo(){
        EASY_FUNCTION(); // Block with name="foo" and default color
        //some code...
    }

    void bar(){
        EASY_FUNCTION(profiler::colors::Green); // Green block with name="bar"
        //some code...
    }

    void baz(){
        EASY_FUNCTION(profiler::FORCE_ON); // Force enabled block with name="baz" and default color (This block will be profiled even if it's parent is OFF_RECURSIVE)
        // som code...
    }
\endcode

Name of the block automatically created with function name.

\ingroup profiler
*/
# define EASY_FUNCTION(...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(::profiler::extract_enable_flag(__VA_ARGS__),\
        EASY_UNIQUE_LINE_ID, __func__, __FILE__, __LINE__, ::profiler::BLOCK_TYPE_BLOCK, ::profiler::extract_color(__VA_ARGS__)));\
    ::profiler::Block EASY_UNIQUE_BLOCK(__LINE__)(EASY_UNIQUE_DESC(__LINE__), "");\
    ::profiler::beginBlock(EASY_UNIQUE_BLOCK(__LINE__)); // this is to avoid compiler warning about unused variable

/** Macro for completion of last opened block.

\code
#include "easy/profiler.h"
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

/** Macro for creating event with custom name and color.

Event is a block with zero duration and special type.

\warning Event ends immidiately and calling EASY_END_BLOCK after EASY_EVENT
will end previously opened EASY_BLOCK or EASY_FUNCTION.

\ingroup profiler
*/
# define EASY_EVENT(name, ...)\
    EASY_LOCAL_STATIC_PTR(const ::profiler::BaseBlockDescriptor*, EASY_UNIQUE_DESC(__LINE__), ::profiler::registerDescription(\
        ::profiler::extract_enable_flag(__VA_ARGS__), EASY_UNIQUE_LINE_ID, EASY_COMPILETIME_NAME(name),\
            __FILE__, __LINE__, ::profiler::BLOCK_TYPE_EVENT, ::profiler::extract_color(__VA_ARGS__)));\
    ::profiler::storeEvent(EASY_UNIQUE_DESC(__LINE__), EASY_RUNTIME_NAME(name));

/** Macro for enabling profiler.

\ingroup profiler
*/
# define EASY_PROFILER_ENABLE ::profiler::setEnabled(true);

/** Macro for disabling profiler.

\ingroup profiler
*/
# define EASY_PROFILER_DISABLE ::profiler::setEnabled(false);

/** Macro for current thread registration.

\note If this thread has been already registered then nothing happens.

\ingroup profiler
*/
# define EASY_THREAD(name)\
    EASY_THREAD_LOCAL static const char* EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__) = 0;\
    if (!EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__))\
        EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__) = ::profiler::registerThread(name);

/** Macro for current thread registration and creating a thread guard object.

\note If this thread has been already registered then nothing happens.

\note Also creates thread guard which marks thread as "expired" on it's destructor
and creates "ThreadFinished" profiler event.

\ingroup profiler
*/
# define EASY_THREAD_SCOPE(name)\
    EASY_THREAD_LOCAL static const char* EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__) = 0;\
    ::profiler::ThreadGuard EASY_TOKEN_CONCATENATE(unique_profiler_thread_guard, __LINE__);\
    if (!EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__))\
        EASY_TOKEN_CONCATENATE(unique_profiler_thread_name, __LINE__) = ::profiler::registerThreadScoped(name,\
        EASY_TOKEN_CONCATENATE(unique_profiler_thread_guard, __LINE__));

/** Macro for main thread registration.

This is just for user's comfort. There is no difference for EasyProfiler GUI between different threads.

\ingroup profiler
*/
# define EASY_MAIN_THREAD EASY_THREAD("Main")

/** Enable or disable event tracing (context switch events).

\note Default value is controlled by EASY_EVENT_TRACING_ENABLED macro.

\note Change will take effect on the next call to EASY_PROFILER_ENABLE.

\sa EASY_PROFILER_ENABLE, EASY_EVENT_TRACING_ENABLED

\ingroup profiler
*/
# define EASY_SET_EVENT_TRACING_ENABLED(isEnabled) ::profiler::setEventTracingEnabled(isEnabled);

/** Set event tracing thread priority (low or normal).

Event tracing with low priority will affect your application performance much more less, but
it can be late to gather information about thread/process (thread could be finished to the moment
when event tracing thread will be awaken) and you will not see process name and process id
information in GUI for such threads. You will still be able to see all context switch events.

Event tracing with normal priority could gather more information about processes but potentially
it could affect performance as it has more work to do. Usually you will not notice any performance
breakdown, but if you care about that then you change set event tracing priority level to low.

\sa EASY_LOW_PRIORITY_EVENT_TRACING

\ingroup profiler
*/
# define EASY_SET_LOW_PRIORITY_EVENT_TRACING(isLowPriority) ::profiler::setLowPriorityEventTracing(isLowPriority);

/** Macro for setting temporary log-file path for Unix event tracing system.

\note Default value is "/tmp/cs_profiling_info.log".

\ingroup profiler
*/
#  define EASY_EVENT_TRACING_SET_LOG(filename) ::profiler::setContextSwitchLogFilename(filename);

/** Macro returning current path to the temporary log-file for Unix event tracing system.

\ingroup profiler
*/
#  define EASY_EVENT_TRACING_LOG ::profiler::getContextSwitchLogFilename();

// EasyProfiler settings:

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

/** If true then EasyProfiler event tracing is enabled by default
and will be turned on and off when you call profiler::setEnabled().
Otherwise, it have to be turned on via GUI and then it will be
turned on/off with next calls of profiler::setEnabled().

\ingroup profiler
*/
# define EASY_EVENT_TRACING_ENABLED true

/** If true then EasyProfiler.ETW thread (Event tracing for Windows) will have low priority by default.

\sa EASY_SET_LOW_PRIORITY_EVENT_TRACING

\note You can always change priority level via GUI or API while profiling session is not launched.
You don't need to rebuild or restart your application for that.

\ingroup profiler
*/
# define EASY_LOW_PRIORITY_EVENT_TRACING true


/** If != 0 then EasyProfiler will print error messages into stderr.
Otherwise, no log messages will be printed.

\ingroup profiler
*/
# define EASY_LOG_ENABLED 0


#else // #ifdef BUILD_WITH_EASY_PROFILER

# define EASY_BLOCK(...)
# define EASY_FUNCTION(...)
# define EASY_END_BLOCK 
# define EASY_PROFILER_ENABLE 
# define EASY_PROFILER_DISABLE 
# define EASY_EVENT(...)
# define EASY_THREAD(...)
# define EASY_THREAD_SCOPE(...)
# define EASY_MAIN_THREAD 
# define EASY_SET_EVENT_TRACING_ENABLED(isEnabled) 
# define EASY_SET_LOW_PRIORITY_EVENT_TRACING(isLowPriority) 

# ifndef _WIN32
#  define EASY_EVENT_TRACING_SET_LOG(filename) 
#  define EASY_EVENT_TRACING_LOG ""
# endif

# define EASY_MEASURE_STORAGE_EXPAND 0
# define EASY_STORAGE_EXPAND_ENABLED false
# define EASY_EVENT_TRACING_ENABLED false
# define EASY_LOW_PRIORITY_EVENT_TRACING true
# define EASY_LOG_ENABLED 0

#endif // #ifndef BUILD_WITH_EASY_PROFILER

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ProfileManager;
struct ThreadStorage;

namespace profiler {

    //////////////////////////////////////////////////////////////////////
    // Core types

    const uint16_t DEFAULT_PORT = 28077;

    typedef uint64_t timestamp_t;
    typedef uint32_t thread_id_t;
    typedef uint32_t  block_id_t;

    enum BlockType : uint8_t
    {
        BLOCK_TYPE_EVENT = 0,
        BLOCK_TYPE_BLOCK,

        BLOCK_TYPES_NUMBER
    };
    typedef BlockType block_type_t;

    //***********************************************

#pragma pack(push,1)
    class PROFILER_API BaseBlockDescriptor
    {
        friend ::ProfileManager;
        friend ::ThreadStorage;

    protected:

        block_id_t          m_id; ///< This descriptor id (We can afford this spending because there are much more blocks than descriptors)
        int               m_line; ///< Line number in the source file
        color_t          m_color; ///< Color of the block packed into 1-byte structure
        block_type_t      m_type; ///< Type of the block (See BlockType)
        EasyBlockStatus m_status; ///< If false then blocks with such id() will not be stored by profiler during profile session

        BaseBlockDescriptor(block_id_t _id, EasyBlockStatus _status, int _line, block_type_t _block_type, color_t _color);

    public:

        inline block_id_t id() const { return m_id; }
        inline int line() const { return m_line; }
        inline color_t color() const { return m_color; }
        inline block_type_t type() const { return m_type; }
        inline EasyBlockStatus status() const { return m_status; }

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

    class PROFILER_API Block EASY_FINAL : public BaseBlockData
    {
        friend ::ProfileManager;
        friend ::ThreadStorage;

        const char*       m_name;
        EasyBlockStatus m_status;

    private:

        void start();
        void start(timestamp_t _time);
        void finish();
        void finish(timestamp_t _time);
        inline bool finished() const { return m_end >= m_begin; }
        inline EasyBlockStatus status() const { return m_status; }
        inline void setStatus(EasyBlockStatus _status) { m_status = _status; }

    public:

        Block(Block&& that);
        Block(const BaseBlockDescriptor* _desc, const char* _runtimeName);
        Block(timestamp_t _begin_time, block_id_t _id, const char* _runtimeName);
        ~Block();

        inline const char* name() const { return m_name; }

    private:

        Block(const Block&) = delete;
        Block& operator = (const Block&) = delete;

    }; // END of class Block.

    //***********************************************

    class PROFILER_API ThreadGuard EASY_FINAL {
        friend ::ProfileManager;
        thread_id_t m_id = 0;
    public:
        ~ThreadGuard();
    };

    //////////////////////////////////////////////////////////////////////
    // Core API
    // Note: it is better to use macros defined above than a direct calls to API.

#ifdef BUILD_WITH_EASY_PROFILER
    extern "C" {

        /** Registers static description of a block.

        It is general information which is common for all such blocks.
        Includes color, block type (see BlockType), file-name, line-number, compile-time name of a block and enable-flag.

        \ingroup profiler
        */
        PROFILER_API const BaseBlockDescriptor* registerDescription(EasyBlockStatus _status, const char* _autogenUniqueId, const char* _compiletimeName, const char* _filename, int _line, block_type_t _block_type, color_t _color);

        /** Stores event in the blocks list.

        An event ends instantly and has zero duration.

        \param _desc Reference to the previously registered description.
        \param _runtimeName Standard zero-terminated string which will be copied to the events buffer.
        
        \ingroup profiler
        */
        PROFILER_API void storeEvent(const BaseBlockDescriptor* _desc, const char* _runtimeName);

        /** Begins block.

        \ingroup profiler
        */
        PROFILER_API void beginBlock(Block& _block);

        /** Ends last started block.

        \ingroup profiler
        */
        PROFILER_API void endBlock();

        /** Enable or disable profiler.

        \ingroup profiler
        */
        PROFILER_API void setEnabled(bool _isEnable);

        /** Save all gathered blocks into file.

        \note This also disables profiler.

        \ingroup profiler
        */
        PROFILER_API uint32_t dumpBlocksToFile(const char* _filename);

        /** Register current thread and give it a name.

        \note Only first call of registerThread() for the current thread will have an effect.

        \ingroup profiler
        */
        PROFILER_API const char* registerThreadScoped(const char* _name, ThreadGuard&);

        /** Register current thread and give it a name.

        \note Only first call of registerThread() for the current thread will have an effect.

        \ingroup profiler
        */
        PROFILER_API const char* registerThread(const char* _name);

        /** Enable or disable event tracing.

        \note This change will take an effect on the next call of setEnabled(true);

        \sa setEnabled, EASY_SET_EVENT_TRACING_ENABLED

        \ingroup profiler
        */
        PROFILER_API void setEventTracingEnabled(bool _isEnable);

        /** Set event tracing thread priority (low or normal).

        \note This change will take effect on the next call of setEnabled(true);

        \sa setEnabled, EASY_SET_LOW_PRIORITY_EVENT_TRACING

        \ingroup profiler
        */
        PROFILER_API void setLowPriorityEventTracing(bool _isLowPriority);

        /** Set temporary log-file path for Unix event tracing system.

        \note Default value is "/tmp/cs_profiling_info.log".

        \ingroup profiler
        */
        PROFILER_API void setContextSwitchLogFilename(const char* _name);

        /** Returns current path to the temporary log-file for Unix event tracing system.

        \ingroup profiler
        */
        PROFILER_API const char* getContextSwitchLogFilename();

        PROFILER_API void startListen(uint16_t _port = profiler::DEFAULT_PORT);
        PROFILER_API void stopListen();

        /** Returns current major version.
        
        \ingroup profiler
        */
        PROFILER_API uint8_t versionMajor();

        /** Returns current minor version.

        \ingroup profiler
        */
        PROFILER_API uint8_t versionMinor();

        /** Returns current version patch.

        \ingroup profiler
        */
        PROFILER_API uint16_t versionPatch();

        /** Returns current version in 32-bit integer format.

        \ingroup profiler
        */
        PROFILER_API uint32_t version();

        /** Returns current version in 32-bit integer format.

        \ingroup profiler
        */
        PROFILER_API const char* versionName();

    }
#else
    inline const BaseBlockDescriptor* registerDescription(EasyBlockStatus, const char*, const char*, const char*, int, block_type_t, color_t)
    { return reinterpret_cast<const BaseBlockDescriptor*>(0xbad); }
    inline void endBlock() { }
    inline void setEnabled(bool) { }
    inline void storeEvent(const BaseBlockDescriptor*, const char*) { }
    inline void beginBlock(Block&) { }
    inline uint32_t dumpBlocksToFile(const char*) { return 0; }
    inline const char* registerThreadScoped(const char*, ThreadGuard&) { return ""; }
    inline const char* registerThread(const char*) { return ""; }
    inline void setEventTracingEnabled(bool) { }
    inline void setLowPriorityEventTracing(bool) { }
    inline void setContextSwitchLogFilename(const char*) { }
    inline const char* getContextSwitchLogFilename() { return ""; }
    inline void startListen(uint16_t _port = profiler::DEFAULT_PORT) { }
    inline void stopListen() { }
    inline uint8_t versionMajor() { return 0; }
    inline uint8_t versionMinor() { return 0; }
    inline uint16_t versionPatch() { return 0; }
    inline uint32_t version() { return 0; }
    inline const char* versionName() { return "v0.0.0_disabled"; }
#endif

    //////////////////////////////////////////////////////////////////////

} // END of namespace profiler.

#if defined ( __clang__ )
# pragma clang diagnostic pop
#endif

#endif // EASY_PROFILER____H_______
