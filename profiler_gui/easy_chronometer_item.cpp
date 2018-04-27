/************************************************************************
* file name         : easy_chronometer_item.cpp
* ----------------- :
* creation time     : 2016/09/15
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of EasyChronometerItem.
* ----------------- :
* change log        : * 2016/09/15 Victor Zarubkin: moved sources from blocks_graphics_view.cpp
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
#include <QFontMetricsF>
#include "blocks_graphics_view.h"
#include "easy_chronometer_item.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const auto CHRONOMETER_FONT = ::profiler_gui::EFont("Helvetica", 16, QFont::Bold);

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//////////////////////////////////////////////////////////////////////////

EasyChronometerItem::EasyChronometerItem(bool _main)
    : QGraphicsItem()
    , m_color(::profiler_gui::CHRONOMETER_COLOR)
    , m_left(0)
    , m_right(0)
    , m_bMain(_main)
    , m_bReverse(false)
    , m_bHoverIndicator(false)
{
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

    const QString text = ::profiler_gui::timeStringReal(EASY_GLOBALS.time_units, selectedInterval); // Displayed text
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
    if (m_bMain && !m_bReverse)
    {
        QPen p(QColor::fromRgba(0xd0000000 | rgb));
        p.setStyle(Qt::DotLine);
        _painter->setPen(p);
    }
    else
    {
        _painter->setPen(QColor::fromRgba(0xd0000000 | rgb));
    }

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

    const auto textRect_width = textRect.width() * ::profiler_gui::FONT_METRICS_FACTOR;
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
//////////////////////////////////////////////////////////////////////////

