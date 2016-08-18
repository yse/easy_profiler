#ifndef GLOBALS_QOBJECTS_H
#define GLOBALS_QOBJECTS_H

#include <QObject>
#include "profiler/profiler.h"

namespace profiler_gui {

    class EasyGlobalSignals final : public QObject
    {
        Q_OBJECT

    public:

        EasyGlobalSignals();
        virtual ~EasyGlobalSignals();

    signals:

        void selectedThreadChanged(::profiler::thread_id_t _id);
        void selectedBlockChanged(unsigned int _block_index);
        void itemsExpandStateChanged();

    }; // END of class EasyGlobalSignals.

} // END of namespace profiler_gui.

#endif // GLOBALS_QOBJECTS_H
