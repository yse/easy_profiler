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
#include <QGraphicsProxyWidget>
#include <QFormLayout>
#include <QLabel>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
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

const qreal MIN_SCALE = pow(::profiler_gui::SCALING_COEFFICIENT_INV, 70); // Up to 1000 sec scale
const qreal MAX_SCALE = pow(::profiler_gui::SCALING_COEFFICIENT, 45); // ~23000 --- Up to 10 ns scale
const qreal BASE_SCALE = pow(::profiler_gui::SCALING_COEFFICIENT_INV, 25); // ~0.003

const uint16_t GRAPHICS_ROW_SIZE = 18;
const uint16_t GRAPHICS_ROW_SPACING = 2;
const uint16_t GRAPHICS_ROW_SIZE_FULL = GRAPHICS_ROW_SIZE + GRAPHICS_ROW_SPACING;
const uint16_t THREADS_ROW_SPACING = 8;
const uint16_t TIMELINE_ROW_SIZE = 20;

const QRgb BORDERS_COLOR = ::profiler::colors::Grey700 & 0x00ffffff;// 0x00686868;
const QRgb BACKGROUND_1 = ::profiler::colors::Grey300;
const QRgb BACKGROUND_2 = ::profiler::colors::White;
const QRgb TIMELINE_BACKGROUND = 0x20000000 | (::profiler::colors::Grey800 & 0x00ffffff);// 0x20303030;
//const QRgb SELECTED_ITEM_COLOR = ::profiler::colors::Dark;// 0x000050a0;
const QColor CHRONOMETER_COLOR2 = QColor::fromRgba(0x40000000 | (::profiler::colors::Dark & 0x00ffffff));// 0x40408040);

inline QRgb selectedItemBorderColor(::profiler::color_t _color)
{
    return ::profiler_gui::isLightColor(_color, 192) ? ::profiler::colors::Black : ::profiler::colors::RichRed;
    //return ::profiler::colors::Black;
}

//const unsigned int TEST_PROGRESSION_BASE = 4;

const int IDLE_TIMER_INTERVAL = 200; // 5Hz
const uint64_t IDLE_TIME = 400;

const int FLICKER_INTERVAL = 10; // 100Hz
const qreal FLICKER_FACTOR = 16.0 / FLICKER_INTERVAL;

const auto BG_FONT = ::profiler_gui::EFont("Helvetica", 10, QFont::Bold);
const auto CHRONOMETER_FONT = ::profiler_gui::EFont("Helvetica", 16, QFont::Bold);
const auto ITEMS_FONT = ::profiler_gui::EFont("Helvetica", 10, QFont::Medium);
const auto SELECTED_ITEM_FONT = ::profiler_gui::EFont("Helvetica", 10, QFont::Bold);

#ifdef _WIN32
const qreal FONT_METRICS_FACTOR = 1.05;
#else
const qreal FONT_METRICS_FACTOR = 1.;
#endif

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

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

EasyGraphicsItem::EasyGraphicsItem(uint8_t _index, const::profiler::BlocksTreeRoot& _root)
    : QGraphicsItem(nullptr)
    , m_threadName(_root.got_name() ? QString("%1 Thread %2").arg(_root.name()).arg(_root.thread_id) : QString("Thread %1").arg(_root.thread_id))
    , m_pRoot(&_root)
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

void EasyGraphicsItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const bool gotItems = !m_levels.empty() && !m_levels.front().empty();
    const bool gotSync = !m_pRoot->sync.empty();

    if (!gotItems && !gotSync)
    {
        return;
    }

    const auto sceneView = view();
    const auto visibleSceneRect = sceneView->visibleSceneRect(); // Current visible scene rect
    const auto visibleBottom = visibleSceneRect.bottom() - 1;
    const auto currentScale = sceneView->scale(); // Current GraphicsView scale
    const auto offset = sceneView->offset();
    const auto sceneLeft = offset, sceneRight = offset + visibleSceneRect.width() / currentScale;

    //printf("VISIBLE = {%lf, %lf}\n", sceneLeft, sceneRight);

    QRectF rect;
    QBrush brush;
    QRgb previousColor = 0, inverseColor = 0xffffffff, textColor = 0;
    Qt::PenStyle previousPenStyle = Qt::NoPen;
    brush.setStyle(Qt::SolidPattern);

    _painter->save();
    _painter->setFont(ITEMS_FONT);
    
    // Reset indices of first visible item for each layer
    const auto levelsNumber = levels();
    for (uint8_t i = 1; i < levelsNumber; ++i) ::profiler_gui::set_max(m_levelsIndexes[i]);


    // Search for first visible top-level item
    if (gotItems)
    {
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
    }



    // This is to make _painter->drawText() work properly
    // (it seems there is a bug in Qt5.6 when drawText called for big coordinates,
    // drawRect at the same time called for actually same coordinates
    // works fine without using this additional shifting)
    //const auto dx = level0[m_levelsIndexes[0]].left() * currentScale;
    const auto dx = offset * currentScale;



    // Shifting coordinates to current screen offset
    //_painter->setTransform(QTransform::fromTranslate(dx - offset * currentScale, -y()), true);
    _painter->setTransform(QTransform::fromTranslate(0, -y()), true);


    if (EASY_GLOBALS.draw_graphics_items_borders)
    {
        previousPenStyle = Qt::SolidLine;
        _painter->setPen(BORDERS_COLOR);
    }
    else
    {
        _painter->setPen(Qt::NoPen);
    }



    // Iterate through layers and draw visible items
    if (gotItems)
    {
        static const auto MAX_CHILD_INDEX = ::profiler_gui::numeric_max<decltype(::profiler_gui::EasyBlockItem::children_begin)>();
        auto const skip_children = [this, &levelsNumber](short next_level, decltype(::profiler_gui::EasyBlockItem::children_begin) children_begin)
        {
            // Mark that we would not paint children of current item
            if (next_level < levelsNumber && children_begin != MAX_CHILD_INDEX)
                m_levels[next_level][children_begin].state = BLOCK_ITEM_DO_NOT_PAINT;
        };

        bool selectedItemsWasPainted = false;
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
                const auto& itemDesc = easyDescriptor(itemBlock.tree.node->id());

                int h = 0, flags = 0;
                if (w < 20 || !itemBlock.expanded)
                {
                    // Items which width is less than 20 will be painted as big rectangles which are hiding it's children

                    //x = item.left() * currentScale - dx;
                    h = item.totalHeight;
                    const auto dh = top + h - visibleBottom;
                    if (dh > 0)
                        h -= dh;

                    if (item.block == EASY_GLOBALS.selected_block)
                        selectedItemsWasPainted = true;

                    const bool colorChange = (previousColor != itemDesc.color());
                    if (colorChange)
                    {
                        // Set background color brush for rectangle
                        previousColor = itemDesc.color();
                        inverseColor = 0xffffffff - previousColor;
                        textColor = ::profiler_gui::textColorForRgb(previousColor);
                        brush.setColor(previousColor);
                        _painter->setBrush(brush);
                    }

                    if (EASY_GLOBALS.draw_graphics_items_borders && (previousPenStyle != Qt::SolidLine || colorChange))
                    {
                        // Restore pen for item which is wide enough to paint borders
                        previousPenStyle = Qt::SolidLine;
                        _painter->setPen(BORDERS_COLOR & inverseColor);// BORDERS_COLOR);
                    }

                    if (w < 2) w = 2;

                    // Draw rectangle
                    rect.setRect(x, top, w, h);
                    _painter->drawRect(rect);

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
                        selectedItemsWasPainted = true;

                    const bool colorChange = (previousColor != itemDesc.color());
                    if (colorChange)
                    {
                        // Set background color brush for rectangle
                        previousColor = itemDesc.color();
                        inverseColor = 0xffffffff - previousColor;
                        textColor = ::profiler_gui::textColorForRgb(previousColor);
                        brush.setColor(previousColor);
                        _painter->setBrush(brush);
                    }

                    if (EASY_GLOBALS.draw_graphics_items_borders && (previousPenStyle != Qt::SolidLine || colorChange))
                    {
                        // Restore pen for item which is wide enough to paint borders
                        previousPenStyle = Qt::SolidLine;
                        _painter->setPen(BORDERS_COLOR & inverseColor);// BORDERS_COLOR);
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
                //auto textColor = inverseColor < 0x00808080 ? profiler::colors::Black : profiler::colors::White;
                //if (textColor == previousColor) textColor = 0;
                _painter->setPen(QColor::fromRgb(textColor));

                if (item.block == EASY_GLOBALS.selected_block)
                    _painter->setFont(SELECTED_ITEM_FONT);

                // drawing text
                auto name = *itemBlock.tree.node->name() != 0 ? itemBlock.tree.node->name() : itemDesc.name();
                _painter->drawText(rect, flags, ::profiler_gui::toUnicode(name));

                // restore previous pen color
                if (previousPenStyle == Qt::NoPen)
                    _painter->setPen(Qt::NoPen);
                else
                    _painter->setPen(BORDERS_COLOR & inverseColor);// BORDERS_COLOR); // restore pen for rectangle painting

                // restore font
                if (item.block == EASY_GLOBALS.selected_block)
                    _painter->setFont(ITEMS_FONT);
                // END Draw text~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
            }
        }

        if (EASY_GLOBALS.selected_block < EASY_GLOBALS.gui_blocks.size())
        {
            const auto& guiblock = EASY_GLOBALS.gui_blocks[EASY_GLOBALS.selected_block];
            if (guiblock.graphics_item == m_index)
            {
                const auto& item = m_levels[guiblock.graphics_item_level][guiblock.graphics_item_index];
                if (item.left() < sceneRight && item.right() > sceneLeft)
                {
                    const auto& itemBlock = easyBlock(item.block);
                    auto top = levelY(guiblock.graphics_item_level);
                    auto w = ::std::max(item.width() * currentScale, 1.0);
                    decltype(top) h = (selectedItemsWasPainted && itemBlock.expanded && w > 20) ? GRAPHICS_ROW_SIZE : item.totalHeight;

                    auto dh = top + h - visibleBottom;
                    if (dh < h)
                    {
                        if (dh > 0)
                            h -= dh;

                        const auto& itemDesc = easyDescriptor(itemBlock.tree.node->id());

                        QPen pen(Qt::SolidLine);
                        pen.setJoinStyle(Qt::MiterJoin);
                        pen.setColor(selectedItemBorderColor(itemDesc.color()));//Qt::red);
                        pen.setWidth(3);
                        _painter->setPen(pen);

                        if (!selectedItemsWasPainted)
                        {
                            brush.setColor(itemDesc.color());// SELECTED_ITEM_COLOR);
                            _painter->setBrush(brush);
                        }
                        else
                        {
                            _painter->setBrush(Qt::NoBrush);
                        }

                        auto x = item.left() * currentScale - dx;
                        rect.setRect(x, top, w, h);
                        _painter->drawRect(rect);

                        if (!selectedItemsWasPainted && w > 20)
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

                            rect.setRect(xtext + 1, top, w - 1, h);

                            // text will be painted with inverse color
                            //auto textColor = 0x00ffffff - previousColor;
                            //if (textColor == previousColor) textColor = 0;
                            textColor = ::profiler_gui::textColorForRgb(itemDesc.color());// SELECTED_ITEM_COLOR);
                            _painter->setPen(textColor);

                            _painter->setFont(SELECTED_ITEM_FONT);

                            // drawing text
                            auto name = *itemBlock.tree.node->name() != 0 ? itemBlock.tree.node->name() : easyDescriptor(itemBlock.tree.node->id()).name();
                            _painter->drawText(rect, Qt::AlignCenter, ::profiler_gui::toUnicode(name));
                            // END Draw text~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
                        }
                    }
                }
            }
        }
    }



    if (gotSync)
    {
        auto firstSync = ::std::lower_bound(m_pRoot->sync.begin(), m_pRoot->sync.end(), sceneLeft, [&sceneView](::profiler::block_index_t _index, qreal _value)
        {
            return sceneView->time2position(blocksTree(_index).node->begin()) < _value;
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
        //firstSync = m_pRoot->sync.begin();

        previousColor = 0;
        qreal prevRight = -1e100, top = y() - 4, h = 3;
        if (top + h < visibleBottom)
        {
            for (auto it = firstSync, end = m_pRoot->sync.end(); it != end; ++it)
            {
                const auto& item = blocksTree(*it);
                auto left = sceneView->time2position(item.node->begin());

                if (left > sceneRight)
                    break; // This is first totally invisible item. No need to check other items.

                decltype(left) width = sceneView->time2position(item.node->end()) - left;
                if (left + width < sceneLeft) // This item is not visible
                    continue;

                left *= currentScale;
                left -= dx;
                width *= currentScale;
                if (left + width <= prevRight) // This item is not visible
                    continue;

                if (left < prevRight)
                {
                    width -= prevRight - left;
                    left = prevRight;
                }

                if (width < 2)
                    width = 2;

                const bool self_thread = item.node->id() != 0 && EASY_GLOBALS.profiler_blocks.find(item.node->id()) != EASY_GLOBALS.profiler_blocks.end();
                ::profiler::color_t color = 0;
                if (self_thread)
                    color = ::profiler::colors::Coral;
                else if (item.node->id() == 0)
                    color = ::profiler::colors::Black;
                else
                    color = ::profiler::colors::RedA400;

                if (previousColor != color)
                {
                    previousColor = color;
                    _painter->setBrush(QColor::fromRgb(color));
                    if (color != ::profiler::colors::Black)
                        _painter->setPen(QColor::fromRgb(0x00808080 & color));
                    else
                        _painter->setPen(QColor::fromRgb(::profiler::colors::Grey800));
                }

                rect.setRect(left, top, width, h);
                _painter->drawRect(rect);
                prevRight = left + width;
            }
        }
    }


    _painter->restore();
}

//////////////////////////////////////////////////////////////////////////

const ::profiler::BlocksTreeRoot* EasyGraphicsItem::root() const
{
    return m_pRoot;
}

const QString& EasyGraphicsItem::threadName() const
{
    return m_threadName;
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

const ::profiler_gui::EasyBlock* EasyGraphicsItem::intersectEvent(const QPointF& _pos) const
{
    if (m_pRoot->sync.empty())
    {
        return nullptr;
    }

    const auto top = y() - 6;

    if (top > _pos.y())
    {
        return nullptr;
    }

    const auto bottom = top + 5;
    if (bottom < _pos.y())
    {
        return nullptr;
    }

    const auto sceneView = view();
    const auto currentScale = view()->scale();
    auto firstSync = ::std::lower_bound(m_pRoot->sync.begin(), m_pRoot->sync.end(), _pos.x(), [&sceneView](::profiler::block_index_t _index, qreal _value)
    {
        return sceneView->time2position(blocksTree(_index).node->begin()) < _value;
    });

    if (firstSync == m_pRoot->sync.end())
        firstSync = m_pRoot->sync.begin() + m_pRoot->sync.size() - 1;
    else if (firstSync != m_pRoot->sync.begin())
        --firstSync;

    for (auto it = firstSync, end = m_pRoot->sync.end(); it != end; ++it)
    {
        const auto& item = easyBlock(*it);

        const auto left = sceneView->time2position(item.tree.node->begin()) - 2;
        if (left > _pos.x())
            break;
        
        const auto right = sceneView->time2position(item.tree.node->end()) + 2;
        if (right < _pos.x())
            continue;

        return &item;
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

void EasyChronometerItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    auto const sceneView = view();
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
    const auto textRect = QFontMetricsF(CHRONOMETER_FONT, sceneView).boundingRect(text); // Calculate displayed text boundingRect
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
    g.setColorAt(0.2, QColor::fromRgba(0x14000000 | rgb));
    g.setColorAt(0.8, QColor::fromRgba(0x14000000 | rgb));
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
    _painter->setRenderHint(QPainter::TextAntialiasing);
    _painter->setPen(0x00ffffff - rgb);
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

    const auto textRect_width = textRect.width() * FONT_METRICS_FACTOR;
    if (textRect_width < rect.width())
    {
        // Text will be drawed inside rectangle
        _painter->drawText(rect, textFlags, text);
        _painter->restore();
        return;
    }

    const auto w = textRect_width / currentScale;
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

EasyGraphicsView* EasyChronometerItem::view()
{
    return static_cast<EasyGraphicsView*>(scene()->parent());
}

//////////////////////////////////////////////////////////////////////////

void EasyBackgroundItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    auto const sceneView = static_cast<EasyGraphicsView*>(scene()->parent());
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
        static const uint16_t OVERLAP = THREADS_ROW_SPACING >> 1;
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
    const auto textWidth = QFontMetricsF(_painter->font(), sceneView).width(QString::number(static_cast<quint64>(0.5 + first_x * factor))) * FONT_METRICS_FACTOR + 10;
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

void EasyTimelineIndicatorItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const auto sceneView = static_cast<const EasyGraphicsView*>(scene()->parent());
    const auto visibleSceneRect = sceneView->visibleSceneRect();
    const auto step = sceneView->timelineStep() * sceneView->scale();
    const QString text = ::profiler_gui::timeStringInt(units2microseconds(sceneView->timelineStep())); // Displayed text

    // Draw scale indicator
    _painter->save();
    _painter->setTransform(QTransform::fromTranslate(-x(), -y()));
    //_painter->setCompositionMode(QPainter::CompositionMode_Difference);
    _painter->setBrush(Qt::NoBrush);

    QPen pen(Qt::black);
    pen.setWidth(2);
    pen.setJoinStyle(Qt::MiterJoin);
    _painter->setPen(pen);

    QRectF rect(visibleSceneRect.width() - 10 - step, visibleSceneRect.height() - 20, step, 10);
    const auto rect_right = rect.right();
    const QPointF points[] = {{rect.left(), rect.bottom()}, {rect.left(), rect.top()}, {rect_right, rect.top()}, {rect_right, rect.top() + 5}};
    _painter->drawPolyline(points, sizeof(points) / sizeof(QPointF));

    rect.translate(0, 3);
    _painter->drawText(rect, Qt::AlignRight | Qt::TextDontClip, text);

    _painter->restore();
}

//////////////////////////////////////////////////////////////////////////

EasyGraphicsView::EasyGraphicsView(QWidget* _parent)
    : Parent(_parent)
    , m_beginTime(::std::numeric_limits<decltype(m_beginTime)>::max())
    , m_scale(1)
    , m_offset(0)
    , m_timelineStep(0)
    , m_idleTime(0)
    , m_mouseButtons(Qt::NoButton)
    , m_pScrollbar(nullptr)
    , m_chronometerItem(nullptr)
    , m_chronometerItemAux(nullptr)
    , m_csInfoWidget(nullptr)
    , m_flickerSpeedX(0)
    , m_flickerSpeedY(0)
    , m_flickerCounterX(0)
    , m_flickerCounterY(0)
    , m_bDoubleClick(false)
    , m_bUpdatingRect(false)
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

void EasyGraphicsView::clearSilent()
{
    const QSignalBlocker blocker(this), sceneBlocker(scene()); // block all scene signals (otherwise clear() would be extremely slow!)

    // Stop flicking
    m_flickerTimer.stop();
    m_flickerSpeedX = 0;
    m_flickerSpeedY = 0;
    m_flickerCounterX = 0;
    m_flickerCounterY = 0;

    if (m_csInfoWidget != nullptr)
    {
        auto widget = m_csInfoWidget->widget();
        widget->setParent(nullptr);
        m_csInfoWidget->setWidget(nullptr);
        delete widget;
        m_csInfoWidget = nullptr;
    }

    // Clear all items
    scene()->clear();
    m_items.clear();
    m_selectedBlocks.clear();

    m_beginTime = ::std::numeric_limits<decltype(m_beginTime)>::max(); // reset begin time
    m_scale = 1; // scale back to initial 100% scale
    m_timelineStep = 1;
    m_offset = 0; // scroll back to the beginning of the scene

    m_idleTimer.stop();
    m_idleTime = 0;

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
    ::profiler::thread_id_t longestTree = 0, mainTree = 0;
    for (const auto& threadTree : _blocksTree)
    {
        const auto& t = threadTree.second;

        auto timestart = m_beginTime;
        auto timefinish = finish;
        
        if (!t.children.empty())
            timestart = blocksTree(t.children.front()).node->begin();
        if (!t.sync.empty())
            timestart = ::std::min(timestart, blocksTree(t.sync.front()).node->begin());

        if (!t.children.empty())
            timefinish = blocksTree(t.children.back()).node->end();
        if (!t.sync.empty())
            timefinish = ::std::max(timefinish, blocksTree(t.sync.back()).node->end());

        if (m_beginTime > timestart)
            m_beginTime = timestart;

        if (finish < timefinish) {
            finish = timefinish;
            longestTree = threadTree.first;
        }

        if (mainTree == 0 && !strcmp(t.name(), "Main"))
            mainTree = threadTree.first;
    }

    // Filling scene with items
    m_items.reserve(_blocksTree.size());
    qreal y = TIMELINE_ROW_SIZE;
    const EasyGraphicsItem *longestItem = nullptr, *mainThreadItem = nullptr;
    for (const auto& threadTree : _blocksTree)
    {
        if (m_items.size() == 0xff)
        {
            qWarning() << "Warning: Maximum threads number (255 threads) exceeded! See EasyGraphicsView::setTree() : " << __LINE__ << " in file " << __FILE__;
            break;
        }

        const auto& t = threadTree.second;

        // fill scene with new items
        qreal h = 0, x = 0;
        
        if (!t.children.empty())
            x = time2position(blocksTree(t.children.front()).node->begin());
        else if (!t.sync.empty())
            x = time2position(blocksTree(t.sync.front()).node->begin());

        auto item = new EasyGraphicsItem(static_cast<uint8_t>(m_items.size()), t);
        if (t.depth)
            item->setLevels(t.depth);
        item->setPos(0, y);

        qreal children_duration = 0;

        if (!t.children.empty())
        {
            children_duration = setTree(item, t.children, h, y, 0);
        }
        else if (!t.sync.empty())
        {
            children_duration = time2position(blocksTree(t.sync.back()).node->end()) - x;
            h = GRAPHICS_ROW_SIZE;
        }

        item->setBoundingRect(0, 0, children_duration + x, h);
        m_items.push_back(item);
        scene()->addItem(item);

        y += h + THREADS_ROW_SPACING;

        if (longestTree == threadTree.first)
            longestItem = item;

        if (mainTree == threadTree.first)
            mainThreadItem = item;
    }

    // Calculating scene rect
    const qreal endX = time2position(finish) + 1500.0;
    scene()->setSceneRect(0, 0, endX, y + TIMELINE_ROW_SIZE);

    // Center view on the beginning of the scene
    updateVisibleSceneRect();
    setScrollbar(m_pScrollbar);

    // Create new chronometer item (previous item was destroyed by scene on scene()->clear()).
    // It will be shown on mouse right button click.
    m_chronometerItemAux = createChronometer(false);
    m_chronometerItem = createChronometer(true);

    bgItem->setBoundingRect(0, 0, endX, y);
    auto indicator = new EasyTimelineIndicatorItem();
    indicator->setBoundingRect(0, 0, endX, y);
    scene()->addItem(indicator);

    // Setting flags
    m_bEmpty = false;

    scaleTo(BASE_SCALE);


    emit treeChanged();

    if (mainThreadItem != nullptr)
    {
        longestItem = mainThreadItem;
    }

    if (longestItem != nullptr)
    {
        m_pScrollbar->setMinimapFrom(longestItem->threadId(), longestItem->items(0));
        EASY_GLOBALS.selected_thread = longestItem->threadId();
        emit EASY_GLOBALS.events.selectedThreadChanged(longestItem->threadId());
    }

    m_idleTimer.start(IDLE_TIMER_INTERVAL);
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

        b.block = child_index;// &child;
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

void EasyGraphicsView::repaintScene()
{
    scene()->update(m_visibleSceneRect);
    emit sceneUpdated();
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
    repaintScene();
}

void EasyGraphicsView::wheelEvent(QWheelEvent* _event)
{
    m_idleTime = 0;

    if (!m_bEmpty)
        onWheel(mapToScene(_event->pos()).x(), _event->delta());
    _event->accept();
}

void EasyGraphicsView::onGraphicsScrollbarWheel(qreal _mouseX, int _wheelDelta)
{
    m_idleTime = 0;

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
    _mouseX = clamp(0., _mouseX, scene()->width());
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
    repaintScene(); // repaint scene
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::mousePressEvent(QMouseEvent* _event)
{
    m_idleTime = 0;

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
    m_idleTime = 0;

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
        emit sceneUpdated();
    }

    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::mouseReleaseEvent(QMouseEvent* _event)
{
    m_idleTime = 0;

    if (m_bEmpty)
    {
        _event->accept();
        return;
    }

    bool chronoHidden = false;
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
            chronoHidden = true;
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
            if (mouseClickPos.x() >= 0)
            {
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

        repaintScene();
    }
    else if (chronoHidden)
    {
        emit sceneUpdated();
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
    m_idleTime = 0;

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
    const auto x = clamp(0., mouseScenePos.x(), scene()->width());

    if (m_mouseButtons & Qt::RightButton)
    {
        bool showItem = moveChrono(m_chronometerItem, x);
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
            moveChrono(m_chronometerItemAux, x);
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
            m_flickerSpeedY += delta.y();
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
        repaintScene(); // repaint scene
    }

    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::keyPressEvent(QKeyEvent* _event)
{
    static const int KeyStep = 100;

    const int key = _event->key();
    m_idleTime = 0;

    switch (key)
    {
        case Qt::Key_Right:
        case Qt::Key_6:
        {
            m_pScrollbar->setValue(m_pScrollbar->value() + KeyStep / m_scale);
            break;
        }

        case Qt::Key_Left:
        case Qt::Key_4:
        {
            m_pScrollbar->setValue(m_pScrollbar->value() - KeyStep / m_scale);
            break;
        }

        case Qt::Key_Up:
        case Qt::Key_8:
        {
            auto vbar = verticalScrollBar();
            vbar->setValue(vbar->value() - KeyStep);
            break;
        }

        case Qt::Key_Down:
        case Qt::Key_2:
        {
            auto vbar = verticalScrollBar();
            vbar->setValue(vbar->value() + KeyStep);
            break;
        }

        case Qt::Key_Plus:
        case Qt::Key_Equal:
        {
            onWheel(mapToScene(mapFromGlobal(QCursor::pos())).x(), KeyStep);
            break;
        }

        case Qt::Key_Minus:
        {
            onWheel(mapToScene(mapFromGlobal(QCursor::pos())).x(), -KeyStep);
            break;
        }
    }

    m_keys.insert(key);
    _event->accept();
}

void EasyGraphicsView::keyReleaseEvent(QKeyEvent* _event)
{
    const int key = _event->key();
    m_idleTime = 0;

    m_keys.erase(key);
    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::resizeEvent(QResizeEvent* _event)
{
    Parent::resizeEvent(_event);

    const QRectF previousRect = m_visibleSceneRect;
    updateVisibleSceneRect(); // Update scene visible rect only once    

    // Update slider width for scrollbar
    const auto windowWidth = m_visibleSceneRect.width() / m_scale;
    m_pScrollbar->setSliderWidth(windowWidth);

    // Calculate new offset to save old screen center
    const auto deltaWidth = m_visibleSceneRect.width() - previousRect.width();
    m_offset = clamp(0., m_offset - deltaWidth * 0.5 / m_scale, scene()->width() - windowWidth);

    // Update slider position
    m_bUpdatingRect = true; // To be sure that updateVisibleSceneRect will not be called by scrollbar change
    m_pScrollbar->setValue(m_offset);
    m_bUpdatingRect = false;

    repaintScene(); // repaint scene
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
    connect(&m_idleTimer, &QTimer::timeout, this, &This::onIdleTimeout);

    auto globalSignals = &EASY_GLOBALS.events;
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChange);
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange);
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::itemsExpandStateChanged, this, &This::onItemsEspandStateChange);
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::drawBordersChanged, this, &This::repaintScene);
    connect(globalSignals, &::profiler_gui::EasyGlobalSignals::chronoPositionChanged, this, &This::repaintScene);
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
            repaintScene();
        }
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::onFlickerTimeout()
{
    ++m_flickerCounterX;
    ++m_flickerCounterY;

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
        repaintScene(); // repaint scene

        const int dx = static_cast<int>(sign(m_flickerSpeedX) * m_flickerCounterX / FLICKER_FACTOR);
        const int dy = static_cast<int>(sign(m_flickerSpeedY) * m_flickerCounterY / FLICKER_FACTOR);

        if (abs(dx) > 0)
        {
            m_flickerSpeedX -= absmin(dx, m_flickerSpeedX);
            m_flickerCounterX = 0;
        }

        if (abs(dy) > 0)
        {
            m_flickerSpeedY -= absmin(dy, m_flickerSpeedY);
            m_flickerCounterY = 0;
        }
    }

    if (m_flickerSpeedX == 0 && m_flickerSpeedY == 0)
    {
        // Flicker stopped, no timer needed.
        m_flickerTimer.stop();
        m_flickerSpeedX = 0;
        m_flickerSpeedY = 0;
        m_flickerCounterX = 0;
        m_flickerCounterY = 0;
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::onIdleTimeout()
{
    m_idleTime += IDLE_TIMER_INTERVAL;

    if (m_idleTime > IDLE_TIME)
    {
        if (m_csInfoWidget != nullptr)
            return;

        auto scenePos = mapToScene(mapFromGlobal(QCursor::pos()));
        decltype(scenePos) pos(m_offset + scenePos.x() / m_scale, scenePos.y());

        // Try to select one of context switches or items
        for (auto item : m_items)
        {
            auto block = item->intersect(pos);
            if (block)
            {
                const auto& itemBlock = blocksTree(block->block);
                auto name = *itemBlock.node->name() != 0 ? itemBlock.node->name() : easyDescriptor(itemBlock.node->id()).name();

                auto widget = new QWidget();
                auto lay = new QFormLayout(widget);
                lay->setLabelAlignment(Qt::AlignRight);
                lay->addRow("Name:", new QLabel(name));
                lay->addRow("Duration:", new QLabel(::profiler_gui::timeStringReal(PROF_MICROSECONDS(itemBlock.node->duration()), 3)));
                if (itemBlock.per_thread_stats)
                {
                    lay->addRow("%/Thread:", new QLabel(QString::number(::profiler_gui::percent(itemBlock.per_thread_stats->total_duration, item->root()->active_time))));
                    lay->addRow("N/Thread:", new QLabel(QString::number(itemBlock.per_thread_stats->calls_number)));
                    if (itemBlock.per_parent_stats->parent_block == item->threadId())
                    {
                        auto percent = ::profiler_gui::percentReal(itemBlock.node->duration(), item->root()->active_time);
                        lay->addRow("%:", new QLabel(percent < 0.5001 ? QString::number(percent, 'f', 2) : QString::number(static_cast<int>(0.5 + percent))));
                    }
                    else
                    {
                        auto percent = ::profiler_gui::percentReal(itemBlock.node->duration(), blocksTree(itemBlock.per_parent_stats->parent_block).node->duration());
                        lay->addRow("%:", new QLabel(percent < 0.5001 ? QString::number(percent, 'f', 2) : QString::number(static_cast<int>(0.5 + percent))));
                    }
                }

                m_csInfoWidget = new QGraphicsProxyWidget();
                m_csInfoWidget->setWidget(widget);

                break;
            }

            auto cse = item->intersectEvent(pos);
            if (cse)
            {
                auto widget = new QWidget();
                auto lay = new QFormLayout(widget);
                lay->setLabelAlignment(Qt::AlignRight);

                auto it = EASY_GLOBALS.profiler_blocks.find(cse->tree.node->id());
                if (it != EASY_GLOBALS.profiler_blocks.end())
                    lay->addRow("Thread:", new QLabel(QString("%1 %2").arg(cse->tree.node->id()).arg(it->second.name())));
                else
                    lay->addRow("Thread:", new QLabel(QString::number(cse->tree.node->id())));
                lay->addRow("Process:", new QLabel(cse->tree.node->name()));
                lay->addRow("Duration:", new QLabel(::profiler_gui::timeStringReal(PROF_MICROSECONDS(cse->tree.node->duration()), 3)));

                m_csInfoWidget = new QGraphicsProxyWidget();
                m_csInfoWidget->setWidget(widget);

                break;
            }
        }

        if (m_csInfoWidget != nullptr)
        {
            scene()->addItem(m_csInfoWidget);

            auto br = m_csInfoWidget->boundingRect();
            if (scenePos.y() + br.height() > m_visibleSceneRect.bottom())
                scenePos.setY(scenePos.y() - br.height());
            if (scenePos.x() + br.width() > m_visibleSceneRect.right())
                scenePos.setX(scenePos.x() - br.width());

            m_csInfoWidget->setPos(scenePos);
            m_csInfoWidget->setOpacity(0.85);
        }
    }
    else if (m_csInfoWidget != nullptr)
    {
        auto widget = m_csInfoWidget->widget();
        widget->setParent(nullptr);
        m_csInfoWidget->setWidget(nullptr);
        delete widget;
        scene()->removeItem(m_csInfoWidget);
        m_csInfoWidget = nullptr;
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
            repaintScene();
            return;
        }
    }

    m_pScrollbar->setMinimapFrom(0, nullptr);
    repaintScene();
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
        repaintScene();
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsView::onItemsEspandStateChange()
{
    if (!m_bUpdatingRect)
    {
        repaintScene();
    }
}

//////////////////////////////////////////////////////////////////////////

EasyGraphicsViewWidget::EasyGraphicsViewWidget(QWidget* _parent)
    : QWidget(_parent)
    , m_scrollbar(new EasyGraphicsScrollbar(this))
    , m_view(new EasyGraphicsView(this))
    , m_threadNamesWidget(new EasyThreadNamesWidget(m_view, m_scrollbar->height(), this))
{
    initWidget();
}

void EasyGraphicsViewWidget::initWidget()
{
    auto lay = new QGridLayout(this);
    lay->setContentsMargins(1, 0, 1, 0);
    lay->addWidget(m_threadNamesWidget, 0, 0, 2, 1);
    lay->setSpacing(1);
    lay->addWidget(m_view, 0, 1);
    lay->setSpacing(1);
    lay->addWidget(m_scrollbar, 1, 1);
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

EasyThreadNameItem::EasyThreadNameItem() : QGraphicsItem(nullptr)
{

}

EasyThreadNameItem::~EasyThreadNameItem()
{

}

QRectF EasyThreadNameItem::boundingRect() const
{
    return m_boundingRect;
}

void EasyThreadNameItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    auto const parentView = static_cast<EasyThreadNamesWidget*>(scene()->parent());
    const auto view = parentView->view();
    const auto& items = view->getItems();
    if (items.empty())
        return;

    const auto visibleSceneRect = view->visibleSceneRect();
    const auto h = visibleSceneRect.height() + TIMELINE_ROW_SIZE - 2;
    const auto w = scene()->sceneRect().width();

    static const uint16_t OVERLAP = THREADS_ROW_SPACING >> 1;
    static const QBrush brushes[2] = {QColor::fromRgb(BACKGROUND_1), QColor::fromRgb(BACKGROUND_2)};
    int i = -1;

    QRectF rect;

    _painter->resetTransform();

    // Draw thread names
    _painter->setFont(BG_FONT);
    for (auto item : items)
    {
        ++i;

        auto br = item->boundingRect();
        auto top = item->y() + br.top() - visibleSceneRect.top() - OVERLAP;
        auto hgt = br.height() + THREADS_ROW_SPACING;
        auto bottom = top + hgt;

        if (top > h || bottom < 0)
            continue;

        if (item->threadId() == EASY_GLOBALS.selected_thread)
            _painter->setBrush(QBrush(QColor::fromRgb(::profiler_gui::SELECTED_THREAD_BACKGROUND)));
        else
            _painter->setBrush(brushes[i & 1]);

        if (top < 0)
        {
            hgt += top;
            top = 0;
        }

        const auto dh = top + hgt - h;
        if (dh > 0)
            hgt -= dh;

        rect.setRect(0, top, w, hgt);

        _painter->setPen(Qt::NoPen);
        _painter->drawRect(rect);

        rect.translate(-5, 0);
        _painter->setPen(QColor::fromRgb(::profiler::colors::Dark));
        _painter->drawText(rect, Qt::AlignRight | Qt::AlignVCenter, item->threadName());
    }

    // Draw separator between thread names area and information area
    _painter->setPen(Qt::darkGray);
    _painter->drawLine(QLineF(0, h, w, h));
    _painter->drawLine(QLineF(0, h + 2, w, h + 2));

    // Draw information
    _painter->setFont(CHRONOMETER_FONT);
    QFontMetricsF fm(CHRONOMETER_FONT, parentView);
    const qreal time1 = view->chronoTime();
    const qreal time2 = view->chronoTimeAux();

    auto y = h + 2;

    auto drawTimeText = [&rect, &w, &y, &fm, &_painter](qreal time, QRgb color)
    {
        if (time > 0)
        {
            const QString text = ::profiler_gui::timeStringReal(time); // Displayed text
            const auto th = fm.height(); // Calculate displayed text height
            rect.setRect(0, y, w, th);

            _painter->setPen(color);
            _painter->drawText(rect, Qt::AlignCenter, text);

            y += th;
        }
    };

    drawTimeText(time1, CHRONOMETER_COLOR.rgb() & 0x00ffffff);
    drawTimeText(time2, CHRONOMETER_COLOR2.rgb() & 0x00ffffff);
}

void EasyThreadNameItem::setBoundingRect(const QRectF& _rect)
{
    m_boundingRect = _rect;
}

//////////////////////////////////////////////////////////////////////////

EasyThreadNamesWidget::EasyThreadNamesWidget(EasyGraphicsView* _view, int _additionalHeight, QWidget* _parent)
    : Parent(_parent)
    , m_view(_view)
    , m_additionalHeight(_additionalHeight + 1)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);

    setScene(new QGraphicsScene(this));

    setCacheMode(QGraphicsView::CacheNone);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFixedWidth(100);

    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChange);
    connect(m_view, &EasyGraphicsView::treeChanged, this, &This::onTreeChange);
    connect(m_view, &EasyGraphicsView::sceneUpdated, this, &This::repaintScene);
    connect(m_view->verticalScrollBar(), &QScrollBar::valueChanged, verticalScrollBar(), &QScrollBar::setValue);
    connect(m_view->verticalScrollBar(), &QScrollBar::rangeChanged, this, &This::setVerticalScrollbarRange);
}

EasyThreadNamesWidget::~EasyThreadNamesWidget()
{

}

void EasyThreadNamesWidget::setVerticalScrollbarRange(int _minValue, int _maxValue)
{
    verticalScrollBar()->setRange(_minValue, _maxValue + m_additionalHeight);
}

void EasyThreadNamesWidget::onTreeChange()
{
    const QSignalBlocker b(this);
    scene()->clear();

    QFontMetricsF fm(BG_FONT, this);
    qreal maxLength = 0;
    const auto& graphicsItems = m_view->getItems();
    for (auto graphicsItem : graphicsItems)
        maxLength = ::std::max(maxLength, fm.width(graphicsItem->threadName()) * FONT_METRICS_FACTOR);

    auto vbar = verticalScrollBar();
    auto viewBar = m_view->verticalScrollBar();

    setVerticalScrollbarRange(viewBar->minimum(), viewBar->maximum());
    vbar->setSingleStep(viewBar->singleStep());
    vbar->setPageStep(viewBar->pageStep());

    auto r = m_view->sceneRect();
    setSceneRect(0, r.top(), maxLength, r.height() + m_additionalHeight);

    auto item = new EasyThreadNameItem();
    item->setPos(0, 0);
    item->setBoundingRect(sceneRect());
    scene()->addItem(item);

    setFixedWidth(maxLength);
}

void EasyThreadNamesWidget::onSelectedThreadChange(::profiler::thread_id_t)
{
    scene()->update();
}

void EasyThreadNamesWidget::repaintScene()
{
    scene()->update();
}

void EasyThreadNamesWidget::mousePressEvent(QMouseEvent* _event)
{
    QMouseEvent e(_event->type(), _event->pos() - QPointF(sceneRect().width(), 0), _event->button(), _event->buttons() & ~Qt::RightButton, _event->modifiers());
    m_view->mousePressEvent(&e);
    _event->accept();
}

void EasyThreadNamesWidget::mouseDoubleClickEvent(QMouseEvent* _event)
{
    static const auto OVERLAP = THREADS_ROW_SPACING >> 1;

    auto y = mapToScene(_event->pos()).y();
    const auto& items = m_view->getItems();
    for (auto item : items)
    {
        auto br = item->boundingRect();
        auto top = item->y() + br.top() - OVERLAP;
        auto bottom = top + br.height() + OVERLAP;

        if (y < top || y > bottom)
            continue;

        const auto thread_id = item->threadId();
        if (thread_id != EASY_GLOBALS.selected_thread)
        {
            EASY_GLOBALS.selected_thread = thread_id;
            emit EASY_GLOBALS.events.selectedThreadChanged(thread_id);
        }

        break;
    }

    _event->accept();
}

void EasyThreadNamesWidget::mouseReleaseEvent(QMouseEvent* _event)
{
    QMouseEvent e(_event->type(), _event->pos() - QPointF(sceneRect().width(), 0), _event->button(), _event->buttons() & ~Qt::RightButton, _event->modifiers());
    m_view->mouseReleaseEvent(&e);
    _event->accept();
}

void EasyThreadNamesWidget::mouseMoveEvent(QMouseEvent* _event)
{
    QMouseEvent e(_event->type(), _event->pos() - QPointF(sceneRect().width(), 0), _event->button(), _event->buttons() & ~Qt::RightButton, _event->modifiers());
    m_view->mouseMoveEvent(&e);
    _event->accept();
}

void EasyThreadNamesWidget::keyPressEvent(QKeyEvent* _event)
{
    m_view->keyPressEvent(_event);
}

void EasyThreadNamesWidget::keyReleaseEvent(QKeyEvent* _event)
{
    m_view->keyReleaseEvent(_event);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

