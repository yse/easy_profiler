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

    enum ChronometerTextPosition
    {
        ChronoTextPosition_Center = 0,
        ChronoTextPosition_Top,
        ChronoTextPosition_Bottom,

    }; // END of enum ChronometerTextPosition.

    //////////////////////////////////////////////////////////////////////////

    struct EasyGlobals final
    {
        static EasyGlobals& instance();

        EasyGlobalSignals                         events; ///< Global signals
        ::profiler::thread_blocks_tree_t profiler_blocks; ///< Profiler blocks tree loaded from file
        ::profiler::descriptors_list_t       descriptors; ///< Profiler block descriptors list
        EasyBlocks                            gui_blocks; ///< Profiler graphics blocks builded by GUI
        ::profiler::thread_id_t          selected_thread; ///< Current selected thread id
        unsigned int                      selected_block; ///< Current selected profiler block index
        ChronometerTextPosition     chrono_text_position; ///< 
        bool                           enable_statistics; ///< Enable gathering and using statistics (Disable if you want to consume less memory)
        bool                 draw_graphics_items_borders; ///< Draw borders for graphics blocks or not
        bool                 display_only_relevant_stats; ///< Display only relevant information in ProfTreeWidget (excludes min, max, average times if there are only 1 calls number)
        bool                collapse_items_on_tree_close; ///< Collapse all items which were displayed in the hierarchy tree after tree close/reset
        bool               all_items_expanded_by_default; ///< Expand all items after file is opened
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
