/************************************************************************
* file name         : globals_qobjects.h
* ----------------- :
* creation time     : 2016/08/08
* authors           : Victor Zarubkin, Sergey Yagovtsev
* email             : v.s.zarubkin@gmail.com, yse.sey@gmail.com
* ----------------- :
* description       : The file contains declaration of EasyGlobalSignals QObject class.
* ----------------- :
* change log        : * 2016/08/08 Sergey Yagovtsev: moved sources from globals.h
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

#ifndef EASY__GLOBALS_QOBJECTS_H___
#define EASY__GLOBALS_QOBJECTS_H___

#include <QObject>
#include "easy/profiler.h"

namespace profiler_gui {

    class EasyGlobalSignals Q_DECL_FINAL : public QObject
    {
        Q_OBJECT

    public:

        EasyGlobalSignals();
        virtual ~EasyGlobalSignals();

    signals:

        void selectedThreadChanged(::profiler::thread_id_t _id);
        void selectedBlockChanged(uint32_t _block_index);
        void selectedBlockIdChanged(::profiler::block_id_t _id);
        void itemsExpandStateChanged();
        void blockStatusChanged(::profiler::block_id_t _id, ::profiler::EasyBlockStatus _status);
        void connectionChanged(bool _connected);
        void blocksRefreshRequired(bool);
        void timelineMarkerChanged();
        void hierarchyFlagChanged(bool);
        void threadNameDecorationChanged();
        void refreshRequired();
        void blocksTreeModeChanged();

    }; // END of class EasyGlobalSignals.

} // END of namespace profiler_gui.

#endif // EASY__GLOBALS_QOBJECTS_H___
