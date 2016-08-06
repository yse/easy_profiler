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
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ProfGraphicsItem;
class ProfTreeWidgetItem;

//////////////////////////////////////////////////////////////////////////

namespace profiler_gui {

	const QString ORGANAZATION_NAME = "EasyProfiler";
	const QString APPLICATION_NAME = "Easy profiler gui application";

    const QColor CHRONOMETER_COLOR = QColor::fromRgba(0x402020c0);
    const QRgb SELECTED_THREAD_BACKGROUND = 0x00d8d840;
    const QRgb SELECTED_THREAD_FOREGROUND = 0x00ffffff - SELECTED_THREAD_BACKGROUND;

    //////////////////////////////////////////////////////////////////////////

    class ProfGlobalSignals final : public QObject
    {
        Q_OBJECT

    public:

        ProfGlobalSignals();
        virtual ~ProfGlobalSignals();

    signals:

        void selectedThreadChanged(::profiler::thread_id_t _id);
        void selectedBlockChanged(unsigned int _block_index);
    };

    //////////////////////////////////////////////////////////////////////////

    struct ProfBlock final
    {
        ProfGraphicsItem*         graphics_item;
        ProfTreeWidgetItem*           tree_item;
        unsigned short      graphics_item_level;
        unsigned int        graphics_item_index;
    };

    typedef ::std::vector<ProfBlock> ProfBlocks;

    //////////////////////////////////////////////////////////////////////////

    struct ProfGlobals final
    {
        static ProfGlobals& instance();

        ProfGlobalSignals                         events;
        ::profiler::thread_blocks_tree_t profiler_blocks;
        ProfBlocks                            gui_blocks;
        ::profiler::thread_id_t          selected_thread;
        unsigned int                      selected_block;
        bool                 draw_graphics_items_borders;

    private:

        ProfGlobals();
    };

    static ProfGlobals& EASY_GLOBALS = ProfGlobals::instance();

    //////////////////////////////////////////////////////////////////////////

} // END of namespace profiler_gui.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__GUI_GLOBALS_H
