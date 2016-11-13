/************************************************************************
* file name         : easy_graphics_scrollbar.h
* ----------------- : 
* creation time     : 2016/07/04
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- : 
* description       : This file contains declaration of 
* ----------------- : 
* change log        : * 2016/07/04 Victor Zarubkin: Initial commit.
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

#ifndef EASY__GRAPHICS_SCROLLBAR__H
#define EASY__GRAPHICS_SCROLLBAR__H

#include <stdlib.h>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QAction>
#include <QPolygonF>
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////

class EasyGraphicsSliderItem : public QGraphicsRectItem
{
    typedef QGraphicsRectItem      Parent;
    typedef EasyGraphicsSliderItem   This;

private:

    QPolygonF m_indicator;
    qreal m_halfwidth;
    bool m_bMain;

public:

    explicit EasyGraphicsSliderItem(bool _main);
    virtual ~EasyGraphicsSliderItem();

    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

    qreal width() const;
    qreal halfwidth() const;

    void setWidth(qreal _width);
    void setHalfwidth(qreal _halfwidth);

    void setColor(QRgb _color);
    void setColor(const QColor& _color);

}; // END of class EasyGraphicsSliderItem.

//////////////////////////////////////////////////////////////////////////

class EasyMinimapItem : public QGraphicsItem
{
    typedef QGraphicsItem Parent;
    typedef EasyMinimapItem This;

    QRectF                           m_boundingRect;
    qreal                             m_maxDuration;
    qreal                             m_minDuration;
    const ::profiler_gui::EasyItems*      m_pSource;
    ::profiler::thread_id_t              m_threadId;

public:

    EasyMinimapItem();
    virtual ~EasyMinimapItem();

    // Public virtual methods

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    // Public non-virtual methods

    ::profiler::thread_id_t threadId() const;

    void setBoundingRect(const QRectF& _rect);
    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);

    void setSource(::profiler::thread_id_t _thread_id, const ::profiler_gui::EasyItems* _items);

}; // END of class EasyMinimapItem.

//////////////////////////////////////////////////////////////////////////

class EasyGraphicsScrollbar : public QGraphicsView
{
    Q_OBJECT

private:

    typedef QGraphicsView         Parent;
    typedef EasyGraphicsScrollbar   This;

    qreal                           m_minimumValue;
    qreal                           m_maximumValue;
    qreal                                  m_value;
    qreal                            m_windowScale;
    QPoint                         m_mousePressPos;
    Qt::MouseButtons                m_mouseButtons;
    EasyGraphicsSliderItem*               m_slider;
    EasyGraphicsSliderItem* m_chronometerIndicator;
    EasyMinimapItem*                     m_minimap;
    bool                              m_bScrolling;

public:

    explicit EasyGraphicsScrollbar(QWidget* _parent = nullptr);
    virtual ~EasyGraphicsScrollbar();

    // Public virtual methods

    void mousePressEvent(QMouseEvent* _event) override;
    void mouseReleaseEvent(QMouseEvent* _event) override;
    void mouseMoveEvent(QMouseEvent* _event) override;
    void wheelEvent(QWheelEvent* _event) override;
    void resizeEvent(QResizeEvent* _event) override;
    void contextMenuEvent(QContextMenuEvent* _event) override;

public:

    // Public non-virtual methods

    void clear();

    qreal getWindowScale() const;
    ::profiler::thread_id_t minimapThread() const;

    qreal minimum() const;
    qreal maximum() const;
    qreal range() const;
    qreal value() const;
    qreal sliderWidth() const;
    qreal sliderHalfWidth() const;

    void setValue(qreal _value);
    void setRange(qreal _minValue, qreal _maxValue);
    void setSliderWidth(qreal _width);
    void setChronoPos(qreal _left, qreal _right);
    void showChrono();
    void hideChrono();

    void setMinimapFrom(::profiler::thread_id_t _thread_id, const::profiler_gui::EasyItems* _items);

    inline void setMinimapFrom(::profiler::thread_id_t _thread_id, const ::profiler_gui::EasyItems& _items)
    {
        setMinimapFrom(_thread_id, &_items);
    }

signals:

    void rangeChanged();
    void valueChanged(qreal _value);
    void wheeled(qreal _mouseX, int _wheelDelta);

private slots:

    void onThreadActionClicked(bool);
    void onWindowWidthChange(qreal _width);

}; // END of class EasyGraphicsScrollbar.

//////////////////////////////////////////////////////////////////////////

#endif // EASY__GRAPHICS_SCROLLBAR__H
