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

#include "globals_qobjects.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace profiler_gui {

    ProfGlobalSignals::ProfGlobalSignals() : QObject()
    {
    }

    ProfGlobalSignals::~ProfGlobalSignals()
    {
    }

} // END of namespace profiler_gui.

//////////////////////////////////////////////////////////////////////////
