/************************************************************************
* file name         : blocks_graphics_view.cpp
* ----------------- :
* creation time     : 2016/06/26
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of GraphicsScene and GraphicsView and
*                   : it's auxiliary classes for displyaing easy_profiler blocks tree.
* ----------------- :
* change log        : * 2016/06/26 Victor Zarubkin: Moved sources from graphics_view.h
*                   :       and renamed classes from My* to Prof*.
*                   :
*                   : * 2016/06/27 Victor Zarubkin: Added text shifting relatively to it's parent item.
*                   :       Disabled border lines painting because of vertical lines painting bug.
*                   :       Changed height of blocks. Variable thread-block height.
*                   :
*                   : * 2016/06/29 Victor Zarubkin: Highly optimized painting performance and memory consumption.
*                   :
*                   : * 2016/06/30 Victor Zarubkin: Replaced doubles with floats (in ProfBlockItem) for less memory consumption.
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

#include <QGraphicsScene>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QGridLayout>
#include <QFont>
#include <QFontMetrics>
#include <QDebug>
#include <QSignalBlocker>
#include <math.h>
#include <algorithm>
#include "blocks_graphics_view.h"

#include "globals.h"

using namespace profiler_gui;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum BlockItemState
{
    BLOCK_ITEM_DO_NOT_PAINT = -1,
    BLOCK_ITEM_UNCHANGED,
    BLOCK_ITEM_DO_PAINT
};

//////////////////////////////////////////////////////////////////////////

const qreal MIN_SCALE = pow(::profiler_gui::SCALING_COEFFICIENT_INV, 70);
const qreal MAX_SCALE = pow(::profiler_gui::SCALING_COEFFICIENT, 30); // ~800
const qreal BASE_SCALE = pow(::profiler_gui::SCALING_COEFFICIENT_INV, 25); // ~0.003

const unsigned short GRAPHICS_ROW_SIZE = 18;
const unsigned short GRAPHICS_ROW_SPACING = 2;
const unsigned short GRAPHICS_ROW_SIZE_FULL = GRAPHICS_ROW_SIZE + GRAPHICS_ROW_SPACING;
const unsigned short THREADS_ROW_SPACING = 8;
const unsigned short TIMELINE_ROW_SIZE = 20;

const QRgb BORDERS_COLOR = 0x00686868;// 0x00a07050;
const QRgb BACKGROUND_1 = 0x00dddddd;
const QRgb BACKGROUND_2 = 0x00ffffff;
const QRgb TIMELINE_BACKGROUND = 0x20303030;
const QRgb SELECTED_ITEM_COLOR = 0x000050a0;
const QColor CHRONOMETER_COLOR2 = QColor::fromRgba(0x40408040);

//const unsigned int TEST_PROGRESSION_BASE = 4;

const int FLICKER_INTERVAL = 16; // 60Hz

const auto CHRONOMETER_FONT = QFont("CourierNew", 16, 2);
const auto ITEMS_FONT = QFont("CourierNew", 9);// , 2);

//////////////////////////////////////////////////////////////////////////

auto const sign = [](int _value) { return _value < 0 ? -1 : 1; };
auto const absmin = [](int _a, int _b) { return abs(_a) < abs(_b) ? _a : _b; };
auto const clamp = [](qreal _minValue, qreal _value, qreal _maxValue) { return _value < _minValue ? _minValue : (_value > _maxValue ? _maxValue : _value); };

//////////////////////////////////////////////////////////////////////////

template <int N, class T>
inline T logn(T _value)
{
    static const double div = 1.0 / log2((double)N);
    return log2(_value) * div;
}

//////////////////////////////////////////////////////////////////////////

EasyGraphicsItem::EasyGraphicsItem(uint8_t _index, const::profiler::BlocksTreeRoot* _root)
    : QGraphicsItem(nullptr)
    , m_pRoot(_root)
    , m_index(_index)
{
}

EasyGraphicsItem::~EasyGraphicsItem()
{
}

const EasyGraphicsView* EasyGraphicsItem::view() const
{
    return static_cast<const EasyGraphicsView*>(scene()->parent());
}

//////////////////////////////////////////////////////////////////////////

QRectF EasyGraphicsItem::boundingRect() const
{
    //const auto sceneView = view();
    //return QRectF(m_boundingRect.left() - sceneView->offset() / sceneView->scale(), m_boundingRect.top(), m_boundingRect.width() * sceneView->scale(), m_boundingRect.height());
    return m_boundingRect;
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    if (m_levels.empty() || m_levels.front().empty())
    {
        return;
    }

    const auto sceneView = view();
    const auto visibleSceneRect = sceneView->visibleSceneRect(); // Current visible scene rect
    const auto currentScale = sceneView->scale(); // Current GraphicsView scale
    const auto offset = sceneView->offset();
    const auto sceneLeft = offset, sceneRight = offset + visibleSceneRect.width() / currentScale;

    //printf("VISIBLE = {%lf, %lf}\n", sceneLeft, sceneRight);

    QRectF rect;
    QBrush brush;
    QRgb previousColor = 0, inverseColor = 0x00ffffff;
    Qt::PenStyle previousPenStyle = Qt::NoPen;
    brush.setStyle(Qt::SolidPattern);

    _painter->save();
    _painter->setFont(ITEMS_FONT);
    
    // Reset indices of first visible item for each layer
    const auto levelsNumber = levels();
    for (uint8_t i = 1; i < levelsNumber; ++i) ::profiler_gui::set_max(m_levelsIndexes[i]);


    // Search for first visible top-level item
    auto& level0 = m_levels.front();
    auto first = ::std::lower_bound(level0.begin(), level0.end(), sceneLeft, [](const ::profiler_gui::EasyBlockItem& _item, qreal _value)
    {
        return _item.left() < _value;
    });

    if (first != level0.end())
    {
        m_levelsIndexes[0] = first - level0.begin();
        if (m_levelsIndexes[0] > 0)
            m_levelsIndexes[0] -= 1;
    }
    else
    {
        m_levelsIndexes[0] = static_cast<unsigned int>(level0.size() - 1);
    }


#ifdef EASY_STORE_CSWITCH_SEPARATELY
    auto firstSync = ::std::lower_bound(m_pRoot->sync.begin(), m_pRoot->sync.end(), sceneLeft, [&sceneView](::profiler::block_index_t _index, qreal _value)
    {
        return sceneView->time2position(easyBlock(_index).tree.node->begin()) < _value;
    });

    if (firstSync != m_pRoot->sync.end())
    {
        if (firstSync != m_pRoot->sync.begin())
            --firstSync;
    }
    else if (!m_pRoot->sync.empty())
    {
        firstSync = m_pRoot->sync.begin() + m_pRoot->sync.size() - 1;
    }
    firstSync = m_pRoot->sync.begin();
#endif


    // This is to make _painter->drawText() work properly
    // (it seems there is a bug in Qt5.6 when drawText called for big coordinates,
    // drawRect at the same time called for actually same coordinates
    // works fine without using this additional shifting)
    auto dx = level0[m_levelsIndexes[0]].left() * currentScale;



    // Shifting coordinates to current screen offset
    _painter->setTransform(QTransform::fromTranslate(dx - offset * currentScale, -y()), true);


    if (EASY_GLOBALS.draw_graphics_items_borders)
    {
        previousPenStyle = Qt::SolidLine;
        _painter->setPen(BORDERS_COLOR);
    }
    else
    {
        _painter->setPen(Qt::NoPen);
    }


    static const auto MAX_CHILD_INDEX = ::profiler_gui::numeric_max<decltype(::profiler_gui::EasyBlockItem::children_begin)>();
    auto const skip_children = [this, &levelsNumber](short next_level, decltype(::profiler_gui::EasyBlockItem::children_begin) children_begin)
    {
        // Mark that we would not paint children of current item
        if (next_level < levelsNumber && children_begin != MAX_CHILD_INDEX)
            m_levels[next_level][children_begin].state = BLOCK_ITEM_DO_NOT_PAINT;
    };


    // Iterate through layers and draw visible items
    bool selectedItemsWasPainted = false;
    const auto visibleBottom = visibleSceneRect.bottom() - 1;
    for (uint8_t l = 0; l < levelsNumber; ++l)
    {
        auto& level = m_levels[l];
        const short next_level = l + 1;
        char state = BLOCK_ITEM_DO_PAINT;

        const auto top = levelY(l);
        qreal prevRight = -1e100;
        for (unsigned int i = m_levelsIndexes[l], end = static_cast<unsigned int>(level.size()); i < end; ++i)
        {
            auto& item = level[i];

            if (item.left() > sceneRight)
                break; // This is first totally invisible item. No need to check other items.

            if (item.state != BLOCK_ITEM_UNCHANGED)
            {
                state = item.state;
            }

            if (item.right() < sceneLeft || state == BLOCK_ITEM_DO_NOT_PAINT || top > visibleBottom || (top + item.totalHeight) < visibleSceneRect.top())
            {
                // This item is not visible
                skip_children(next_level, item.children_begin);
                continue;
            }

            const auto x = item.left() * currentScale - dx;
            auto w = item.width() * currentScale;
            if (x + w <= prevRight)
            {
                // This item is not visible
                if (w < 20)
                    skip_children(next_level, item.children_begin);
                continue;
            }

            const auto& itemBlock = easyBlock(item.block);
            int h = 0, flags = 0;
            if (w < 20 || !itemBlock.expanded)
            {
                // Items which width is less than 20 will be painted as big rectangles which are hiding it's children

                //x = item.left() * currentScale - dx;
                h = item.totalHeight;
                const auto dh = top + h - visibleBottom;
                if (dh > 0)
                    h -= dh;

                bool changepen = false;
                if (item.block == EASY_GLOBALS.selected_block)
                {
                    selectedItemsWasPainted = true;
                    changepen = true;
                    QPen pen(Qt::SolidLine);
                    pen.setColor(Qt::red);
                    pen.setWidth(2);
                    _painter->setPen(pen);

                    previousColor = SELECTED_ITEM_COLOR;
                    inverseColor = 0x00ffffff - previousColor;
                    brush.setColor(previousColor);
                    _painter->setBrush(brush);
                }
                else
                {
                    const bool colorChange = (previousColor != item.color);
                    if (colorChange)
                    {
                        // Set background color brush for rectangle
                        previousColor = item.color;
                        inverseColor = 0x00ffffff - previousColor;
                        brush.setColor(previousColor);
                        _painter->setBrush(brush);
                    }

                    if (EASY_GLOBALS.draw_graphics_items_borders)
                    {
                        //if (w < 2)
                        //{
                        //    // Do not paint borders for very narrow items
                        //    if (previousPenStyle != Qt::NoPen)
                        //    {
                        //        previousPenStyle = Qt::NoPen;
                        //        _painter->setPen(Qt::NoPen);
                        //    }
                        //}
                        //else
                        if (previousPenStyle != Qt::SolidLine || colorChange)
                        {
                            // Restore pen for item which is wide enough to paint borders
                            previousPenStyle = Qt::SolidLine;
                            _painter->setPen(BORDERS_COLOR & inverseColor);// BORDERS_COLOR);
                        }
                    }
                }

                if (w < 2) w = 2;
                //if (w < 2 && !changepen)
                //{
                //    // Draw line
                //    changepen = true;
                //    _painter->setPen(previousColor);
                //    _painter->drawLine(QPointF(x, top), QPointF(x, top + h));
                //}
                //else
                {
                    // Draw rectangle
                    rect.setRect(x, top, w, h);
                    _painter->drawRect(rect);
                }

                if (changepen)
                {
                    if (previousPenStyle == Qt::NoPen)
                        _painter->setPen(Qt::NoPen);
                    else
                        _painter->setPen(BORDERS_COLOR & inverseColor);// BORDERS_COLOR); // restore pen for rectangle painting
                }

                prevRight = rect.right();
                skip_children(next_level, item.children_begin);
                if (w < 20)
                    continue;

                if (item.totalHeight > GRAPHICS_ROW_SIZE)
                    flags = Qt::AlignCenter;
                else if (!(item.width() < 1))
                    flags = Qt::AlignHCenter;
            }
            else
            {
                if (next_level < levelsNumber && item.children_begin != MAX_CHILD_INDEX)
                {
                    if (m_levelsIndexes[next_level] == MAX_CHILD_INDEX)
                    {
                        // Mark first potentially visible child item on next sublevel
                        m_levelsIndexes[next_level] = item.children_begin;
                    }

                    // Mark children items that we want to draw them
                    m_levels[next_level][item.children_begin].state = BLOCK_ITEM_DO_PAINT;
                }

                if (item.block == EASY_GLOBALS.selected_block)
                {
                    selectedItemsWasPainted = true;
                    QPen pen(Qt::SolidLine);
                    pen.setColor(Qt::red);
                    pen.setWidth(2);
                    _painter->setPen(pen);

                    previousColor = SELECTED_ITEM_COLOR;
                    inverseColor = 0x00ffffff - previousColor;
                    brush.setColor(previousColor);
                    _painter->setBrush(brush);
                }
                else
                {
                    const bool colorChange = (previousColor != item.color);
                    if (colorChange)
                    {
                        // Set background color brush for rectangle
                        previousColor = item.color;
                        inverseColor = 0x00ffffff - previousColor;
                        brush.setColor(previousColor);
                        _painter->setBrush(brush);
                    }

                    if (EASY_GLOBALS.draw_graphics_items_borders && (previousPenStyle != Qt::SolidLine || colorChange))
                    {
                        // Restore pen for item which is wide enough to paint borders
                        previousPenStyle = Qt::SolidLine;
                        _painter->setPen(BORDERS_COLOR & inverseColor);// BORDERS_COLOR);
                    }
                }

                // Draw rectangle
                //x = item.left() * currentScale - dx;
                h = GRAPHICS_ROW_SIZE;
                const auto dh = top + h - visibleBottom;
                if (dh > 0)
                    h -= dh;

                rect.setRect(x, top, w, h);
                _painter->drawRect(rect);

                if (!(item.width() < 1))
                    flags = Qt::AlignHCenter;

                prevRight = rect.right();
            }

            // Draw text-----------------------------------
            // calculating text coordinates
            auto xtext = x;
            if (item.left() < sceneLeft)
            {
                // if item left border is out of screen then attach text to the left border of the screen
                // to ensure text is always visible for items presenting on the screen.
                w += (item.left() - sceneLeft) * currentScale;
                xtext = sceneLeft * currentScale - dx;
            }

            if (item.right() > sceneRight)
            {
                w -= (item.right() - sceneRight) * currentScale;
            }

            rect.setRect(xtext + 1, top, w - 1, h);

            // text will be painted with inverse color
            auto textColor = inverseColor;
            if (textColor == previousColor) textColor = 0;
            _painter->setPen(textColor);

            // drawing text
            auto name = *itemBlock.tree.node->name() != 0 ? itemBlock.tree.node->name() : easyDescriptor(itemBlock.tree.node->id()).name();
            _painter->drawText(rect, flags, ::profiler_gui::toUnicode(name));

            // restore previous pen color
            if (previousPenStyle == Qt::NoPen)
                _painter->setPen(Qt::NoPen);
            else
                _painter->setPen(BORDERS_COLOR & inverseColor);// BORDERS_COLOR); // restore pen for rectangle painting
            // END Draw text~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        }
    }

    if (!selectedItemsWasPainted && EASY_GLOBALS.selected_block < EASY_GLOBALS.gui_blocks.size())
    {
        const auto& guiblock = EASY_GLOBALS.gui_blocks[EASY_GLOBALS.selected_block];
        if (guiblock.graphics_item == m_index)
        {
            const auto& item = m_levels[guiblock.graphics_item_level][guiblock.graphics_item_index];
            if (item.left() < sceneRight && item.right() > sceneLeft)
            {
                QPen pen(Qt::SolidLine);
                pen.setColor(Qt::red);
                pen.setWidth(2);
                _painter->setPen(pen);

                brush.setColor(SELECTED_ITEM_COLOR);
                _painter->setBrush(brush);

                auto top = levelY(guiblock.graphics_item_level);
                auto x = item.left() * currentScale - dx;
                auto w = ::std::max(item.width() * currentScale, 1.0);
                rect.setRect(x, top, w, item.totalHeight);
                _painter->drawRect(rect);

                if (w > 20)
                {
                    // Draw text-----------------------------------
                    // calculating text coordinates
                    auto xtext = x;
                    if (item.left() < sceneLeft)
                    {
                        // if item left border is out of screen then attach text to the left border of the screen
                        // to ensure text is always visible for items presenting on the screen.
                        w += (item.left() - sceneLeft) * currentScale;
                        xtext = sceneLeft * currentScale - dx;
                    }

                    if (item.right() > sceneRight)
                    {
                        w -= (item.right() - sceneRight) * currentScale;
                    }

                    rect.setRect(xtext + 1, top, w - 1, item.totalHeight);

                    // text will be painted with inverse color
                    auto textColor = 0x00ffffff - previousColor;
                    if (textColor == previousColor) textColor = 0;
                    _painter->setPen(textColor);

                    // drawing text
                    const auto& itemBlock = easyBlock(item.block);
                    auto name = *itemBlock.tree.node->name() != 0 ? itemBlock.tree.node->name() : easyDescriptor(itemBlock.tree.node->id()).name();
                    _painter->drawText(rect, Qt::AlignCenter, ::profiler_gui::toUnicode(name));
                    // END Draw text~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                }
            }
        }
    }


#ifdef EASY_STORE_CSWITCH_SEPARATELY
    if (!m_pRoot->sync.empty())
    {
        _painter->setBrush(QColor::fromRgba(0xfff08040));
        _painter->setPen(QColor::fromRgb(0x00505050));

        qreal prevRight = -1e100, top = y() - 4, h = 3;
        if (top + h < visibleBottom)
        {
            for (auto it = firstSync, end = m_pRoot->sync.end(); it != end; ++it)
            {
                const auto& item = easyBlock(*it).tree;
                auto begin = sceneView->time2position(item.node->begin());

                if (begin > sceneRight)
                    break; // This is first totally invisible item. No need to check other items.

                decltype(begin) width = sceneView->time2position(item.node->end()) - begin;
                auto r = begin + width;
                if (r < sceneLeft) // This item is not visible
                    continue;

                begin *= currentScale;
                begin -= dx;
                width *= currentScale;
                r = begin + width;
                if (r <= prevRight) // This item is not visible
                    continue;

                if (begin < prevRight)
                {
                    width -= prevRight - begin;
                    begin = prevRight;
                }

                if (width < 2)
                    width = 2;

                //_painter->drawLine(QLineF(::std::max(begin, prevRight), top, begin + width, top));
                rect.setRect(begin, top, width, h);
                _painter->drawRect(rect);
                prevRight = begin + width;
            }
        }
    }
#endif


    _painter->restore();
}

//////////////////////////////////////////////////////////////////////////

QRect EasyGraphicsItem::getRect() const
{
    return view()->mapFromScene(m_boundingRect).boundingRect();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsItem::getBlocks(qreal _left, qreal _right, ::profiler_gui::TreeBlocks& _blocks) const
{
    //if (m_bTest)
    //{
    //    return;
    //}

    // Search for first visible top-level item
    auto& level0 = m_levels.front();
    auto first = ::std::lower_bound(level0.begin(), level0.end(), _left, [](const ::profiler_gui::EasyBlockItem& _item, qreal _value)
    {
        return _item.left() < _value;
    });

    size_t itemIndex = 0;
    if (first != level0.end())
    {
        itemIndex = first - level0.begin();
        if (itemIndex > 0)
            itemIndex -= 1;
    }
    else
    {
        itemIndex = level0.size() - 1;
    }

    // Add all visible top-level items into array of visible blocks
    for (size_t i = itemIndex, end = level0.size(); i < end; ++i)
    {
        const auto& item = level0[i];

        if (item.left() > _right)
        {
            // First invisible item. No need to check other items.
            break;
        }

        if (item.right() < _left)
        {
            // This item is not visible yet
            // This is just to be sure
            continue;
        }

        _blocks.emplace_back(m_pRoot, item.block);
    }
}

//////////////////////////////////////////////////////////////////////////

const ::profiler_gui::EasyBlockItem* EasyGraphicsItem::intersect(const QPointF& _pos) const
{
    if (m_levels.empty() || m_levels.front().empty())
    {
        return nullptr;
    }

    const auto& level0 = m_levels.front();
    const auto top = y();

    if (top > _pos.y())
    {
        return nullptr;
    }

    const auto bottom = top + m_levels.size() * GRAPHICS_ROW_SIZE_FULL;
    if (bottom < _pos.y())
    {
        return nullptr;
    }

    const unsigned int levelIndex = static_cast<unsigned int>(_pos.y() - top) / GRAPHICS_ROW_SIZE_FULL;
    if (levelIndex >= m_levels.size())
    {
        return nullptr;
    }

    const auto currentScale = view()->scale();
    unsigned int i = 0;
    size_t itemIndex = ::std::numeric_limits<size_t>::max();
    size_t firstItem = 0, lastItem = static_cast<unsigned int>(level0.size());
    while (i <= levelIndex)
    {
        const auto& level = m_levels[i];

        // Search for first visible item
        auto first = ::std::lower_bound(level.begin() + firstItem, level.begin() + lastItem, _pos.x(), [](const ::profiler_gui::EasyBlockItem& _item, qreal _value)
        {
            return _item.left() < _value;
        });

        if (first != level.end())
        {
            itemIndex = first - level.begin();
            if (itemIndex != 0)
                --itemIndex;
        }
        else
        {
            itemIndex = level.size() - 1;
        }

        for (auto size = level.size(); itemIndex < size; ++itemIndex)
        {
            const auto& item = level[itemIndex];
            static const auto MAX_CHILD_INDEX = ::profiler_gui::numeric_max(item.children_begin);

            if (item.left() > _pos.x())
            {
                return nullptr;
            }

            if (item.right() < _pos.x())
            {
                continue;
            }

            const auto w = item.width() * currentScale;
            if (i == levelIndex || w < 20 || !easyBlock(item.block).expanded)
            {
                return &item;
            }

            if (item.children_begin == MAX_CHILD_INDEX)
            {
                if (itemIndex != 0)
                {
                    auto j = itemIndex;
                    firstItem = 0;
                    do {

                        --j;
                        const auto& item2 = level[j];
                        if (item2.children_begin != MAX_CHILD_INDEX)
                        {
                            firstItem = item2.children_begin;
                            break;
                        }

                    } while (j != 0);
                }
                else
                {
                    firstItem = 0;
                }
            }
            else
            {
                firstItem = item.children_begin;
            }

            lastItem = m_levels[i + 1].size();
            for (auto j = itemIndex + 1; j < size; ++j)
            {
                const auto& item2 = level[j];
                if (item2.children_begin != MAX_CHILD_INDEX)
                {
                    lastItem = item2.children_begin;
                    break;
                }
            }

            break;
        }

        ++i;
    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsItem::setBoundingRect(qreal x, qreal y, qreal w, qreal h)
{
    m_boundingRect.setRect(x, y, w, h);
}

void EasyGraphicsItem::setBoundingRect(const QRectF& _rect)
{
    m_boundingRect = _rect;
}

//////////////////////////////////////////////////////////////////////////

::profiler::thread_id_t EasyGraphicsItem::threadId() const
{
    return m_pRoot->thread_id;
}

//////////////////////////////////////////////////////////////////////////

uint8_t EasyGraphicsItem::levels() const
{
    return static_cast<uint8_t>(m_levels.size());
}

float EasyGraphicsItem::levelY(uint8_t _level) const
{
    return y() + static_cast<int>(_level) * static_cast<int>(GRAPHICS_ROW_SIZE_FULL);
}

void EasyGraphicsItem::setLevels(uint8_t _levels)
{
    typedef decltype(m_levelsIndexes) IndexesT;
    static const auto MAX_CHILD_INDEX = ::profiler_gui::numeric_max<IndexesT::value_type>();

    m_levels.resize(_levels);
    m_levelsIndexes.resize(_levels, MAX_CHILD_INDEX);
}

void EasyGraphicsItem::reserve(uint8_t _level, unsigned int _items)
{
    m_levels[_level].reserve(_items);
}

//////////////////////////////////////////////////////////////////////////

const EasyGraphicsItem::Children& EasyGraphicsItem::items(uint8_t _level) const
{
    return m_levels[_level];
}

const ::profiler_gui::EasyBlockItem& EasyGraphicsItem::getItem(uint8_t _level, unsigned int _index) const
{
    return m_levels[_level][_index];
}

::profiler_gui::EasyBlockItem& EasyGraphicsItem::getItem(uint8_t _level, unsigned int _index)
{
    return m_levels[_level][_index];
}

unsigned int EasyGraphicsItem::addItem(uint8_t _level)
{
    m_levels[_level].emplace_back();
    return static_cast<unsigned int>(m_levels[_level].size() - 1);
}

//////////////////////////////////////////////////////////////////////////

EasyChronometerItem::EasyChronometerItem(bool _main)
    : QGraphicsItem()
    , m_color(CHRONOMETER_COLOR)
    , m_left(0)
    , m_right(0)
    , m_bMain(_main)
    , m_bReverse(false)
    , m_bHoverIndicator(false)
{
    //setZValue(_main ? 10 : 9);
    m_indicator.reserve(3);
}

EasyChronometerItem::~EasyChronometerItem()
{
}

QRectF EasyChronometerItem::boundingRect() const
{
    return m_boundingRect;
}

void EasyChronometerItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    const auto sceneView = view();
    const auto currentScale = sceneView->scale();
    const auto offset = sceneView->offset();
    const auto visibleSceneRect = sceneView->visibleSceneRect();
    auto sceneLeft = offset, sceneRight = offset + visibleSceneRect.width() / currentScale;

    if (m_bMain)
        m_indicator.clear();

    if (m_left > sceneRight || m_right < sceneLeft)
    {
        // This item is out of screen

        if (m_bMain)
        {
            const int size = m_bHoverIndicator ? 12 : 10;
            auto vcenter = visibleSceneRect.top() + visibleSceneRect.height() * 0.5;
            auto color = QColor::fromRgb(m_color.rgb());
            auto pen = _painter->pen();
            pen.setColor(color);

            m_indicator.clear();
            if (m_left > sceneRight)
            {
                sceneRight = (sceneRight - offset) * currentScale;
                m_indicator.push_back(QPointF(sceneRight - size, vcenter - size));
                m_indicator.push_back(QPointF(sceneRight, vcenter));
                m_indicator.push_back(QPointF(sceneRight - size, vcenter + size));
            }
            else
            {
                sceneLeft = (sceneLeft - offset) * currentScale;
                m_indicator.push_back(QPointF(sceneLeft + size, vcenter - size));
                m_indicator.push_back(QPointF(sceneLeft, vcenter));
                m_indicator.push_back(QPointF(sceneLeft + size, vcenter + size));
            }

            _painter->save();
            _painter->setTransform(QTransform::fromTranslate(-x(), -y()), true);
            _painter->setBrush(m_bHoverIndicator ? QColor::fromRgb(0xffff0000) : color);
            _painter->setPen(pen);
            _painter->drawPolygon(m_indicator);
            _painter->restore();
        }

        return;
    }

    auto selectedInterval = width();
    QRectF rect((m_left - offset) * currentScale, visibleSceneRect.top(), ::std::max(selectedInterval * currentScale, 1.0), visibleSceneRect.height());
    selectedInterval = units2microseconds(selectedInterval);

    const QString text = ::profiler_gui::timeStringReal(selectedInterval); // Displayed text
    const auto textRect = QFontMetricsF(CHRONOMETER_FONT).boundingRect(text); // Calculate displayed text boundingRect
    const auto rgb = m_color.rgb() & 0x00ffffff;



    // Paint!--------------------------
    _painter->save();

    // instead of scrollbar we're using manual offset
    _painter->setTransform(QTransform::fromTranslate(-x(), -y()), true);

    if (m_left < sceneLeft)
        rect.setLeft(0);

    if (m_right > sceneRight)
        rect.setWidth((sceneRight - offset) * currentScale - rect.left());

    // draw transparent rectangle
    auto vcenter = rect.top() + rect.height() * 0.5;
    QLinearGradient g(rect.left(), vcenter, rect.right(), vcenter);
    g.setColorAt(0, m_color);
    g.setColorAt(0.15, QColor::fromRgba(0x10000000 | rgb));
    g.setColorAt(0.85, QColor::fromRgba(0x10000000 | rgb));
    g.setColorAt(1, m_color);
    _painter->setBrush(g);
    _painter->setPen(Qt::NoPen);
    _painter->drawRect(rect);

    // draw left and right borders
    _painter->setBrush(Qt::NoBrush);
    _painter->setPen(QColor::fromRgba(0xd0000000 | rgb));
    if (m_left > sceneLeft)
        _painter->drawLine(QPointF(rect.left(), rect.top()), QPointF(rect.left(), rect.bottom()));
    if (m_right < sceneRight)
        _painter->drawLine(QPointF(rect.right(), rect.top()), QPointF(rect.right(), rect.bottom()));

    // draw text
    _painter->setCompositionMode(QPainter::CompositionMode_Difference); // This lets the text to be visible on every background
    _painter->setPen(0xffffffff - rgb);
    _painter->setFont(CHRONOMETER_FONT);

    int textFlags = 0;
    switch (EASY_GLOBALS.chrono_text_position)
    {
        case ::profiler_gui::ChronoTextPosition_Top:
            textFlags = Qt::AlignTop | Qt::AlignHCenter;
            if (!m_bMain) rect.setTop(rect.top() + textRect.height() * 0.75);
            break;

        case ::profiler_gui::ChronoTextPosition_Center:
            textFlags = Qt::AlignCenter;
            if (!m_bMain) rect.setTop(rect.top() + textRect.height() * 1.5);
            break;

        case ::profiler_gui::ChronoTextPosition_Bottom:
            textFlags = Qt::AlignBottom | Qt::AlignHCenter;
            if (!m_bMain) rect.setHeight(rect.height() - textRect.height() * 0.75);
            break;
    }

    if (textRect.width() < rect.width())
    {
        // Text will be drawed inside rectangle
        _painter->drawText(rect, textFlags, text);
        _painter->restore();
        return;
    }

    const auto w = textRect.width() / currentScale;
    if (m_right + w < sceneRight)
    {
        // Text will be drawed to the right of rectangle
        rect.translate(rect.width(), 0);
        textFlags &= ~Qt::AlignHCenter;
        textFlags |= Qt::AlignLeft;
    }
    else if (m_left - w > sceneLeft)
    {
        // Text will be drawed to the left of rectangle
        rect.translate(-rect.width(), 0);
        textFlags &= ~Qt::AlignHCenter;
        textFlags |= Qt::AlignRight;
    }
    //else // Text will be drawed inside rectangle

    _painter->drawText(rect, textFlags | Qt::TextDontClip, text);

    _painter->restore();
    // END Paint!~~~~~~~~~~~~~~~~~~~~~~
}

bool EasyChronometerItem::contains(const QPointF& _pos) const
{
    const auto sceneView = view();
    const auto clickX = (_pos.x() - sceneView->offset()) * sceneView->scale() - x();
    if (!m_indicator.empty() && m_indicator.containsPoint(QPointF(clickX, _pos.y()), Qt::OddEvenFill))
        return true;
    return false;
}

void EasyChronometerItem::setColor(const QColor& _color)
{
    m_color = _color;
}

void EasyChronometerItem::setBoundingRect(qreal x, qreal y, qreal w, qreal h)
{
    m_boundingRect.setRect(x, y, w, h);
}

void EasyChronometerItem::setBoundingRect(const QRectF& _rect)
{
    m_boundingRect = _rect;
}

void EasyChronometerItem::setLeftRight(qreal _left, qreal _right)
{
    if (_left < _right)
    {
        m_left = _left;
        m_right = _right;
    }
    else
    {
        m_left = _right;
        m_right = _left;
    }
}

void EasyChronometerItem::setReverse(bool _reverse)
{
    m_bReverse = _reverse;
}

void EasyChronometerItem::setHover(bool _hover)
{
    m_bHoverIndicator = _hover;
}

const EasyGraphicsView* EasyChronometerItem::view() const
{
    return static_cast<const EasyGraphicsView*>(scene()->parent());
}

//////////////////////////////////////////////////////////////////////////

void EasyBackgroundItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    const auto sceneView = static_cast<const EasyGraphicsView*>(scene()->parent());
    const auto visibleSceneRect = sceneView->visibleSceneRect();
    const auto currentScale = sceneView->scale();
    const auto offset = sceneView->offset();
    const auto left = offset * currentScale;
    const auto h = visibleSceneRect.height();
    const auto visibleBottom = h - 1;

    QRectF rect;

    _painter->save();
    _painter->setTransform(QTransform::fromTranslate(-x(), -y()));

    const auto& items = sceneView->getItems();
    if (!items.empty())
    {
        static const auto OVERLAP = THREADS_ROW_SPACING >> 1;
        static const QBrush brushes[2] = {QColor::fromRgb(BACKGROUND_1), QColor::fromRgb(BACKGROUND_2)};
        int i = -1;

        // Draw background
        _painter->setPen(Qt::NoPen);
        for (auto item : items)
        {
            ++i;

            auto br = item->boundingRect();
            auto top = item->y() + br.top() - visibleSceneRect.top();
            auto bottom = top + br.height();

            if (top > h || bottom < 0)
                continue;

            if (item->threadId() == EASY_GLOBALS.selected_thread)
                _painter->setBrush(QBrush(QColor::fromRgb(::profiler_gui::SELECTED_THREAD_BACKGROUND)));
            else
                _painter->setBrush(brushes[i & 1]);

            rect.setRect(0, top - OVERLAP, visibleSceneRect.width(), br.height() + THREADS_ROW_SPACING);
            const auto dh = rect.bottom() - visibleBottom;
            if (dh > 0)
                rect.setHeight(rect.height() - dh);

            _painter->drawRect(rect);
        }
    }

    // Draw timeline scale marks ----------------
    _painter->setBrush(QColor::fromRgba(TIMELINE_BACKGROUND));

    const auto sceneStep = sceneView->timelineStep();
    const auto factor = ::profiler_gui::timeFactor(sceneStep);
    const auto step = sceneStep * currentScale;
    auto first = static_cast<quint64>(offset / sceneStep);
    const int odd = first & 1;
    const auto nsteps = (1 + odd) * 2 + static_cast<int>(visibleSceneRect.width() / step);
    first -= odd;

    QPen pen(Qt::gray);
    pen.setWidth(2);
    _painter->setPen(pen);
    _painter->drawLine(QPointF(0, h), QPointF(visibleSceneRect.width(), h));
    _painter->setPen(Qt::gray);

    QLineF marks[20];
    qreal first_x = first * sceneStep;
    const auto textWidth = QFontMetricsF(_painter->font()).boundingRect(QString::number(static_cast<quint64>(0.5 + first_x * factor))).width() + 10;
    const int n = 1 + static_cast<int>(textWidth / step);
    int next = first % n;
    if (next)
        next = n - next;

    first_x *= currentScale;
    for (int i = 0; i < nsteps; ++i, --next)
    {
        auto current = first_x - left + step * i;

        if ((i & 1) == 0)
        {
            rect.setRect(current, 0, step, h);
            _painter->drawRect(rect);

            for (int j = 0; j < 20; ++j)
            {
                auto xmark = current + j * step * 0.1;
                marks[j].setLine(xmark, h, xmark, h + ((j % 5) ? 4 : 8));
            }

            _painter->drawLines(marks, 20);
        }

        if (next <= 0)
        {
            next = n;
            _painter->setPen(Qt::black);
            _painter->drawText(QPointF(current + 1, h + 17), QString::number(static_cast<quint64>(0.5 + (current + left) * factor / currentScale)));
            _painter->setPen(Qt::gray);
        }

        // TEST
        // this is for testing (order of lines will be painted):
        //_painter->setPen(Qt::black);
        //_painter->drawText(QPointF(current + step * 0.4, h - 20), QString::number(i));
        //_painter->setPen(Qt::gray);
        // TEST
    }
    // END Draw timeline scale marks ~~~~~~~~~~~~

    _painter->restore();
}

//////////////////////////////////////////////////////////////////////////

void EasyTimelineIndicatorItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    const auto sceneView = static_cast<const EasyGraphicsView*>(scene()->parent());
    const auto visibleSceneRect = sceneView->visibleSceneRect();
    const auto step = sceneView->timelineStep() * sceneView->scale();
    const QString text = ::profiler_gui::timeStringInt(units2microseconds(sceneView->timelineStep())); // Displayed text

    // Draw scale indicator
    _painter->save();
    _painter->setTransform(QTransform::fromTranslate(-x(), -y()));
    _painter->setCompositionMode(QPainter::CompositionMode_Difference);
    _painter->setBrush(Qt::NoBrush);

    //_painter->setBrush(Qt::white);
    //_painter->setPen(Qt::NoPen);

    QPen pen(Qt::white);
    pen.setWidth(2);
    pen.setJoinStyle(Qt::MiterJoin);
    _painter->setPen(pen);

    QRectF rect(visibleSceneRect.width() - 10 - step, visibleSceneRect.height() - 20, step, 10);
    const auto rect_right = rect.right();
    const QPointF points[] = {{rect.left(), rect.bottom()}, {rect.left(), rect.top()}, {rect_right, rect.top()}, {rect_right, rect.top() + 5}};
    _painter->drawPolyline(points, sizeof(points) / sizeof(QPointF));
    //_painter->drawRect(rect);

    rect.translate(0, 3);
    _painter->drawText(rect, Qt::AlignRight | Qt::TextDontClip, text);

    _painter->restore();
}

//////////////////////////////////////////////////////////////////////////

EasyGraphicsView::EasyGraphicsView(QWidget* _parent)
    : QGraphicsView(_parent)
    , m_beginTime(::std::numeric_limits<decltype(m_beginTime)>::max())
    , m_scale(1)
    , m_offset(0)
    , m_timelineStep(0)
    , m_mouseButtons(Qt::NoButton)
    , m_pScrollbar(nullptr)
    , m_chronometerItem(nullptr)
    , m_chronometerItemAux(nullptr)
    , m_flickerSpeedX(0)
    , m_flickerSpeedY(0)
    , m_bDoubleClick(false)
    , m_bUpdatingRect(false)
    //, m_bTest(false)
    , m_bEmpty(true)
{
    initMode();
    setScene(new QGraphicsScene(this));
    updateVisibleSceneRect();
}

EasyGraphicsView::~EasyGraphicsView()
{
}

//////////////////////////////////////////////////////////////////////////

EasyChronometerItem* EasyGraphicsView::createChronometer(bool _main)
{
    auto chronoItem = new EasyChronometerItem(_main);
    chronoItem->setColor(_main ? CHRONOMETER_COLOR : CHRONOMETER_COLOR2);
    chronoItem->setBoundingRect(scene()->sceneRect());
    chronoItem->hide();
    scene()->addItem(chronoItem);

    return chronoItem;
}

//////////////////////////////////////////////////////////////////////////

/*void EasyGraphicsView::fillTestChildren(EasyGraphicsItem* _item, const int _maxlevel, int _level, qreal _x, unsigned int _childrenNumber, unsigned int& _total_items)
{
    unsigned int nchildren = _childrenNumber;
    _childrenNumber = TEST_PROGRESSION_BASE;

    for (unsigned int i = 0; i < nchildren; ++i)
    {
        auto j = _item->addItem(_level);
        auto& b = _item->getItem(_level, j);
        b.color = ::profiler_gui::toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);
        b.state = 0;

        if (_level < _maxlevel)
        {
            const auto& children = _item->items(_level + 1);
            b.children_begin = static_cast<unsigned int>(children.size());

            fillTestChildren(_item, _maxlevel, _level + 1, _x, _childrenNumber, _total_items);

            const auto& last = children.back();
            b.setPos(_x, last.right() - _x);
            b.totalHeight = GRAPHICS_ROW_SIZE_FULL + last.totalHeight;
        }
        else
        {
            b.setPos(_x, units2microseconds(10 + rand() % 190));
            b.totalHeight = GRAPHICS_ROW_SIZE;
            ::profiler_gui::set_max(b.children_begin);
        }

        _x = b.right();
        ++_total_items;
    }
}

void EasyGraphicsView::test(unsigned int _frames_number, unsigned int _total_items_number_estimate, uint8_t _rows)
{
    static const qreal X_BEGIN = 50;
    static const qreal Y_BEGIN = 0;

    clearSilent(); // Clear scene

    // Calculate items number for first level
    _rows = ::std::max((uint8_t)1, _rows);
    const auto children_per_frame = static_cast<unsigned int>(0.5 + static_cast<double>(_total_items_number_estimate) / static_cast<double>(_rows * _frames_number));
    const uint8_t max_depth = ::std::min(254, static_cast<int>(logn<TEST_PROGRESSION_BASE>(children_per_frame * (TEST_PROGRESSION_BASE - 1) * 0.5 + 1)));
    const auto first_level_children_count = static_cast<unsigned int>(static_cast<double>(children_per_frame) * (1.0 - TEST_PROGRESSION_BASE) / (1.0 - pow(TEST_PROGRESSION_BASE, max_depth)) + 0.5);


    auto bgItem = new EasyBackgroundItem();
    scene()->addItem(bgItem);


    ::std::vector<EasyGraphicsItem*> thread_items(_rows);
    for (uint8_t i = 0; i < _rows; ++i)
    {
        auto item = new EasyGraphicsItem(i, true);
        thread_items[i] = item;

        item->setPos(0, Y_BEGIN + i * (max_depth * GRAPHICS_ROW_SIZE_FULL + THREADS_ROW_SPACING * 5));

        item->setLevels(max_depth + 1);
        item->reserve(0, _frames_number);
    }

    // Calculate items number for each sublevel
    auto chldrn = first_level_children_count;
    for (uint8_t i = 1; i <= max_depth; ++i)
    {
        for (uint8_t j = 0; j < _rows; ++j)
        {
            auto item = thread_items[j];
            item->reserve(i, chldrn * _frames_number);
        }

        chldrn *= TEST_PROGRESSION_BASE;
    }

    // Create required number of items
    unsigned int total_items = 0;
    qreal maxX = 0;
    const EasyGraphicsItem* longestItem = nullptr;
    for (uint8_t i = 0; i < _rows; ++i)
    {
        auto item = thread_items[i];
        qreal x = X_BEGIN, y = item->y();
        for (unsigned int f = 0; f < _frames_number; ++f)
        {
            auto j = item->addItem(0);
            auto& b = item->getItem(0, j);
            b.color = ::profiler_gui::toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);
            b.state = 0;

            const auto& children = item->items(1);
            b.children_begin = static_cast<unsigned int>(children.size());

            fillTestChildren(item, max_depth, 1, x, first_level_children_count, total_items);

            const auto& last = children.back();
            b.setPos(x, last.right() - x);
            b.totalHeight = GRAPHICS_ROW_SIZE_FULL + last.totalHeight;

            x += b.width() * 1.2;

            ++total_items;
        }

        const auto h = item->getItem(0, 0).totalHeight;
        item->setBoundingRect(0, 0, x, h);

        m_items.push_back(item);
        scene()->addItem(item);

        if (maxX < x)
        {
            maxX = x;
            longestItem = item;
        }
    }

    printf("TOTAL ITEMS = %u\n", total_items);

    // Calculate scene rect
    auto item = thread_items.back();
    scene()->setSceneRect(0, 0, maxX, item->y() + item->getItem(0, 0).totalHeight);

    // Reset necessary values
    m_offset = 0;
    updateVisibleSceneRect();
    setScrollbar(m_pScrollbar);

    if (longestItem != nullptr)
    {
        m_pScrollbar->setMinimapFrom(0, longestItem->items(0));
        EASY_GLOBALS.selected_thread = 0;
        emitEASY_GLOBALS.events.selectedThreadChanged(0);
    }

    // Create new chronometer item (previous item was destroyed by scene on scene()->clear()).
    // It will be shown on mouse right button click.
    m_chronometerItemAux = createChronometer(false);
    m_chronometerItem = createChronometer(true);

    bgItem->setBoundingRect(scene()->sceneRect());
    auto indicator = new EasyTimelineIndicatorItem();
    indicator->setBoundingRect(scene()->sceneRect());
    scene()->addItem(indicator);

    // Set necessary flags
    m_bTest = true;
    m_bEmpty = false;

    scaleTo(BASE_SCALE);
}*/

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::clearSilent()
{
    const QSignalBlocker blocker(this), sceneBlocker(scene()); // block all scene signals (otherwise clear() would be extremely slow!)

    // Stop flicking
    m_flickerTimer.stop();
    m_flickerSpeedX = 0;
    m_flickerSpeedY = 0;

    // Clear all items
    scene()->clear();
    m_items.clear();
    m_selectedBlocks.clear();

    m_beginTime = ::std::numeric_limits<decltype(m_beginTime)>::max(); // reset begin time
    m_scale = 1; // scale back to initial 100% scale
    m_timelineStep = 1;
    m_offset = 0; // scroll back to the beginning of the scene

    // Reset necessary flags
    //m_bTest = false;
    m_bEmpty = true;

    // notify ProfTreeWidget that selection was reset
    emit intervalChanged(m_selectedBlocks, m_beginTime, 0, 0, false);
}

