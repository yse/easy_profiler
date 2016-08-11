/************************************************************************
* file name         : common_types.h
* ----------------- :
* creation time     : 2016/07/31
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of common types for both GraphicsView
*                   : and TreeWidget.
* ----------------- :
* change log        : * 2016/07/31 Victor Zarubkin: initial commit.
*                   :
*                   : *
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#ifndef EASY_PROFILER__GUI_COMMON_TYPES_H
#define EASY_PROFILER__GUI_COMMON_TYPES_H

#include <stdlib.h>
#include <vector>
#include <unordered_map>
#include <QRgb>
#include <QString>
#include "profiler/reader.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#define PROF_MICROSECONDS(timestamp) ((timestamp) * 1e-3)
//#define PROF_MICROSECONDS(timestamp) (timestamp)

#define PROF_FROM_MICROSECONDS(to_timestamp) ((to_timestamp) * 1e3)
//#define PROF_FROM_MICROSECONDS(to_timestamp) (to_timestamp)

#define PROF_MILLISECONDS(timestamp) ((timestamp) * 1e-6)
//#define PROF_MILLISECONDS(timestamp) ((timestamp) * 1e-3)

#define PROF_FROM_MILLISECONDS(to_timestamp) ((to_timestamp) * 1e6)
//#define PROF_FROM_MILLISECONDS(to_timestamp) ((to_timestamp) * 1e3)

#define PROF_NANOSECONDS(timestamp) (timestamp)
//#define PROF_NANOSECONDS(timestamp) ((timestamp) * 1000)

//////////////////////////////////////////////////////////////////////////

namespace profiler_gui {

template <const size_t SIZEOF_T>
struct no_hasher {
    template <class T> inline size_t operator () (const T& _data) const {
        return (size_t)_data;
    }
};

#ifdef _WIN64
template <> struct no_hasher<8> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};
#endif

template <> struct no_hasher<4> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};

template <> struct no_hasher<2> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};

template <> struct no_hasher<1> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};

template <class T>
struct do_no_hash {
    typedef no_hasher<sizeof(T)> hasher_t;
};

//////////////////////////////////////////////////////////////////////////

const QRgb DEFAULT_COLOR = 0x00d4b494;//0x00f0e094;

inline QRgb toRgb(unsigned int _red, unsigned int _green, unsigned int _blue)
{
    return (_red << 16) + (_green << 8) + _blue;
}

inline QRgb fromProfilerRgb(unsigned int _red, unsigned int _green, unsigned int _blue)
{
    if (_red == 0 && _green == 0 && _blue == 0)
        return DEFAULT_COLOR;
    return toRgb(_red, _green, _blue) | 0x00141414;
}

//////////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)
struct ProfBlockItem final
{
    const ::profiler::BlocksTree* block; ///< Pointer to profiler block
    qreal                             x; ///< x coordinate of the item (this is made qreal=double to avoid mistakes on very wide scene)
    float                             w; ///< Width of the item
    QRgb                          color; ///< Background color of the item
    unsigned int         children_begin; ///< Index of first child item on the next sublevel
    unsigned short          totalHeight; ///< Total height of the item including heights of all it's children
    char                          state; ///< 0 = no change, 1 = paint, -1 = do not paint

    // Possible optimizations:
    // 1) We can save 1 more byte per block if we will use char instead of short + real time calculations for "totalHeight" var;
    // 2) We can save 12 bytes per block if "x" and "w" vars will be removed (all this information exist inside BlocksTree),
    //      but this will make impossible to run graphics test without loading any .prof file.

    inline void setPos(qreal _x, float _w) { x = _x; w = _w; }
    inline qreal left() const { return x; }
    inline qreal right() const { return x + w; }
    inline float width() const { return w; }

}; // END of struct ProfBlockItem.
#pragma pack(pop)

typedef ::std::vector<ProfBlockItem> ProfItems;

//////////////////////////////////////////////////////////////////////////

struct ProfSelectedBlock final
{
    const ::profiler::BlocksTreeRoot* root;
    const ::profiler::BlocksTree*     tree;

    ProfSelectedBlock() : root(nullptr), tree(nullptr)
    {
    }

    ProfSelectedBlock(const ::profiler::BlocksTreeRoot* _root, const ::profiler::BlocksTree* _tree)
        : root(_root)
        , tree(_tree)
    {
    }

}; // END of struct ProfSelectedBlock.

typedef ::std::vector<ProfSelectedBlock> TreeBlocks;

//////////////////////////////////////////////////////////////////////////

inline QString timeStringReal(qreal _interval, int _precision = 1)
{
    if (_interval < 1) // interval in nanoseconds
        return QString("%1 ns").arg(_interval * 1e3, 0, 'f', _precision);

    if (_interval < 1e3) // interval in microseconds
        return QString("%1 us").arg(_interval, 0, 'f', _precision);

    if (_interval < 1e6) // interval in milliseconds
        return QString("%1 ms").arg(_interval * 1e-3, 0, 'f', _precision);

    // interval in seconds
    return QString("%1 sec").arg(_interval * 1e-6, 0, 'f', _precision);
}

inline QString timeStringInt(qreal _interval)
{
    if (_interval < 1) // interval in nanoseconds
        return QString("%1 ns").arg(static_cast<int>(_interval * 1e3));

    if (_interval < 1e3) // interval in microseconds
        return QString("%1 us").arg(static_cast<int>(_interval));

    if (_interval < 1e6) // interval in milliseconds
        return QString("%1 ms").arg(static_cast<int>(_interval * 1e-3));

    // interval in seconds
    return QString("%1 sec").arg(static_cast<int>(_interval * 1e-6));
}

//////////////////////////////////////////////////////////////////////////

template <class T> inline T numeric_max() {
    return ::std::numeric_limits<T>::max();
}

template <class T> inline T numeric_max(T) {
    return ::std::numeric_limits<T>::max();
}

template <class T> inline void set_max(T& _value) {
    _value = ::std::numeric_limits<T>::max();
}

//////////////////////////////////////////////////////////////////////////

} // END of namespace profiler_gui.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__GUI_COMMON_TYPES_H
