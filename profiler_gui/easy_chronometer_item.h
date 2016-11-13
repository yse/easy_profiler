/************************************************************************
* file name         : easy_chronometer_item.h
* ----------------- :
* creation time     : 2016/09/15
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of EasyChronometerItem - an item
*                   : used to display selected interval on graphics scene.
* ----------------- :
* change log        : * 2016/09/15 Victor Zarubkin: moved sources from blocks_graphics_view.h
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

#ifndef EASY__CHRONOMETER_ITEM__H_
#define EASY__CHRONOMETER_ITEM__H_

#include <QGraphicsItem>
#include <QRectF>
#include <QPolygonF>
#include <QColor>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class QWidget;
class QPainter;
class QStyleOptionGraphicsItem;
class EasyGraphicsView;

class EasyChronometerItem : public QGraphicsItem
{
    QPolygonF  m_indicator; ///< Indicator displayed when this chrono item is out of screen (displaying only for main item)
    QRectF  m_boundingRect; ///< boundingRect (see QGraphicsItem)
    QColor         m_color; ///< Color of the item
    qreal  m_left, m_right; ///< Left and right bounds of the selection zone
    bool           m_bMain; ///< Is this chronometer main (true, by default)
    bool        m_bReverse; ///< 
    bool m_bHoverIndicator; ///< Mouse hover above indicator

public:

    explicit EasyChronometerItem(bool _main = true);
    virtual ~EasyChronometerItem();

    // Public virtual methods

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    // Public non-virtual methods

    void setColor(const QColor& _color);

    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);
    void setBoundingRect(const QRectF& _rect);

    void setLeftRight(qreal _left, qreal _right);

    void setReverse(bool _reverse);

    void setHover(bool _hover);

    bool contains(const QPointF& _pos) const override;

    inline bool hoverIndicator() const
    {
        return m_bHoverIndicator;
    }

    inline bool reverse() const
    {
        return m_bReverse;
    }

    inline qreal left() const
    {
        return m_left;
    }

    inline qreal right() const
    {
        return m_right;
    }

    inline qreal width() const
    {
        return m_right - m_left;
    }

private:

    ///< Returns pointer to the EasyGraphicsView widget.
    const EasyGraphicsView* view() const;
    EasyGraphicsView* view();

}; // END of class EasyChronometerItem.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY__CHRONOMETER_ITEM__H_
