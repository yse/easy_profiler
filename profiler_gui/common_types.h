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
#include <QRgb>
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

const QRgb DEFAULT_COLOR = 0x00f0e094;

inline QRgb toRgb(unsigned int _red, unsigned int _green, unsigned int _blue)
{
    if (_red == 0 && _green == 0 && _blue == 0)
        return DEFAULT_COLOR;
    return (_red << 16) + (_green << 8) + _blue;
}

//////////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)
struct ProfBlockItem
{
    const BlocksTree*       block; ///< Pointer to profiler block
    qreal                       x; ///< x coordinate of the item (this is made qreal=double to avoid mistakes on very wide scene)
    float                       w; ///< Width of the item
    float                       y; ///< y coordinate of the item
    float                       h; ///< Height of the item
    QRgb                    color; ///< Background color of the item
    unsigned int   children_begin; ///< Index of first child item on the next sublevel
    unsigned short    totalHeight; ///< Total height of the item including heights of all it's children
    char                    state; ///< 0 = no change, 1 = paint, -1 = do not paint

    inline void setRect(qreal _x, float _y, float _w, float _h) {
        x = _x;
        y = _y;
        w = _w;
        h = _h;
    }

    inline qreal left() const { return x; }
    inline float top() const { return y; }
    inline float width() const { return w; }
    inline float height() const { return h; }
    inline qreal right() const { return x + w; }
    inline float bottom() const { return y + h; }
};
#pragma pack(pop)

typedef ::std::vector<ProfBlockItem> ProfItems;

//////////////////////////////////////////////////////////////////////////

struct ProfBlock
{
    const BlocksTree*     thread_tree;
    const BlocksTree*            tree;
    ::profiler::thread_id_t thread_id;

    ProfBlock() : thread_tree(nullptr), tree(nullptr), thread_id(0)
    {
    }

    ProfBlock(::profiler::thread_id_t _id, const BlocksTree* _thread_tree, const BlocksTree* _tree) : thread_tree(_thread_tree), tree(_tree), thread_id(_id)
    {
    }
};

typedef ::std::vector<ProfBlock> TreeBlocks;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__GUI_COMMON_TYPES_H
