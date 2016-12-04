/************************************************************************
* file name         : easy_qtimer.h
* ----------------- : 
* creation time     : 2016/12/05
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- : 
* description       : This file contains declaration of EasyQTimer class used to
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

#ifndef EASY__QTIMER__H
#define EASY__QTIMER__H

#include <QTimer>
#include <functional>

//////////////////////////////////////////////////////////////////////////

class EasyQTimer : public QObject
{
    Q_OBJECT

private:

    QTimer                    m_timer;
    ::std::function<void()> m_handler;

public:

    EasyQTimer();
    EasyQTimer(::std::function<void()>&& _handler);
    virtual ~EasyQTimer();

    void setHandler(::std::function<void()>&& _handler);

    inline void start(int msec) { m_timer.start(msec); }
    inline void stop() { m_timer.stop(); }
    inline bool isActive() const { return m_timer.isActive(); }

}; // END of class EasyQTimer.

//////////////////////////////////////////////////////////////////////////

#endif // EASY__QTIMER__H
