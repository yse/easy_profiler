#ifndef GLOBALS_QOBJECTS_H
#define GLOBALS_QOBJECTS_H

#include <QObject>
#include "profiler/profiler.h"

namespace profiler_gui {
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

}

#endif // GLOBALS_QOBJECTS_H
