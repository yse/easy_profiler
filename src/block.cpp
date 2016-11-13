/************************************************************************
* file name         : block.cpp
* ----------------- :
* creation time     : 2016/02/16
* authors           : Sergey Yagovtsev, Victor Zarubkin
* emails            : yse.sey@gmail.com, v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of profiling blocks
*                   :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   :
*                   : Licensed under the Apache License, Version 2.0 (the "License");
*                   : you may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
*                   :
*                   :
*                   : GNU General Public License Usage
*                   : Alternatively, this file may be used under the terms of the GNU
*                   : General Public License as published by the Free Software Foundation,
*                   : either version 3 of the License, or (at your option) any later version.
*                   :
*                   : This program is distributed in the hope that it will be useful,
*                   : but WITHOUT ANY WARRANTY; without even the implied warranty of
*                   : MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*                   : GNU General Public License for more details.
*                   :
*                   : You should have received a copy of the GNU General Public License
*                   : along with this program.If not, see <http://www.gnu.org/licenses/>.
************************************************************************/

#include "profile_manager.h"
#include "current_time.h"

using namespace profiler;

BaseBlockData::BaseBlockData(timestamp_t _begin_time, block_id_t _descriptor_id)
    : m_begin(_begin_time)
    , m_end(0)
    , m_id(_descriptor_id)
{

}

Block::Block(Block&& that)
    : BaseBlockData(that.m_begin, that.m_id)
    , m_name(that.m_name)
    , m_status(that.m_status)
{
    m_end = that.m_end;
}

Block::Block(const BaseBlockDescriptor* _descriptor, const char* _runtimeName)
    : BaseBlockData(1ULL, _descriptor->id())
    , m_name(_runtimeName)
    , m_status(_descriptor->status())
{

}

Block::Block(timestamp_t _begin_time, block_id_t _descriptor_id, const char* _runtimeName)
    : BaseBlockData(_begin_time, _descriptor_id)
    , m_name(_runtimeName)
    , m_status(::profiler::ON)
{

}

void Block::start()
{
    m_begin = getCurrentTime();
}

void Block::start(timestamp_t _time)
{
    m_begin = _time;
}

void Block::finish()
{
    m_end = getCurrentTime();
}

void Block::finish(timestamp_t _time)
{
    m_end = _time;
}

Block::~Block()
{
    if (!finished())
        ::profiler::endBlock();
}
