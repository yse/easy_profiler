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

#ifndef EASY_PROFILER_SERIALIZED_BLOCK__H_______
#define EASY_PROFILER_SERIALIZED_BLOCK__H_______

#include "profiler/profiler.h"

class ThreadStorage;

namespace profiler {

    //////////////////////////////////////////////////////////////////////////

    class PROFILER_API SerializedBlock final : public BaseBlockData
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
    class PROFILER_API SerializedBlockDescriptor final : public BaseBlockDescriptor
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

    private:

        SerializedBlockDescriptor(const SerializedBlockDescriptor&) = delete;
        SerializedBlockDescriptor& operator = (const SerializedBlockDescriptor&) = delete;
        ~SerializedBlockDescriptor() = delete;

    }; // END of SerializedBlockDescriptor.
#pragma pack(pop)

    //////////////////////////////////////////////////////////////////////////

} // END of namespace profiler.

#endif // EASY_PROFILER_SERIALIZED_BLOCK__H_______