void EasyGraphicsView::setTree(const ::profiler::thread_blocks_tree_t& _blocksTree)
{
    // clear scene
    clearSilent();

    if (_blocksTree.empty())
    {
        return;
    }

    auto bgItem = new EasyBackgroundItem();
    scene()->addItem(bgItem);

    // set new blocks tree
    // calculate scene size and fill it with items

    // Calculating start and end time
    ::profiler::timestamp_t finish = 0;
    ::profiler::thread_id_t longestTree = 0;
    const EasyGraphicsItem* longestItem = nullptr;
    for (const auto& threadTree : _blocksTree)
    {
        const auto& tree = threadTree.second.children;
        auto timestart = blocksTree(tree.front()).node->begin();

#ifdef EASY_STORE_CSWITCH_SEPARATELY
        if (!threadTree.second.sync.empty())
            timestart = ::std::min(timestart, blocksTree(threadTree.second.sync.front()).node->begin());
#endif

        const auto timefinish = blocksTree(tree.back()).node->end();

        if (m_beginTime > timestart)
        {
            m_beginTime = timestart;
        }

        if (finish < timefinish)
        {
            finish = timefinish;
            longestTree = threadTree.first;
        }
    }

    // Filling scene with items
    m_items.reserve(_blocksTree.size());
    qreal y = TIMELINE_ROW_SIZE;
    for (const auto& threadTree : _blocksTree)
    {
        if (m_items.size() == 0xff)
        {
            qWarning() << "Warning: Maximum threads number (255 threads) exceeded! See EasyGraphicsView::setTree() : " << __LINE__ << " in file " << __FILE__;
            break;
        }

        // fill scene with new items
        const auto& tree = threadTree.second.children;
        qreal h = 0, x = time2position(blocksTree(tree.front()).node->begin());
        auto item = new EasyGraphicsItem(static_cast<uint8_t>(m_items.size()), &threadTree.second);
        item->setLevels(threadTree.second.depth);
        item->setPos(0, y);

        const auto children_duration = setTree(item, tree, h, y, 0);

        item->setBoundingRect(0, 0, children_duration + x, h);
        m_items.push_back(item);
        scene()->addItem(item);

        y += h + THREADS_ROW_SPACING;

        if (longestTree == threadTree.first)
        {
            longestItem = item;
        }
    }

    // Calculating scene rect
    const qreal endX = time2position(finish) + 1500.0;
    scene()->setSceneRect(0, 0, endX, y + TIMELINE_ROW_SIZE);
    //for (auto item : m_items)
    //    item->setBoundingRect(0, 0, endX, item->boundingRect().height());

    // Center view on the beginning of the scene
    updateVisibleSceneRect();
    setScrollbar(m_pScrollbar);

    if (longestItem != nullptr)
    {
        m_pScrollbar->setMinimapFrom(longestItem->threadId(), longestItem->items(0));
        EASY_GLOBALS.selected_thread = longestItem->threadId();
        emit EASY_GLOBALS.events.selectedThreadChanged(longestItem->threadId());
    }

    // Create new chronometer item (previous item was destroyed by scene on scene()->clear()).
    // It will be shown on mouse right button click.
    m_chronometerItemAux = createChronometer(false);
    m_chronometerItem = createChronometer(true);

    bgItem->setBoundingRect(0, 0, endX, y);
    auto indicator = new EasyTimelineIndicatorItem();
    indicator->setBoundingRect(0, 0, endX, y);
    scene()->addItem(indicator);

    // Setting flags
    //m_bTest = false;
    m_bEmpty = false;

    scaleTo(BASE_SCALE);
}

