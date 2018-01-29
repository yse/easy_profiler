/************************************************************************
* file name         : easy_frame_rate_viewer.cpp
* ----------------- :
* creation time     : 2017/04/02
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : This file contains declaration of FpsViewerWidget widget.
* ----------------- :
* change log        : * 2017/04/02 Victor Zarubkin: Initial commit.
*                   :
*                   : *
* ----------------- : 
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2018  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
*                   :
*                   : The MIT License
*                   :
*                   : Permission is hereby granted, free of charge, to any person obtaining a copy
*                   : of this software and associated documentation files (the "Software"), to deal
*                   : in the Software without restriction, including without limitation the rights 
*                   : to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
*                   : of the Software, and to permit persons to whom the Software is furnished 
*                   : to do so, subject to the following conditions:
*                   : 
*                   : The above copyright notice and this permission notice shall be included in all 
*                   : copies or substantial portions of the Software.
*                   : 
*                   : THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
*                   : INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
*                   : PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
*                   : LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
*                   : TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
*                   : USE OR OTHER DEALINGS IN THE SOFTWARE.
*                   : 
*                   : The Apache License, Version 2.0 (the "License")
*                   :
*                   : You may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
************************************************************************/

#ifndef EASY__FRAME_RATE_VIEWER__H
#define EASY__FRAME_RATE_VIEWER__H

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QTimer>
#include <vector>
#include <deque>
#include <utility>
#include <stdint.h>

//////////////////////////////////////////////////////////////////////////

class FpsGraphicsItem : public QGraphicsItem
{
    using Parent = QGraphicsItem;
    using This = FpsGraphicsItem;
    using FrameTimes = std::deque<std::pair<uint32_t, uint32_t> >;

    std::vector<QPointF> m_points1, m_points2;
    FrameTimes                       m_frames;
    QRectF                     m_boundingRect;

public:

    explicit FpsGraphicsItem();
    ~FpsGraphicsItem() override;

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget) override;

    void setBoundingRect(const QRectF& _boundingRect);
    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);

    void clear();
    void addPoint(uint32_t _maxFrameTime, uint32_t _avgFrameTime);

}; // END of class FpsGraphicsItem.

//////////////////////////////////////////////////////////////////////////

class FpsViewerWidget : public QGraphicsView
{
    Q_OBJECT

private:

    using Parent = QGraphicsView;
    using This = FpsViewerWidget;

    FpsGraphicsItem* m_fpsItem;

public:

    explicit FpsViewerWidget(QWidget* _parent = nullptr);
    ~FpsViewerWidget() override;

    void resizeEvent(QResizeEvent* _event) override;
    void hideEvent(QHideEvent* _event) override;
    void showEvent(QShowEvent* _event) override;
    void contextMenuEvent(QContextMenuEvent* _event) override;
    void dragEnterEvent(QDragEnterEvent*) override {}

public slots:

    void clear();
    void addPoint(uint32_t _maxFrameTime, uint32_t _avgFrameTime);

}; // END of class FpsViewerWidget.

//////////////////////////////////////////////////////////////////////////

#endif // EASY__FRAME_RATE_VIEWER__H
