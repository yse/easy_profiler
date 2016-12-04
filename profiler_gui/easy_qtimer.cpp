/************************************************************************
* file name         : easy_qtimer.h
* ----------------- :
* creation time     : 2016/12/05
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : This file contains implementation of EasyQTimer class used to
*                   : connect QTimer to non-QObject classes.
* ----------------- :
* change log        : * 2016/12/05 Victor Zarubkin: Initial commit.
*                   :
*                   : *
* ----------------- :
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

#include "easy_qtimer.h"

//////////////////////////////////////////////////////////////////////////

EasyQTimer::EasyQTimer()
    : QObject()
{
    connect(&m_timer, &QTimer::timeout, [this](){ m_handler(); });
}

EasyQTimer::EasyQTimer(::std::function<void()>&& _handler)
    : QObject()
    , m_handler(::std::forward<::std::function<void()>&&>(_handler))
{
    connect(&m_timer, &QTimer::timeout, [this](){ m_handler(); });
}

EasyQTimer::~EasyQTimer()
{

}

void EasyQTimer::setHandler(::std::function<void()>&& _handler)
{
    m_handler = _handler;
}

//////////////////////////////////////////////////////////////////////////

