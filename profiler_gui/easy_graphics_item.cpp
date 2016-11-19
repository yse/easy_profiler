/************************************************************************
* file name         : easy_graphics_item.cpp
* ----------------- :
* creation time     : 2016/09/15
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of EasyGraphicsItem.
* ----------------- :
* change log        : * 2016/09/15 Victor Zarubkin: Moved sources from blocks_graphics_view.cpp
*                   :
*                   : * 
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   :
*                   : Licensed under the Apache License, Version 2.0 (the "License");
*                   : you may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
*                   :
*                   :
*                   : GNU General Public License Usage
*                   : Alternatively, this file may be used under the terms of the GNU
*                   : General Public License as published by the Free Software Foundation,
*                   : either version 3 of the License, or (at your option) any later version.
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
#include <QDebug>
#include <algorithm>
#include "easy_graphics_item.h"
#include "blocks_graphics_view.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

enum BlockItemState : int8_t
{
    BLOCK_ITEM_DO_PAINT_FIRST = -2,
    BLOCK_ITEM_DO_NOT_PAINT = -1,
    BLOCK_ITEM_UNCHANGED,
    BLOCK_ITEM_DO_PAINT
};

//////////////////////////////////////////////////////////////////////////

const int MIN_ITEM_WIDTH = 3;
const int MIN_ITEMS_SPACING = 2;
const int MIN_SYNC_SPACING = 1;
const int NARROW_ITEM_WIDTH = 20;
const QRgb BORDERS_COLOR = ::profiler::colors::Grey700 & 0x00ffffff;// 0x00686868;

inline QRgb selectedItemBorderColor(::profiler::color_t _color) {
    return ::profiler_gui::isLightColor(_color, 192) ? ::profiler::colors::Black : ::profiler::colors::RichRed;
    //return ::profiler::colors::Black;
}

const auto ITEMS_FONT = ::profiler_gui::EFont("Helvetica", 10, QFont::Medium);
const auto SELECTED_ITEM_FONT = ::profiler_gui::EFont("Helvetica", 10, QFont::Bold);

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//////////////////////////////////////////////////////////////////////////

EasyGraphicsItem::EasyGraphicsItem(uint8_t _index, const::profiler::BlocksTreeRoot& _root)
    : QGraphicsItem(nullptr)
    , m_pRoot(&_root)
    , m_index(_index)
{
    const auto u_thread = ::profiler_gui::toUnicode("thread");
    if (_root.got_name())
    {
        QString rootname(::profiler_gui::toUnicode(_root.name()));
        if (rootname.contains(u_thread, Qt::CaseInsensitive))
            m_threadName = ::std::move(QString("%1 %2").arg(rootname).arg(_root.thread_id));
        else
            m_threadName = ::std::move(QString("%1 Thread %2").arg(rootname).arg(_root.thread_id));
    }
    else
    {
        m_threadName = ::std::move(QString("Thread %1").arg(_root.thread_id));
    }
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
    const auto dx = offset * currentScale;

    // Shifting coordinates to current screen offset
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

        auto const dont_skip_children = [this, &levelsNumber](short next_level, decltype(::profiler_gui::EasyBlockItem::children_begin) children_begin, int8_t _state)
        {
            if (next_level < levelsNumber && children_begin != MAX_CHILD_INDEX)
            {
                if (m_levelsIndexes[next_level] == MAX_CHILD_INDEX)
                {
                    // Mark first potentially visible child item on next sublevel
                    m_levelsIndexes[next_level] = children_begin;
                }

                // Mark children items that we want to draw them
                m_levels[next_level][children_begin].state = _state;
            }
        };

        //size_t iterations = 0;
        bool selectedItemsWasPainted = false;
        for (uint8_t l = 0; l < levelsNumber; ++l)
        {
            auto& level = m_levels[l];
            const short next_level = l + 1;
            char state = BLOCK_ITEM_DO_PAINT;

            const auto top = levelY(l);
            if (top > visibleBottom)
                break;

            int j = 1;
            uint32_t par = ~0U;
            qreal prevRight = -1e100;
            for (uint32_t i = m_levelsIndexes[l], end = static_cast<uint32_t>(level.size()); i < end; ++i)
            {
                //++iterations;

                auto& item = level[i];

                if (item.left() > sceneRight)
                    break; // This is first totally invisible item. No need to check other items.

                if (item.state != BLOCK_ITEM_UNCHANGED)
                {
                    state = item.state;
                    item.state = BLOCK_ITEM_DO_NOT_PAINT;
                }

                if (item.right() < sceneLeft)
                {
                    // This item is not visible
                    //skip_children(next_level, item.children_begin);
                    continue;
                } 

                if (state == BLOCK_ITEM_DO_NOT_PAINT)
                {
                    // This item is not visible
                    //skip_children(next_level, item.children_begin);
                    if (item.parent != ~0U)
                        i += static_cast<uint32_t>(easyBlock(item.parent).tree.children.size()) - 1; // Skip all neighbours
                    continue;
                }

                if (state == BLOCK_ITEM_DO_PAINT_FIRST)
                {
                    // Paint only first child which has own children

                    if (par != item.parent)
                        j = 1;

                    par = item.parent;

                    if (item.children_begin == MAX_CHILD_INDEX && next_level < levelsNumber)
                    {
                        // This item has no children and would not be painted
                        ++j;
                        continue;
                    }

                    if (item.parent != ~0U)
                        i += static_cast<uint32_t>(easyBlock(item.parent).tree.children.size()) - j; // Skip all neighbours
                }

                const auto& itemBlock = easyBlock(item.block);
                const uint16_t totalHeight = itemBlock.tree.depth * ::profiler_gui::GRAPHICS_ROW_SIZE_FULL + ::profiler_gui::GRAPHICS_ROW_SIZE;
                if ((top + totalHeight) < visibleSceneRect.top())
                {
                    // This item is not visible
                    //skip_children(next_level, item.children_begin);
                    continue;
                }

                auto x = item.left() * currentScale - dx;
                auto w = item.width() * currentScale;
                if (x + w <= prevRight)
                {
                    // This item is not visible
                    if (!(EASY_GLOBALS.hide_narrow_children && w < NARROW_ITEM_WIDTH) && l > 0)
                        dont_skip_children(next_level, item.children_begin, BLOCK_ITEM_DO_PAINT_FIRST);
                    //else
                    //    skip_children(next_level, item.children_begin);
                    continue;
                }

                if (x < prevRight)
                {
                    w -= prevRight - x;
                    x = prevRight;
                }

                const auto& itemDesc = easyDescriptor(itemBlock.tree.node->id());

                int h = 0, flags = 0;
                if ((EASY_GLOBALS.hide_narrow_children && w < NARROW_ITEM_WIDTH) || !itemBlock.expanded)
                {
                    // Items which width is less than 20 will be painted as big rectangles which are hiding it's children

                    //x = item.left() * currentScale - dx;
                    h = totalHeight;
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

                    if (w < MIN_ITEM_WIDTH)
                        w = MIN_ITEM_WIDTH;

                    // Draw rectangle
                    rect.setRect(x, top, w, h);
                    _painter->drawRect(rect);

                    prevRight = rect.right() + MIN_ITEMS_SPACING;
                    //skip_children(next_level, item.children_begin);
                    if (w < NARROW_ITEM_WIDTH)
                        continue;

                    if (totalHeight > ::profiler_gui::GRAPHICS_ROW_SIZE)
                        flags = Qt::AlignCenter;
                    else if (!(item.width() < 1))
                        flags = Qt::AlignHCenter;
                }
                else
                {
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
                    h = ::profiler_gui::GRAPHICS_ROW_SIZE;
                    const auto dh = top + h - visibleBottom;
                    if (dh > 0)
                        h -= dh;

                    if (w < MIN_ITEM_WIDTH)
                        w = MIN_ITEM_WIDTH;

                    rect.setRect(x, top, w, h);
                    _painter->drawRect(rect);

                    prevRight = rect.right() + MIN_ITEMS_SPACING;
                    if (w < NARROW_ITEM_WIDTH)
                    {
                        dont_skip_children(next_level, item.children_begin, BLOCK_ITEM_DO_PAINT_FIRST);
                        continue;
                    }

                    dont_skip_children(next_level, item.children_begin, BLOCK_ITEM_DO_PAINT);
                    if (!(item.width() < 1))
                        flags = Qt::AlignHCenter;
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
                    decltype(top) h = (!itemBlock.expanded || (w < NARROW_ITEM_WIDTH && EASY_GLOBALS.hide_narrow_children)) ? (itemBlock.tree.depth * ::profiler_gui::GRAPHICS_ROW_SIZE_FULL + ::profiler_gui::GRAPHICS_ROW_SIZE) : ::profiler_gui::GRAPHICS_ROW_SIZE;

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

                        if (!selectedItemsWasPainted && w > NARROW_ITEM_WIDTH)
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

        //printf("%u: %llu\n", m_index, iterations);
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

                if (width < MIN_ITEM_WIDTH)
                    width = MIN_ITEM_WIDTH;

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
                prevRight = left + width + MIN_SYNC_SPACING;
            }
        }
    }



    if (EASY_GLOBALS.enable_event_indicators && !m_pRoot->events.empty())
    {
        auto first = ::std::lower_bound(m_pRoot->events.begin(), m_pRoot->events.end(), offset, [&sceneView](::profiler::block_index_t _index, qreal _value)
        {
            return sceneView->time2position(blocksTree(_index).node->begin()) < _value;
        });

        if (first != m_pRoot->events.end())
        {
            if (first != m_pRoot->events.begin())
                --first;
        }
        else if (!m_pRoot->events.empty())
        {
            first = m_pRoot->events.begin() + m_pRoot->events.size() - 1;
        }

        previousColor = 0;
        qreal prevRight = -1e100, top = y() + boundingRect().height() - 1, h = 3;
        if (top + h < visibleBottom)
        {
            for (auto it = first, end = m_pRoot->events.end(); it != end; ++it)
            {
                const auto& item = blocksTree(*it);
                auto left = sceneView->time2position(item.node->begin());

                if (left > sceneRight)
                    break; // This is first totally invisible item. No need to check other items.

                decltype(left) width = 0.25;
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

                ::profiler::color_t color = easyDescriptor(item.node->id()).color();
                if (previousColor != color)
                {
                    previousColor = color;
                    _painter->setBrush(QColor::fromRgb(color));
                    _painter->setPen(QColor::fromRgb(BORDERS_COLOR & (0xffffffff - color)));
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

    const auto bottom = top + m_levels.size() * ::profiler_gui::GRAPHICS_ROW_SIZE_FULL;
    if (bottom < _pos.y())
    {
        return nullptr;
    }

    const unsigned int levelIndex = static_cast<unsigned int>(_pos.y() - top) / ::profiler_gui::GRAPHICS_ROW_SIZE_FULL;
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
            if (i == levelIndex || (w < NARROW_ITEM_WIDTH && EASY_GLOBALS.hide_narrow_children) || !easyBlock(item.block).expanded)
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
    return y() + static_cast<int>(_level) * static_cast<int>(::profiler_gui::GRAPHICS_ROW_SIZE_FULL);
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
//////////////////////////////////////////////////////////////////////////

