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

#ifndef EASY_PROFILER_SERIALIZED_BLOCK__H_______
#define EASY_PROFILER_SERIALIZED_BLOCK__H_______

#include "easy/profiler.h"

namespace profiler {

    //////////////////////////////////////////////////////////////////////////

    class PROFILER_API SerializedBlock EASY_FINAL : public BaseBlockData
    {
        friend ::ProfileManager;
        friend ::ThreadStorage;

    public:

        inline const char* data() const { return reinterpret_cast<const char*>(this); }
        inline const char* name() const { return data() + sizeof(BaseBlockData); }

    private:

        SerializedBlock(const ::profiler::Block& block, uint16_t name_length);

        SerializedBlock(const SerializedBlock&) = delete;
        SerializedBlock& operator = (const SerializedBlock&) = delete;
        ~SerializedBlock() = delete;

    }; // END of SerializedBlock.

    //////////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)
    class PROFILER_API SerializedBlockDescriptor EASY_FINAL : public BaseBlockDescriptor
    {
        uint16_t m_nameLength;

    public:

        inline const char* data() const {
            return reinterpret_cast<const char*>(this);
        }

        inline const char* name() const {
            static const auto shift = sizeof(BaseBlockDescriptor) + sizeof(decltype(m_nameLength));
            return data() + shift;
        }

        inline const char* file() const {
            return name() + m_nameLength;
        }

        inline void setStatus(EasyBlockStatus _status)
        {
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

#endif // EASY_PROFILER_SERIALIZED_BLOCK__H_______
