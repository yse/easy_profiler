#define IGNORE_GLOBALS_DECLARATION
#include "globals.h"
#undef IGNORE_GLOBALS_DECLARATION

namespace profiler_gui {

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
