/**
Lightweight profiler library for c++
Copyright(C) 2016-2018  Sergey Yagovtsev, Victor Zarubkin

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

#include "thread_storage.h"
#include "current_thread.h"
#include "current_time.h"

ThreadStorage::ThreadStorage()
    : nonscopedBlocks(16)
    , frameStartTime(0)
    , id(getCurrentThreadId())
    , stackSize(0)
    , allowChildren(true)
    , named(false)
    , guarded(false)
    , frameOpened(false)
{
    expired = ATOMIC_VAR_INIT(0);
}

void ThreadStorage::storeValue(profiler::timestamp_t _timestamp, profiler::block_id_t _id, profiler::DataType _type, const void* _data, uint16_t _size, bool _isArray, profiler::ValueId _vin)
{
    const uint16_t serializedDataSize = _size + static_cast<uint16_t>(sizeof(profiler::ArbitraryValue));
    void* data = blocks.closedList.allocate(serializedDataSize);

    ::new (data) profiler::ArbitraryValue(_timestamp, _vin.m_id, _id, _size, _type, _isArray);

    char* cdata = reinterpret_cast<char*>(data);
    memcpy(cdata + sizeof(profiler::ArbitraryValue), _data, _size);

    blocks.frameMemorySize += serializedDataSize;

    putMarkIfEmpty();
}

void ThreadStorage::storeBlock(const profiler::Block& block)
{
#if EASY_OPTION_MEASURE_STORAGE_EXPAND != 0
    EASY_LOCAL_STATIC_PTR(const BaseBlockDescriptor*, desc, \
                          MANAGER.addBlockDescriptor(EASY_OPTION_STORAGE_EXPAND_BLOCKS_ON ? profiler::ON : profiler::OFF, EASY_UNIQUE_LINE_ID, "EasyProfiler.ExpandStorage", \
                                                     __FILE__, __LINE__, profiler::BlockType::Block, EASY_COLOR_INTERNAL_EVENT));

    EASY_THREAD_LOCAL static profiler::timestamp_t beginTime = 0ULL;
    EASY_THREAD_LOCAL static profiler::timestamp_t endTime = 0ULL;
#endif

    const uint16_t nameLength = static_cast<uint16_t>(strlen(block.name()));
#if EASY_OPTION_MEASURE_STORAGE_EXPAND == 0
    const 
#endif
    uint16_t serializedDataSize = static_cast<uint16_t>(sizeof(profiler::BaseBlockData) + nameLength + 1);

#if EASY_OPTION_MEASURE_STORAGE_EXPAND != 0
    const bool expanded = (desc->m_status & profiler::ON) && blocks.closedList.need_expand(serializedDataSize);
    if (expanded) beginTime = profiler::clock::now();
#endif

    void* data = blocks.closedList.allocate(serializedDataSize);

#if EASY_OPTION_MEASURE_STORAGE_EXPAND != 0
    if (expanded) endTime = profiler::clock::now();
#endif

    ::new (data) profiler::SerializedBlock(block, nameLength);
    blocks.frameMemorySize += serializedDataSize;

#if EASY_OPTION_MEASURE_STORAGE_EXPAND != 0
    if (expanded)
    {
        profiler::Block b(beginTime, desc->id(), "");
        b.finish(endTime);

        serializedDataSize = static_cast<uint16_t>(sizeof(profiler::BaseBlockData) + 1);
        data = blocks.closedList.allocate(serializedDataSize);
        ::new (data) profiler::SerializedBlock(b, 0);
        blocks.frameMemorySize += serializedDataSize;
    }
#endif
}

void ThreadStorage::storeBlockForce(const profiler::Block& block)
{
    const uint16_t nameLength = static_cast<uint16_t>(strlen(block.name()));
    const uint16_t serializedDataSize = static_cast<uint16_t>(sizeof(profiler::BaseBlockData) + nameLength + 1);

    void* data = blocks.closedList.marked_allocate(serializedDataSize);
    ::new (data) profiler::SerializedBlock(block, nameLength);
    blocks.usedMemorySize += serializedDataSize;
}

void ThreadStorage::storeCSwitch(const CSwitchBlock& block)
{
    const uint16_t nameLength = static_cast<uint16_t>(strlen(block.name()));
    const uint16_t serializedDataSize = static_cast<uint16_t>(sizeof(profiler::CSwitchEvent) + nameLength + 1);

    void* data = sync.closedList.allocate(serializedDataSize);
    ::new (data) profiler::SerializedCSwitch(block, nameLength);
    sync.usedMemorySize += serializedDataSize;
}

void ThreadStorage::clearClosed()
{
    blocks.clearClosed();
    sync.clearClosed();
}

void ThreadStorage::popSilent()
{
    if (!blocks.openedList.empty())
    {
        profiler::Block& top = blocks.openedList.back();
        top.m_end = top.m_begin;
        if (!top.m_isScoped)
            nonscopedBlocks.pop();
        blocks.openedList.pop_back();
    }
}

void ThreadStorage::beginFrame()
{
    if (!frameOpened)
    {
        frameStartTime = profiler::clock::now();
        frameOpened = true;
    }
}

profiler::timestamp_t ThreadStorage::endFrame()
{
    frameOpened = false;
    return profiler::clock::now() - frameStartTime;
}

void ThreadStorage::putMark()
{
    blocks.closedList.put_mark();
    blocks.usedMemorySize += blocks.frameMemorySize;
    blocks.frameMemorySize = 0;
}

void ThreadStorage::putMarkIfEmpty()
{
    if (!frameOpened)
        putMark();
}
