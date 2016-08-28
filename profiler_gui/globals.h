/************************************************************************
* file name         : globals.h
* ----------------- :
* creation time     : 2016/08/03
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of global constants and variables for profiler gui.
* ----------------- :
* change log        : * 2016/08/03 Victor Zarubkin: initial commit.
*                   :
*                   : *
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#ifndef EASY_PROFILER__GUI_GLOBALS_H
#define EASY_PROFILER__GUI_GLOBALS_H

#include <string>
#include <QObject>
#include <QColor>
#include <QTextCodec>
#include "common_types.h"
#include "globals_qobjects.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace profiler_gui {

	const QString ORGANAZATION_NAME = "EasyProfiler";
	const QString APPLICATION_NAME = "Easy profiler gui application";

    const QColor CHRONOMETER_COLOR = QColor::fromRgba(0x402020c0);
    const QRgb SELECTED_THREAD_BACKGROUND = 0x00e0e060;
    const QRgb SELECTED_THREAD_FOREGROUND = 0x00ffffff - SELECTED_THREAD_BACKGROUND;

    const qreal SCALING_COEFFICIENT = 1.25;
    const qreal SCALING_COEFFICIENT_INV = 1.0 / SCALING_COEFFICIENT;

    //////////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)
    struct EasyBlock final
    {
        unsigned int            tree_item;
        unsigned int  graphics_item_index;
        unsigned char graphics_item_level;
        unsigned char       graphics_item;
        bool                     expanded;
    };
#pragma pack(pop)

    typedef ::std::vector<EasyBlock> EasyBlocks;

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
        bool                 draw_graphics_items_borders; ///< Draw borders for graphics blocks or not
        bool                 display_only_relevant_stats; ///< Display only relevant information in ProfTreeWidget (excludes min, max, average times if there are only 1 calls number)
        bool                collapse_items_on_tree_close; ///< Collapse all items which were displayed in the hierarchy tree after tree close/reset
        bool               all_items_expanded_by_default; ///< Expand all items after file is opened
        bool           bind_scene_and_tree_expand_status; /** \brief If true then items on graphics scene and in the tree (blocks hierarchy) are binded on each other
                                                                so expanding/collapsing items on scene also expands/collapse items in the tree. */

    private:

        EasyGlobals();

    }; // END of struct EasyGlobals.

#ifndef IGNORE_GLOBALS_DECLARATION
    static EasyGlobals& EASY_GLOBALS = EasyGlobals::instance();
#endif
    //////////////////////////////////////////////////////////////////////////

} // END of namespace profiler_gui.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__GUI_GLOBALS_H