const EasyGraphicsView::Items &EasyGraphicsView::getItems() const
{
    return m_items;
}

qreal EasyGraphicsView::setTree(EasyGraphicsItem* _item, const ::profiler::BlocksTree::children_t& _children, qreal& _height, qreal _y, short _level)
{
    static const qreal MIN_DURATION = 0.25;

    if (_children.empty())
    {
        return 0;
    }

    const auto level = static_cast<uint8_t>(_level);
    _item->reserve(level, static_cast<unsigned int>(_children.size()));

    const short next_level = _level + 1;
    bool warned = false;
    qreal total_duration = 0, prev_end = 0, maxh = 0;
    qreal start_time = -1;
    for (auto child_index : _children)
    {
        auto& gui_block = easyBlock(child_index);
        const auto& child = gui_block.tree;

        auto xbegin = time2position(child.node->begin());
        if (start_time < 0)
        {
            start_time = xbegin;
        }

        auto duration = time2position(child.node->end()) - xbegin;

        //const auto dt = xbegin - prev_end;
        //if (dt < 0)
        //{
        //    duration += dt;
        //    xbegin -= dt;
        //}

        if (duration < MIN_DURATION)
        {
            duration = MIN_DURATION;
        }

        auto i = _item->addItem(level);
        auto& b = _item->getItem(level, i);

        gui_block.graphics_item = _item->index();
        gui_block.graphics_item_level = level;
        gui_block.graphics_item_index = i;

        if (next_level < 256 && next_level < _item->levels() && !child.children.empty())
        {
            b.children_begin = static_cast<unsigned int>(_item->items(static_cast<uint8_t>(next_level)).size());
        }
        else
        {
            ::profiler_gui::set_max(b.children_begin);
        }

        qreal h = 0;
        qreal children_duration = 0;

        if (next_level < 256)
        {
            children_duration = setTree(_item, child.children, h, _y + GRAPHICS_ROW_SIZE_FULL, next_level);
        }
        else if (!child.children.empty() && !warned)
        {
            warned = true;
            qWarning() << "Warning: Maximum blocks depth (255) exceeded! See EasyGraphicsView::setTree() : " << __LINE__ << " in file " << __FILE__;
        }

        if (duration < children_duration)
        {
            duration = children_duration;
        }

        if (h > maxh)
        {
            maxh = h;
        }

        const auto color = EASY_GLOBALS.descriptors[child.node->id()]->color();
        b.block = child_index;// &child;
        b.color = ::profiler_gui::fromProfilerRgb(::profiler::colors::get_red(color), ::profiler::colors::get_green(color), ::profiler::colors::get_blue(color));
        b.setPos(xbegin, duration);
        b.totalHeight = GRAPHICS_ROW_SIZE + h;
        b.state = BLOCK_ITEM_UNCHANGED;

        prev_end = xbegin + duration;
        total_duration = prev_end - start_time;
    }

    _height += GRAPHICS_ROW_SIZE_FULL + maxh;

    return total_duration;
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::setScrollbar(EasyGraphicsScrollbar* _scrollbar)
{
    auto const prevScrollbar = m_pScrollbar;
    const bool makeConnect = prevScrollbar == nullptr || prevScrollbar != _scrollbar;

    if (prevScrollbar != nullptr && prevScrollbar != _scrollbar)
    {
        disconnect(prevScrollbar, &EasyGraphicsScrollbar::valueChanged, this, &This::onGraphicsScrollbarValueChange);
        disconnect(prevScrollbar, &EasyGraphicsScrollbar::wheeled, this, &This::onGraphicsScrollbarWheel);
    }

    m_pScrollbar = _scrollbar;
    m_pScrollbar->setMinimapFrom(0, nullptr);
    m_pScrollbar->hideChrono();
    m_pScrollbar->setRange(0, scene()->width());
    m_pScrollbar->setSliderWidth(m_visibleSceneRect.width());
    m_pScrollbar->setValue(0);

    if (makeConnect)
    {
        connect(m_pScrollbar, &EasyGraphicsScrollbar::valueChanged, this, &This::onGraphicsScrollbarValueChange);
        connect(m_pScrollbar, &EasyGraphicsScrollbar::wheeled, this, &This::onGraphicsScrollbarWheel);
    }

    EASY_GLOBALS.selected_thread = 0;
    emit EASY_GLOBALS.events.selectedThreadChanged(0);
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::updateVisibleSceneRect()
{
    m_visibleSceneRect = mapToScene(rect()).boundingRect();

    auto vbar = verticalScrollBar();
    if (vbar && vbar->isVisible())
        m_visibleSceneRect.setWidth(m_visibleSceneRect.width() - vbar->width() - 2);
    m_visibleSceneRect.setHeight(m_visibleSceneRect.height() - TIMELINE_ROW_SIZE);
}

void EasyGraphicsView::updateTimelineStep(qreal _windowWidth)
{
    const auto time = units2microseconds(_windowWidth);
    if (time < 100)
        m_timelineStep = 1e-2;
    else if (time < 10e3)
        m_timelineStep = 1;
    else if (time < 10e6)
        m_timelineStep = 1e3;
    else
        m_timelineStep = 1e6;

    const auto optimal_steps = static_cast<int>(40 * m_visibleSceneRect.width() / 1500);
    auto steps = time / m_timelineStep;
    while (steps > optimal_steps) {
        m_timelineStep *= 10;
        steps *= 0.1;
    }

    m_timelineStep = microseconds2units(m_timelineStep);
}

void EasyGraphicsView::updateScene()
{
    scene()->update(m_visibleSceneRect);
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::scaleTo(qreal _scale)
{
    if (m_bEmpty)
    {
        return;
    }

    // have to limit scale because of Qt's QPainter feature: it doesn't draw text
    // with very big coordinates (but it draw rectangles with the same coordinates good).
    m_scale = clamp(MIN_SCALE, _scale, MAX_SCALE); 
    updateVisibleSceneRect();

    // Update slider width for scrollbar
    const auto windowWidth = m_visibleSceneRect.width() / m_scale;
    m_pScrollbar->setSliderWidth(windowWidth);

    updateTimelineStep(windowWidth);
    updateScene();
}

void EasyGraphicsView::wheelEvent(QWheelEvent* _event)
{
    if (!m_bEmpty)
        onWheel(mapToScene(_event->pos()).x(), _event->delta());
    _event->accept();
}

void EasyGraphicsView::onGraphicsScrollbarWheel(qreal _mouseX, int _wheelDelta)
{
    for (auto item : m_items)
    {
        if (item->threadId() == EASY_GLOBALS.selected_thread)
        {
            m_bUpdatingRect = true;
            auto vbar = verticalScrollBar();
            vbar->setValue(item->y() + (item->boundingRect().height() - vbar->pageStep()) * 0.5);
            m_bUpdatingRect = false;
            break;
        }
    }

    onWheel(_mouseX, _wheelDelta);
}

void EasyGraphicsView::onWheel(qreal _mouseX, int _wheelDelta)
{
    const decltype(m_scale) scaleCoeff = _wheelDelta > 0 ? ::profiler_gui::SCALING_COEFFICIENT : ::profiler_gui::SCALING_COEFFICIENT_INV;

    // Remember current mouse position
    const auto mousePosition = m_offset + _mouseX / m_scale;

    // have to limit scale because of Qt's QPainter feature: it doesn't draw text
    // with very big coordinates (but it draw rectangles with the same coordinates good).
    m_scale = clamp(MIN_SCALE, m_scale * scaleCoeff, MAX_SCALE);

    //updateVisibleSceneRect(); // Update scene rect

    // Update slider width for scrollbar
    const auto windowWidth = m_visibleSceneRect.width() / m_scale;
    m_pScrollbar->setSliderWidth(windowWidth);

    // Calculate new offset to simulate QGraphicsView::AnchorUnderMouse scaling behavior
    m_offset = clamp(0., mousePosition - _mouseX / m_scale, scene()->width() - windowWidth);

    // Update slider position
    m_bUpdatingRect = true; // To be sure that updateVisibleSceneRect will not be called by scrollbar change
    m_pScrollbar->setValue(m_offset);
    m_bUpdatingRect = false;

    updateVisibleSceneRect(); // Update scene rect
    updateTimelineStep(windowWidth);
    updateScene(); // repaint scene
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::mousePressEvent(QMouseEvent* _event)
{
    if (m_bEmpty)
    {
        _event->accept();
        return;
    }

    m_mouseButtons = _event->buttons();
    m_mousePressPos = _event->pos();

    if (m_mouseButtons & Qt::RightButton)
    {
        const auto mouseX = m_offset + mapToScene(m_mousePressPos).x() / m_scale;
        m_chronometerItem->setLeftRight(mouseX, mouseX);
        m_chronometerItem->setReverse(false);
        m_chronometerItem->hide();
        m_pScrollbar->hideChrono();
    }

    _event->accept();
}

void EasyGraphicsView::mouseDoubleClickEvent(QMouseEvent* _event)
{
    if (m_bEmpty)
    {
        _event->accept();
        return;
    }

    m_mouseButtons = _event->buttons();
    m_mousePressPos = _event->pos();
    m_bDoubleClick = true;

    if (m_mouseButtons & Qt::LeftButton)
    {
        const auto mouseX = m_offset + mapToScene(m_mousePressPos).x() / m_scale;
        m_chronometerItemAux->setLeftRight(mouseX, mouseX);
        m_chronometerItemAux->setReverse(false);
        m_chronometerItemAux->hide();
    }

    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::mouseReleaseEvent(QMouseEvent* _event)
{
    if (m_bEmpty)
    {
        _event->accept();
        return;
    }

    bool changedSelection = false, changedSelectedItem = false;
    if (m_mouseButtons & Qt::RightButton)
    {
        if (m_chronometerItem->isVisible() && m_chronometerItem->width() < 1e-6)
        {
            m_chronometerItem->hide();
            m_chronometerItem->setHover(false);
            m_pScrollbar->hideChrono();
        }

        if (!m_selectedBlocks.empty())
        {
            changedSelection = true;
            m_selectedBlocks.clear();
        }

        if (m_chronometerItem->isVisible())
        {
            //printf("INTERVAL: {%lf, %lf} ms\n", m_chronometerItem->left(), m_chronometerItem->right());

            for (auto item : m_items)
            {
                item->getBlocks(m_chronometerItem->left(), m_chronometerItem->right(), m_selectedBlocks);
            }

            if (!m_selectedBlocks.empty())
            {
                changedSelection = true;
            }
        }
    }

    const ::profiler_gui::EasyBlockItem* selectedBlock = nullptr;
    const auto previouslySelectedBlock = EASY_GLOBALS.selected_block;
    if (m_mouseButtons & Qt::LeftButton)
    {
        bool clicked = false;

        if (m_chronometerItemAux->isVisible() && m_chronometerItemAux->width() < 1e-6)
        {
            m_chronometerItemAux->hide();
        }
        else if (m_chronometerItem->isVisible() && m_chronometerItem->hoverIndicator())
        {
            // Jump to selected zone
            clicked = true;
            m_flickerSpeedX = m_flickerSpeedY = 0;
            m_pScrollbar->setValue(m_chronometerItem->left() + m_chronometerItem->width() * 0.5 - m_pScrollbar->sliderHalfWidth());
        }

        if (!clicked && m_mouseMovePath.manhattanLength() < 5)
        {
            // Handle Click

            //clicked = true;
            auto mouseClickPos = mapToScene(m_mousePressPos);
            mouseClickPos.setX(m_offset + mouseClickPos.x() / m_scale);

            // Try to select one of item blocks
            for (auto item : m_items)
            {
                auto block = item->intersect(mouseClickPos);
                if (block)
                {
                    changedSelectedItem = true;
                    selectedBlock = block;
                    EASY_GLOBALS.selected_block = block->block;
                    break;
                }
            }

            if (!changedSelectedItem && EASY_GLOBALS.selected_block != ::profiler_gui::numeric_max(EASY_GLOBALS.selected_block))
            {
                changedSelectedItem = true;
                ::profiler_gui::set_max(EASY_GLOBALS.selected_block);
            }
        }
    }

    m_bDoubleClick = false;
    m_mouseButtons = _event->buttons();
    m_mouseMovePath = QPoint();
    _event->accept();

    if (changedSelection)
    {
        emit intervalChanged(m_selectedBlocks, m_beginTime, position2time(m_chronometerItem->left()), position2time(m_chronometerItem->right()), m_chronometerItem->reverse());
    }

    m_chronometerItem->setReverse(false);

    if (changedSelectedItem)
    {
        m_bUpdatingRect = true;
        if (selectedBlock != nullptr && previouslySelectedBlock == EASY_GLOBALS.selected_block && !blocksTree(selectedBlock->block).children.empty())
        {
            EASY_GLOBALS.gui_blocks[previouslySelectedBlock].expanded = !EASY_GLOBALS.gui_blocks[previouslySelectedBlock].expanded;
            emit EASY_GLOBALS.events.itemsExpandStateChanged();
        }
        emit EASY_GLOBALS.events.selectedBlockChanged(EASY_GLOBALS.selected_block);
        m_bUpdatingRect = false;

        updateScene();
    }
}

//////////////////////////////////////////////////////////////////////////

bool EasyGraphicsView::moveChrono(EasyChronometerItem* _chronometerItem, qreal _mouseX)
{
    if (_chronometerItem->reverse())
    {
        if (_mouseX > _chronometerItem->right())
        {
            _chronometerItem->setReverse(false);
            _chronometerItem->setLeftRight(_chronometerItem->right(), _mouseX);
        }
        else
        {
            _chronometerItem->setLeftRight(_mouseX, _chronometerItem->right());
        }
    }
    else
    {
        if (_mouseX < _chronometerItem->left())
        {
            _chronometerItem->setReverse(true);
            _chronometerItem->setLeftRight(_mouseX, _chronometerItem->left());
        }
        else
        {
            _chronometerItem->setLeftRight(_chronometerItem->left(), _mouseX);
        }
    }

    if (!_chronometerItem->isVisible() && _chronometerItem->width() > 1e-6)
    {
        _chronometerItem->show();
        return true;
    }

    return false;
}

void EasyGraphicsView::mouseMoveEvent(QMouseEvent* _event)
{
    if (m_bEmpty || (m_mouseButtons == 0 && !m_chronometerItem->isVisible()))
    {
        _event->accept();
        return;
    }

    bool needUpdate = false;
    const auto pos = _event->pos();
    const auto delta = pos - m_mousePressPos;
    m_mousePressPos = pos;

    if (m_mouseButtons != 0)
    {
        m_mouseMovePath.setX(m_mouseMovePath.x() + ::std::abs(delta.x()));
        m_mouseMovePath.setY(m_mouseMovePath.y() + ::std::abs(delta.y()));
    }

    auto mouseScenePos = mapToScene(m_mousePressPos);
    mouseScenePos.setX(m_offset + mouseScenePos.x() / m_scale);

    if (m_mouseButtons & Qt::RightButton)
    {
        bool showItem = moveChrono(m_chronometerItem, mouseScenePos.x());
        m_pScrollbar->setChronoPos(m_chronometerItem->left(), m_chronometerItem->right());

        if (showItem)
        {
            m_pScrollbar->showChrono();
        }

        needUpdate = true;
    }

    if (m_mouseButtons & Qt::LeftButton)
    {
        if (m_bDoubleClick)
        {
            moveChrono(m_chronometerItemAux, mouseScenePos.x());
        }
        else
        {
            auto vbar = verticalScrollBar();

            m_bUpdatingRect = true; // Block scrollbars from updating scene rect to make it possible to do it only once
            vbar->setValue(vbar->value() - delta.y());
            m_pScrollbar->setValue(m_pScrollbar->value() - delta.x() / m_scale);
            m_bUpdatingRect = false;
            // Seems like an ugly stub, but QSignalBlocker is also a bad decision
            // because if scrollbar does not emit valueChanged signal then viewport does not move

            updateVisibleSceneRect(); // Update scene visible rect only once

            // Update flicker speed
            m_flickerSpeedX += delta.x() >> 1;
            m_flickerSpeedY += delta.y() >> 1;
            if (!m_flickerTimer.isActive())
            {
                // If flicker timer is not started, then start it
                m_flickerTimer.start(FLICKER_INTERVAL);
            }
        }

        needUpdate = true;
    }

    if (m_chronometerItem->isVisible())
    {
        auto prevValue = m_chronometerItem->hoverIndicator();
        m_chronometerItem->setHover(m_chronometerItem->contains(mouseScenePos));
        needUpdate = needUpdate || (prevValue != m_chronometerItem->hoverIndicator());
    }

    if (needUpdate)
    {
        updateScene(); // repaint scene
    }

    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::resizeEvent(QResizeEvent* _event)
{
    QGraphicsView::resizeEvent(_event);
    updateVisibleSceneRect(); // Update scene visible rect only once
    updateScene(); // repaint scene
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::initMode()
{
    // TODO: find mode with least number of bugs :)
    // There are always some display bugs...

    setCacheMode(QGraphicsView::CacheNone);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &This::onScrollbarValueChange);
    connect(&m_flickerTimer, &QTimer::timeout, this, &This::onFlickerTimeout);

    auto globalSignals = &EASY_GLOBALS.events;
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChange);
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange);
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::itemsExpandStateChanged, this, &This::onItemsEspandStateChange);
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::drawBordersChanged, this, &This::updateScene);
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::chronoPositionChanged, this, &This::updateScene);
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::onScrollbarValueChange(int)
{
    if (!m_bUpdatingRect && !m_bEmpty)
        updateVisibleSceneRect();
}

void EasyGraphicsView::onGraphicsScrollbarValueChange(qreal _value)
{
    if (!m_bEmpty)
    {
        m_offset = _value;
        if (!m_bUpdatingRect)
        {
            updateVisibleSceneRect();
            updateScene();
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::onFlickerTimeout()
{
    if (m_mouseButtons & Qt::LeftButton)
    {
        // Fast slow-down and stop if mouse button is pressed, no flicking.
        m_flickerSpeedX >>= 1;
        m_flickerSpeedY >>= 1;
        if (m_flickerSpeedX == -1) m_flickerSpeedX = 0;
        if (m_flickerSpeedY == -1) m_flickerSpeedY = 0;
    }
    else
    {
        // Flick when mouse button is not pressed

        auto vbar = verticalScrollBar();

        m_bUpdatingRect = true; // Block scrollbars from updating scene rect to make it possible to do it only once
        m_pScrollbar->setValue(m_pScrollbar->value() - m_flickerSpeedX / m_scale);
        vbar->setValue(vbar->value() - m_flickerSpeedY);
        m_bUpdatingRect = false;
        // Seems like an ugly stub, but QSignalBlocker is also a bad decision
        // because if scrollbar does not emit valueChanged signal then viewport does not move

        updateVisibleSceneRect(); // Update scene visible rect only once
        updateScene(); // repaint scene

        m_flickerSpeedX -= absmin(sign(m_flickerSpeedX), m_flickerSpeedX);
        m_flickerSpeedY -= absmin(sign(m_flickerSpeedY), m_flickerSpeedY);
    }

    if (m_flickerSpeedX == 0 && m_flickerSpeedY == 0)
    {
        // Flicker stopped, no timer needed.
        m_flickerTimer.stop();
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::onSelectedThreadChange(::profiler::thread_id_t _id)
{
    if (m_pScrollbar == nullptr || m_pScrollbar->minimapThread() == _id)
    {
        return;
    }

    if (_id == 0)
    {
        m_pScrollbar->setMinimapFrom(0, nullptr);
        return;
    }

    for (auto item : m_items)
    {
        if (item->threadId() == _id)
        {
            m_pScrollbar->setMinimapFrom(_id, item->items(0));
            updateScene();
            return;
        }
    }

    m_pScrollbar->setMinimapFrom(0, nullptr);
    updateScene();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::onSelectedBlockChange(unsigned int _block_index)
{
    if (!m_bUpdatingRect)
    {
        if (_block_index < EASY_GLOBALS.gui_blocks.size())
        {
            // Scroll to item

            const auto& guiblock = EASY_GLOBALS.gui_blocks[_block_index];
            const auto thread_item = m_items[guiblock.graphics_item];
            const auto& item = thread_item->items(guiblock.graphics_item_level)[guiblock.graphics_item_index];

            m_flickerSpeedX = m_flickerSpeedY = 0;

            m_bUpdatingRect = true;
            verticalScrollBar()->setValue(static_cast<int>(thread_item->levelY(guiblock.graphics_item_level) - m_visibleSceneRect.height() * 0.5));
            m_pScrollbar->setValue(item.left() + item.width() * 0.5 - m_pScrollbar->sliderHalfWidth());
            m_bUpdatingRect = false;
        }

        updateVisibleSceneRect();
        updateScene();
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::onItemsEspandStateChange()
{
    if (!m_bUpdatingRect)
    {
        updateScene();
    }
}

//////////////////////////////////////////////////////////////////////////

EasyGraphicsViewWidget::EasyGraphicsViewWidget(QWidget* _parent)
    : QWidget(_parent)
    , m_scrollbar(new EasyGraphicsScrollbar(nullptr))
    , m_view(new EasyGraphicsView(nullptr))
    //, m_threadWidget(new EasyThreadViewWidget(this,m_view))
{
    initWidget();
}

void EasyGraphicsViewWidget::initWidget()
{
    auto lay = new QGridLayout(this);
    lay->setContentsMargins(1, 0, 1, 0);
    lay->addWidget(m_view, 0, 1);
    lay->setSpacing(1);
    lay->addWidget(m_scrollbar, 1, 1);
    //lay->setSpacing(1);
    //lay->addWidget(m_threadWidget, 0, 0);
    setLayout(lay);

    m_view->setScrollbar(m_scrollbar);
}

EasyGraphicsViewWidget::~EasyGraphicsViewWidget()
{

}

EasyGraphicsView* EasyGraphicsViewWidget::view()
{
    return m_view;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

EasyThreadViewWidget::EasyThreadViewWidget(QWidget *parent, EasyGraphicsView* view):QWidget(parent),
    m_view(view)
  , m_label(new QLabel("",this))
{
    m_layout = new QHBoxLayout;
    //QPushButton *button1 = new QPushButton();

    //m_layout->addWidget(m_label);
    //setLayout(m_layout);
    //show();
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChange);
}

EasyThreadViewWidget::~EasyThreadViewWidget()
{

}

void EasyThreadViewWidget::onSelectedThreadChange(::profiler::thread_id_t _id)
{
/*
    auto threadName = EASY_GLOBALS.profiler_blocks[EASY_GLOBALS.selected_thread].thread_name;
    if(threadName[0]!=0)
    {
        m_label->setText(threadName);
    }
    else
    {
        m_label->setText(QString("Thread %1").arg(EASY_GLOBALS.selected_thread));
    }
*/
    QLayoutItem *ditem;
    while ((ditem = m_layout->takeAt(0)))
        delete ditem;

    const auto& items = m_view->getItems();
    for(const auto& item: items)
    {
        m_layout->addWidget(new QLabel(QString("Thread %1").arg(item->threadId())));
        m_layout->setSpacing(1);
    }
    setLayout(m_layout);

}
