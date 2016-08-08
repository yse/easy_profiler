/************************************************************************
* file name         : globals.cpp
* ----------------- :
* creation time     : 2016/08/03
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of global constants and variables for profiler gui.
* ----------------- :
* change log        : * 2016/08/03 Victor Zarubkin: initial commit.
*                   :
*                   : *
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#include "globals.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace profiler_gui {

    ProfGlobalSignals::ProfGlobalSignals() : QObject()
    {
    }

    ProfGlobalSignals::~ProfGlobalSignals()
    {
    }

    ProfGlobals& ProfGlobals::instance()
    {
        static ProfGlobals globals;
        return globals;
    }

    ProfGlobals::ProfGlobals()
        : selected_thread(0)
        , selected_block(-1)
        , draw_graphics_items_borders(true)
    {

    }

} // END of namespace profiler_gui.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

