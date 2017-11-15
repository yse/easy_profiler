/**
Lightweight profiler library for c++
Copyright(C) 2016-2017  Sergey Yagovtsev, Victor Zarubkin

Licensed under either of
    * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
    * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
at your option.

The MIT License
    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights 
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
    of the Software, and to permit persons to whom the Software is furnished 
    to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all 
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
    INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
    PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
    LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
    USE OR OTHER DEALINGS IN THE SOFTWARE.


The Apache License, Version 2.0 (the "License");
    You may not use this file except in compliance with the License.
    You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

**/

#ifndef EASY_PROFILER_THREAD_STORAGE_H
#define EASY_PROFILER_THREAD_STORAGE_H

#include <easy/details/profiler_public_types.h>
#include <easy/details/arbitrary_value_public_types.h>
#include <easy/serialized_block.h>
#include <vector>
#include <string>
#include <atomic>
#include <functional>
#include "stack_buffer.h"
#include "chunk_allocator.h"

//////////////////////////////////////////////////////////////////////////

template <class T, const uint16_t N>
struct BlocksList
{
    BlocksList() = default;

    std::vector<T>            openedList;
    chunk_allocator<N>        closedList;
    uint64_t          usedMemorySize = 0;

    void clearClosed() {
        //closedList.clear();
        usedMemorySize = 0;
    }

private:

    BlocksList(const BlocksList&) = delete;
    BlocksList(BlocksList&&) = delete;

}; // END of struct BlocksList.

//////////////////////////////////////////////////////////////////////////

class CSwitchBlock : public profiler::CSwitchEvent
{
    const char* m_name;

public:

    CSwitchBlock(profiler::timestamp_t _begin_time, profiler::thread_id_t _tid, const char* _runtimeName) EASY_NOEXCEPT;
    inline const char* name() const EASY_NOEXCEPT { return m_name; }
};

//////////////////////////////////////////////////////////////////////////

const uint16_t SIZEOF_BLOCK = sizeof(profiler::BaseBlockData) + 1 + sizeof(uint16_t); // SerializedBlock stores BaseBlockData + at least 1 character for name ('\0') + 2 bytes for size of serialized data
const uint16_t SIZEOF_CSWITCH = sizeof(profiler::CSwitchEvent) + 1 + sizeof(uint16_t); // SerializedCSwitch also stores additional 4 bytes to be able to save 64-bit thread_id

struct ThreadStorage EASY_FINAL
{
    StackBuffer<NonscopedBlock>                                                 nonscopedBlocks;
    BlocksList<std::reference_wrapper<profiler::Block>, SIZEOF_BLOCK * (uint16_t)128U>   blocks;
    BlocksList<CSwitchBlock, SIZEOF_CSWITCH * (uint16_t)128U>                              sync;

    std::string                     name; ///< Thread name
    profiler::timestamp_t frameStartTime; ///< Current frame start time. Used to calculate FPS.
    const profiler::thread_id_t       id; ///< Thread ID
    std::atomic<char>            expired; ///< Is thread expired
    std::atomic_bool profiledFrameOpened; ///< Is new profiled frame opened (this is true when profiling is enabled and there is an opened frame) \sa frameOpened
    int32_t                    stackSize; ///< Current thread stack depth. Used when switching profiler state to begin collecting blocks only when new frame would be opened.
    bool                   allowChildren; ///< False if one of previously opened blocks has OFF_RECURSIVE or ON_WITHOUT_CHILDREN status
    bool                           named; ///< True if thread name was set
    bool                         guarded; ///< True if thread has been registered using ThreadGuard
    bool                     frameOpened; ///< Is new frame opened (this does not depend on profiling status) \sa profiledFrameOpened
    bool                            halt; ///< This is set to true when new frame started while dumping blocks. Used to restrict collecting blocks during dumping process.

    void storeValue(profiler::timestamp_t _timestamp, profiler::block_id_t _id, profiler::DataType _type, const void* _data, size_t _size, bool _isArray, profiler::ValueId _vin);
    void storeBlock(const profiler::Block& _block);
    void storeCSwitch(const CSwitchBlock& _block);
    void clearClosed();
    void popSilent();

    void beginFrame();
    profiler::timestamp_t endFrame();

    ThreadStorage();

private:

    ThreadStorage(const ThreadStorage&) = delete;
    ThreadStorage(ThreadStorage&&) = delete;

}; // END of struct ThreadStorage.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_THREAD_STORAGE_H
