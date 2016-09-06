/************************************************************************
* file name         : globals.cpp
* ----------------- :
* creation time     : 2016/08/03
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of global constants and variables for profiler gui.
* ----------------- :
* change log        : * 2016/08/03 Victor Zarubkin: initial commit.
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

#define IGNORE_GLOBALS_DECLARATION
#include "globals.h"
#undef IGNORE_GLOBALS_DECLARATION

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace profiler_gui {

    EasyGlobals& EasyGlobals::instance()
    {
        static EasyGlobals globals;
        return globals;
    }

    EasyGlobals::EasyGlobals()
        : selected_thread(0)
        , selected_block(::profiler_gui::numeric_max<decltype(selected_block)>())
        , chrono_text_position(ChronoTextPosition_Center)
        , draw_graphics_items_borders(true)
        , display_only_relevant_stats(true)
        , collapse_items_on_tree_close(false)
        , all_items_expanded_by_default(true)
        , bind_scene_and_tree_expand_status(true)
    {

    }

} // END of namespace profiler_gui.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

