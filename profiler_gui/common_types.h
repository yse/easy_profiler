/************************************************************************
* file name         : common_types.h
* ----------------- :
* creation time     : 2016/07/31
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

inline QRgb toRgb(uint32_t _red, uint32_t _green, uint32_t _blue)
{
    return (_red << 16) + (_green << 8) + _blue;
}

inline QRgb fromProfilerRgb(uint32_t _red, uint32_t _green, uint32_t _blue)
{
    if (_red == 0 && _green == 0 && _blue == 0)
        return ::profiler::colors::Default;
    return toRgb(_red, _green, _blue) | 0x00141414;
}

inline bool isLightColor(::profiler::color_t _color)
{
    const auto sum = 255. - (((_color & 0x00ff0000) >> 16) * 0.299 + ((_color & 0x0000ff00) >> 8) * 0.587 + (_color & 0x000000ff) * 0.114);
    return sum < 76.5 || ((_color & 0xff000000) >> 24) < 0x80;
}

inline bool isLightColor(::profiler::color_t _color, qreal _maxSum)
{
    const auto sum = 255. - (((_color & 0x00ff0000) >> 16) * 0.299 + ((_color & 0x0000ff00) >> 8) * 0.587 + (_color & 0x000000ff) * 0.114);
    return sum < _maxSum || ((_color & 0xff000000) >> 24) < 0x80;
}

inline ::profiler::color_t textColorForRgb(::profiler::color_t _color)
{
    return isLightColor(_color) ? ::profiler::colors::Dark : ::profiler::colors::CreamWhite;
}

//////////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)
struct EasyBlockItem final
{
    //const ::profiler::BlocksTree* block; ///< Pointer to profiler block
    qreal                             x; ///< x coordinate of the item (this is made qreal=double to avoid mistakes on very wide scene)
    float                             w; ///< Width of the item
    QRgb                          color; ///< Background color of the item
    ::profiler::block_index_t     block; ///< Index of profiler block
    uint32_t             children_begin; ///< Index of first child item on the next sublevel
    uint16_t                totalHeight; ///< Total height of the item including heights of all it's children
    char                          state; ///< 0 = no change, 1 = paint, -1 = do not paint

    // Possible optimizations:
    // 1) We can save 1 more byte per block if we will use char instead of short + real time calculations for "totalHeight" var;
    // 2) We can save 12 bytes per block if "x" and "w" vars will be removed (all this information exist inside BlocksTree),
    //      but this will make impossible to run graphics test without loading any .prof file.

    inline void setPos(qreal _x, float _w) { x = _x; w = _w; }
    inline qreal left() const { return x; }
    inline qreal right() const { return x + w; }
    inline float width() const { return w; }

}; // END of struct EasyBlockItem.

struct EasyBlock final
{
    ::profiler::BlocksTree       tree;
    uint32_t                tree_item;
    uint32_t      graphics_item_index;
    uint8_t       graphics_item_level;
    uint8_t             graphics_item;
    bool                     expanded;

    EasyBlock() = default;

    EasyBlock(EasyBlock&& that)
        : tree(::std::move(that.tree))
        , tree_item(that.tree_item)
        , graphics_item_index(that.graphics_item_index)
        , graphics_item_level(that.graphics_item_level)
        , graphics_item(that.graphics_item)
        , expanded(that.expanded)
    {
    }

private:

    EasyBlock(const EasyBlock&) = delete;
};
#pragma pack(pop)

typedef ::std::vector<EasyBlockItem> EasyItems;
typedef ::std::vector<EasyBlock> EasyBlocks;

//////////////////////////////////////////////////////////////////////////

struct EasySelectedBlock final
{
    const ::profiler::BlocksTreeRoot* root;
    ::profiler::block_index_t         tree;

    EasySelectedBlock() : root(nullptr), tree(0xffffffff)
    {
    }

    EasySelectedBlock(const ::profiler::BlocksTreeRoot* _root, const ::profiler::block_index_t _tree)
        : root(_root)
        , tree(_tree)
    {
    }

}; // END of struct EasySelectedBlock.

typedef ::std::vector<EasySelectedBlock> TreeBlocks;

//////////////////////////////////////////////////////////////////////////

inline qreal timeFactor(qreal _interval)
{
    if (_interval < 1) // interval in nanoseconds
        return 1e3;

    if (_interval < 1e3) // interval in microseconds
        return 1;

    if (_interval < 1e6) // interval in milliseconds
        return 1e-3;

    // interval in seconds
    return 1e-6;
}

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
