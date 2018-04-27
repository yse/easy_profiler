/************************************************************************
* file name         : globals.h
* ----------------- :
* creation time     : 2016/08/03
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of global constants and variables for profiler gui.
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

#ifndef EASY_PROFILER__GUI_GLOBALS_H
#define EASY_PROFILER__GUI_GLOBALS_H

#include <string>
#include <QObject>
#include <QColor>
#include <QTextCodec>
#include <QSize>
#include "common_types.h"
#include "globals_qobjects.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace profiler_gui {

    const QString ORGANAZATION_NAME = "EasyProfiler";
    const QString APPLICATION_NAME = "Easy profiler gui application";

    const QColor CHRONOMETER_COLOR = QColor::fromRgba(0x40000000 | (::profiler::colors::RichBlue & 0x00ffffff));// 0x402020c0);
    const QColor CHRONOMETER_COLOR2 = QColor::fromRgba(0x40000000 | (::profiler::colors::Dark & 0x00ffffff));// 0x40408040);
    const QRgb SELECTED_THREAD_BACKGROUND = 0x00e0e060;
    const QRgb SELECTED_THREAD_FOREGROUND = 0x00ffffff - SELECTED_THREAD_BACKGROUND;

    const qreal SCALING_COEFFICIENT = 1.25;
    const qreal SCALING_COEFFICIENT_INV = 1.0 / SCALING_COEFFICIENT;

    const QSize ICONS_SIZE(28, 28);
    const uint16_t GRAPHICS_ROW_SIZE = 18;
    const uint16_t GRAPHICS_ROW_SPACING = 2;
    const uint16_t GRAPHICS_ROW_SIZE_FULL = GRAPHICS_ROW_SIZE + GRAPHICS_ROW_SPACING;
    const uint16_t THREADS_ROW_SPACING = 8;

#ifdef _WIN32
    const qreal FONT_METRICS_FACTOR = 1.05;
#else
    const qreal FONT_METRICS_FACTOR = 1.;
#endif

    //////////////////////////////////////////////////////////////////////////

    template <class T>
    inline auto toUnicode(const T& _inputString) -> decltype(QTextCodec::codecForLocale()->toUnicode(_inputString))
    {
        return QTextCodec::codecForLocale()->toUnicode(_inputString);
    }

    //////////////////////////////////////////////////////////////////////////

    inline QString decoratedThreadName(bool _use_decorated_thread_name, const::profiler::BlocksTreeRoot& _root, const QString& _unicodeThreadWord)
    {
        if (_root.got_name())
        {
            QString rootname(toUnicode(_root.name()));
            if (!_use_decorated_thread_name || rootname.contains(_unicodeThreadWord, Qt::CaseInsensitive))
                return QString("%1 %2").arg(rootname).arg(_root.thread_id);
            return QString("%1 Thread %2").arg(rootname).arg(_root.thread_id);
        }

        return QString("Thread %1").arg(_root.thread_id);
    }

    inline QString decoratedThreadName(bool _use_decorated_thread_name, const ::profiler::BlocksTreeRoot& _root)
    {
        if (_root.got_name())
        {
            QString rootname(toUnicode(_root.name()));
            if (!_use_decorated_thread_name || rootname.contains(toUnicode("thread"), Qt::CaseInsensitive))
                return QString("%1 %2").arg(rootname).arg(_root.thread_id);
            return QString("%1 Thread %2").arg(rootname).arg(_root.thread_id);
        }

        return QString("Thread %1").arg(_root.thread_id);
    }

    //////////////////////////////////////////////////////////////////////////

    enum ChronometerTextPosition : int8_t
    {
        ChronoTextPosition_Center = 0,
        ChronoTextPosition_Top,
        ChronoTextPosition_Bottom,

    }; // END of enum ChronometerTextPosition.

    //////////////////////////////////////////////////////////////////////////

    struct EasyGlobals Q_DECL_FINAL
    {
        static EasyGlobals& instance();

        EasyGlobalSignals                         events; ///< Global signals
        ::profiler::thread_blocks_tree_t profiler_blocks; ///< Profiler blocks tree loaded from file
        ::profiler::descriptors_list_t       descriptors; ///< Profiler block descriptors list
        EasyBlocks                            gui_blocks; ///< Profiler graphics blocks builded by GUI
        ::profiler::timestamp_t               begin_time; ///< 
        ::profiler::thread_id_t          selected_thread; ///< Current selected thread id
        ::profiler::block_index_t         selected_block; ///< Current selected profiler block index
        ::profiler::block_id_t         selected_block_id; ///< Current selected profiler block id
        float                                 frame_time; ///< Value in microseconds to be displayed at minimap on graphics scrollbar
        int                               blocks_spacing; ///< Minimum blocks spacing on diagram
        int                              blocks_size_min; ///< Minimum blocks size on diagram
        int                           blocks_narrow_size; ///< Width indicating narrow blocks
        ChronometerTextPosition     chrono_text_position; ///< Selected interval text position
        TimeUnits                             time_units; ///< Units type for time (milliseconds, microseconds, nanoseconds or auto-definition)
        bool                                   connected; ///< Is connected to source (to be able to capture profiling information)
        bool                   use_decorated_thread_name; ///< Add "Thread" to the name of each thread (if there is no one)
        bool                     enable_event_indicators; ///< Enable event indicators painting (These are narrow rectangles at the bottom of each thread)
        bool                           enable_statistics; ///< Enable gathering and using statistics (Disable if you want to consume less memory)
        bool                          enable_zero_length; ///< Enable zero length blocks (if true, then such blocks will have width == 1 pixel on each scale)
        bool                add_zero_blocks_to_hierarchy; ///< Enable adding zero blocks into hierarchy tree
        bool                 draw_graphics_items_borders; ///< Draw borders for graphics blocks or not
        bool                        hide_narrow_children; ///< Hide children for narrow graphics blocks (See blocks_narrow_size)
        bool                         hide_minsize_blocks; ///< Hide blocks which screen size is less than blocks_size_min
        bool                 display_only_relevant_stats; ///< Display only relevant information in ProfTreeWidget (excludes min, max, average times if there are only 1 calls number)
        bool                collapse_items_on_tree_close; ///< Collapse all items which were displayed in the hierarchy tree after tree close/reset
        bool               all_items_expanded_by_default; ///< Expand all items after file is opened
        bool               only_current_thread_hierarchy; ///< Build hierarchy tree for current thread only
        bool               highlight_blocks_with_same_id; ///< Highlight all blocks with same id on diagram
        bool              selecting_block_changes_thread; ///< If true then current selected thread will change every time you select block
        bool           bind_scene_and_tree_expand_status; /** \brief If true then items on graphics scene and in the tree (blocks hierarchy) are binded on each other
                                                                so expanding/collapsing items on scene also expands/collapse items in the tree. */

    private:

        EasyGlobals();

    }; // END of struct EasyGlobals.

    //////////////////////////////////////////////////////////////////////////

} // END of namespace profiler_gui.

#ifndef IGNORE_GLOBALS_DECLARATION
static ::profiler_gui::EasyGlobals& EASY_GLOBALS = ::profiler_gui::EasyGlobals::instance();

inline ::profiler_gui::EasyBlock& easyBlock(::profiler::block_index_t i) {
    return EASY_GLOBALS.gui_blocks[i];
}

inline ::profiler::SerializedBlockDescriptor& easyDescriptor(::profiler::block_id_t i) {
    return *EASY_GLOBALS.descriptors[i];
}

inline ::profiler::BlocksTree& blocksTree(::profiler::block_index_t i) {
    return easyBlock(i).tree;
}
#endif

#define SET_ICON(objectName, iconName) { QIcon icon(iconName); if (!icon.isNull()) objectName->setIcon(icon); }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__GUI_GLOBALS_H
