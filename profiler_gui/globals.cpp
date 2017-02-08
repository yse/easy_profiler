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
        : selected_thread(0U)
        , selected_block(::profiler_gui::numeric_max<decltype(selected_block)>())
        , selected_block_id(::profiler_gui::numeric_max<decltype(selected_block_id)>())
        , begin_time(0)
        , frame_time(4e4f)
        , blocks_spacing(2)
        , blocks_size_min(2)
        , blocks_narrow_size(20)
        , chrono_text_position(ChronoTextPosition_Center)
        , time_units(TimeUnits_auto)
        , connected(false)
        , use_decorated_thread_name(true)
        , enable_event_indicators(true)
        , enable_statistics(true)
        , enable_zero_length(true)
        , add_zero_blocks_to_hierarchy(false)
        , draw_graphics_items_borders(true)
        , hide_narrow_children(false)
        , hide_minsize_blocks(false)
        , display_only_relevant_stats(true)
        , collapse_items_on_tree_close(false)
        , all_items_expanded_by_default(true)
        , only_current_thread_hierarchy(false)
        , highlight_blocks_with_same_id(true)
        , selecting_block_changes_thread(true)
        , bind_scene_and_tree_expand_status(true)
    {

    }

} // END of namespace profiler_gui.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

