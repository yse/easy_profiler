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
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

namespace profiler_gui {

    class ProfGlobalSignals : public QObject
    {
        Q_OBJECT

    public:

        ProfGlobalSignals();
        virtual ~ProfGlobalSignals();

    signals:

        void selectedThreadChanged(::profiler::thread_id_t _id);
    };

    struct ProfGlobals
    {
        static ProfGlobals& instance();

        ProfGlobalSignals                         events;
        ::profiler::thread_blocks_tree_t profiler_blocks;
        ::profiler::thread_id_t              selected_thread;

    private:

        ProfGlobals();
    };

    static ProfGlobals& EASY_GLOBALS = ProfGlobals::instance();

} // END of namespace profiler_gui.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__GUI_GLOBALS_H
