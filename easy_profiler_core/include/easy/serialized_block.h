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

#ifndef EASY_PROFILER_SERIALIZED_BLOCK_H
#define EASY_PROFILER_SERIALIZED_BLOCK_H

#include <easy/profiler.h>

class CSwitchBlock;

namespace profiler {

    //////////////////////////////////////////////////////////////////////////

    class PROFILER_API SerializedBlock : public BaseBlockData
    {
        friend ::ProfileManager;
        friend ::ThreadStorage;

    public:

        inline const char* data() const { return reinterpret_cast<const char*>(this); }

        ///< Run-time block name is stored right after main BaseBlockData data
        inline const char* name() const { return data() + sizeof(BaseBlockData); }

    protected:

        SerializedBlock(const ::profiler::Block& block, uint16_t name_length);

        SerializedBlock(const SerializedBlock&) = delete;
        SerializedBlock& operator = (const SerializedBlock&) = delete;

        //TODO yse: reason of deleted
        //~SerializedBlock() = delete;

    protected:

        SerializedBlock(const ::profiler::Block& block);

    }; // END of SerializedBlock.

    //////////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)

    /** This is serious work-around to be able to read/write valid
    thread ids after changing thread_id_t from 32-bit to 64-bit.
    
    Before v1.3.0 thread_id_t was uint32_t and has been stored in BaseBlockData::m_id.
    After v1.3.0 we have to allocate additional 4 bytes per context switch block to
    be able to store 64-bit value.

    This is bad design decision at first look, but it has backward compatibility,
    does not require serious changes to Core API (except this particular class)
    and saves 4 bytes per context switch block.

    TODO: think about better solution.
    */
    class PROFILER_API SerializedCSwitch : public SerializedBlock
    {
        friend ::ProfileManager;
        friend ::ThreadStorage;

        uint32_t m_reserve; /** Additional 4 bytes used to store second part of thread_id_t
                            (first part is stored in BaseBlockData::m_id as it was before
                            changing thread_id_t to 64-bit) */

    public:

        ///< Thread id is stored in m_id + m_reserve
        inline thread_id_t tid() const { return *reinterpret_cast<const thread_id_t*>(&m_id); }

        ///< Run-time block name is stored right after main BaseBlockData data and reserved 4 bytes block
        inline const char* name() const { return data() + sizeof(BaseBlockData) + 4; }

    private:

        SerializedCSwitch(const CSwitchBlock& block, uint16_t name_length);

        SerializedCSwitch(const SerializedBlock&) = delete;
        SerializedCSwitch& operator = (const SerializedBlock&) = delete;
        ~SerializedCSwitch() = delete;

    }; // END of SerializedCSwitch.

    //////////////////////////////////////////////////////////////////////////

    class PROFILER_API SerializedBlockDescriptor EASY_FINAL : public BaseBlockDescriptor
    {
        uint16_t m_nameLength; ///< Length of the name including trailing '\0' sybmol

    public:

        inline const char* data() const {
            return reinterpret_cast<const char*>(this);
        }

        ///< Name is stored right after m_nameLength
        inline const char* name() const {
            static const auto shift = sizeof(BaseBlockDescriptor) + sizeof(decltype(m_nameLength));
            return data() + shift;
        }

        ///< File name is stored right after the name
        inline const char* file() const {
            return name() + m_nameLength;
        }

        inline void setStatus(EasyBlockStatus _status) {
            m_status = _status;
        }

    private:

        SerializedBlockDescriptor(const SerializedBlockDescriptor&) = delete;
        SerializedBlockDescriptor& operator = (const SerializedBlockDescriptor&) = delete;
        ~SerializedBlockDescriptor() = delete;

    }; // END of SerializedBlockDescriptor.
#pragma pack(pop)

    //////////////////////////////////////////////////////////////////////////

} // END of namespace profiler.

#endif // EASY_PROFILER_SERIALIZED_BLOCK_H
