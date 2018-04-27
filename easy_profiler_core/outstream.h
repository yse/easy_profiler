/************************************************************************
* file name         : outstream.h
* ----------------- :
* creation time     : 2016/09/11
* authors           : Sergey Yagovtsev, Victor Zarubkin
* emails            : yse.sey@gmail.com, v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains definition of output stream helpers.
* ----------------- :
* change log        : * 2016/09/11 Victor Zarubkin: Initial commit. Moved sources from profiler_manager.h/.cpp
*                   : 
*                   : *
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : This program is free software : you can redistribute it and / or modify
*                   : it under the terms of the GNU General Public License as published by
*                   : the Free Software Foundation, either version 3 of the License, or
*                   : (at your option) any later version.
*                   :
*                   : This program is distributed in the hope that it will be useful,
*                   : but WITHOUT ANY WARRANTY; without even the implied warranty of
*                   : MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*                   : GNU General Public License for more details.
*                   :
*                   : You should have received a copy of the GNU General Public License
*                   : along with this program.If not, see <http://www.gnu.org/licenses/>.
************************************************************************/

#ifndef EASY_PROFILER__OUTPUT_STREAM__H_
#define EASY_PROFILER__OUTPUT_STREAM__H_

#include <sstream>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace profiler {

    class OStream
    {
        ::std::stringstream m_stream;

    public:

        explicit OStream() : m_stream(std::ios_base::out | std::ios_base::binary)
        {

        }

        template <typename T> void write(const char* _data, T _size)
        {
            m_stream.write(_data, _size);
        }

        template <class T> void write(const T& _data)
        {
            m_stream.write((const char*)&_data, sizeof(T));
        }

        ::std::stringstream& stream()
        {
            return m_stream;
        }

        const ::std::stringstream& stream() const
        {
            return m_stream;
        }

    }; // END of class OStream.

} // END of namespace profiler.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__OUTPUT_STREAM__H_
