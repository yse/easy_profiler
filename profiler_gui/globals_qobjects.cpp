/************************************************************************
* file name         : globals_qobjects.cpp
* ----------------- :
* creation time     : 2016/08/08
* authors           : Victor Zarubkin, Sergey Yagovtsev
* email             : v.s.zarubkin@gmail.com, yse.sey@gmail.com
* ----------------- :
* description       : The file contains implementation of EasyGlobalSignals QObject class.
* ----------------- :
* change log        : * 2016/08/08 Sergey Yagovtsev: moved sources from globals.cpp
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

#include "globals_qobjects.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace profiler_gui {

    EasyGlobalSignals::EasyGlobalSignals() : QObject()
    {
    }

    EasyGlobalSignals::~EasyGlobalSignals()
    {
    }

} // END of namespace profiler_gui.

//////////////////////////////////////////////////////////////////////////
