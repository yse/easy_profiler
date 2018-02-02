/************************************************************************
* file name         : arbitrary_value_inspector.cpp
* ----------------- :
* creation time     : 2017/11/30
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of .
* ----------------- :
* change log        : * 2017/11/30 Victor Zarubkin: initial commit.
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

#include <QPainter>
#include <QGraphicsScene>
#include <QColor>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QSplitter>
#include <QResizeEvent>
#include <QVariant>
#include <QSettings>
#include <QToolBar>
#include <QAction>
#include <list>
#include <cmath>
#include "arbitrary_value_inspector.h"
#include "treeview_first_column_delegate.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////

EASY_CONSTEXPR int ChartBound = 2; ///< Top and bottom bounds for chart
EASY_CONSTEXPR int ChartBounds = ChartBound << 1;

void getChartPoints(const ArbitraryValuesCollection& _collection, Points& _points, qreal& _minValue, qreal& _maxValue)
{
    _minValue =  1e300;
    _maxValue = -1e300;

    const auto size = _collection.size();
    _points.clear();
    _points.reserve(size);

    if (size == 0)
        return;

    const auto& valuesByThread = _collection.valuesMap();

    if (valuesByThread.size() == 1)
    {
        const auto& values = valuesByThread.begin()->second;
        for (auto value : values)
        {
            const qreal x = sceneX(value->begin());
            const qreal y = profiler_gui::value2real(*value);
            _points.emplace_back(x, y);

            if (y > _maxValue)
                _maxValue = y;
            if (y < _minValue)
                _minValue = y;
        }
    }
    else
    {
        std::list<profiler::thread_id_t> threadIds;
        for (const auto& it : valuesByThread)
            threadIds.push_back(it.first);

        size_t i = 0;
        while (!threadIds.empty())
        {
            for (auto it = threadIds.begin(); it != threadIds.end();)
            {
                const auto& values = valuesByThread.at(*it);
                if (i >= values.size())
                {
                    it = threadIds.erase(it);
                    continue;
                }

                const qreal x = sceneX(values[i]->begin());
                const qreal y = profiler_gui::value2real(*values[i]);
                _points.emplace_back(x, y);

                if (y > _maxValue)
                    _maxValue = y;
                if (y < _minValue)
                    _minValue = y;

                ++it;
            }
        }

        std::sort(_points.begin(), _points.end(), [](const QPointF& lhs, const QPointF& rhs) -> bool {
            return lhs.x() < rhs.x();
        });
    }
}

//////////////////////////////////////////////////////////////////////////

ArbitraryValuesCollection::ArbitraryValuesCollection()
    : m_beginTime(0)
    , m_minValue(1e300)
    , m_maxValue(-1e300)
    , m_jobType(0)
{
    m_status = ATOMIC_VAR_INIT(Idle);
    m_bInterrupt = ATOMIC_VAR_INIT(false);
}

ArbitraryValuesCollection::~ArbitraryValuesCollection()
{

}

const ArbitraryValuesMap& ArbitraryValuesCollection::valuesMap() const
{
    return m_values;
}

const Points& ArbitraryValuesCollection::points() const
{
    return m_points;
}

ArbitraryValuesCollection::JobStatus ArbitraryValuesCollection::status() const
{
    return static_cast<JobStatus>(m_status.load(std::memory_order_acquire));
}

size_t ArbitraryValuesCollection::size() const
{
    size_t totalSize = 0;
    for (const auto& it : m_values)
        totalSize += it.second.size();
    return totalSize;
}

qreal ArbitraryValuesCollection::minValue() const
{
    return m_minValue;
}

qreal ArbitraryValuesCollection::maxValue() const
{
    return m_maxValue;
}

void ArbitraryValuesCollection::collectValues(profiler::thread_id_t _threadId, profiler::vin_t _valueId, const char* _valueName
    , profiler::block_id_t _parentBlockId, bool _directParent)
{
    interrupt();

    setStatus(InProgress);
    m_points.clear();
    m_jobType = ValuesJob;

    if (_valueId == 0)
        m_worker.enqueue([=] { collectByName(_threadId, _valueName, _parentBlockId, _directParent); }, m_bInterrupt);
    else
        m_worker.enqueue([=] { collectById(_threadId, _valueId, _parentBlockId, _directParent); }, m_bInterrupt);
}

void ArbitraryValuesCollection::collectValues(profiler::thread_id_t _threadId, profiler::vin_t _valueId, const char* _valueName
    , profiler::timestamp_t _beginTime, profiler::block_id_t _parentBlockId, bool _directParent)
{
    interrupt();

    setStatus(InProgress);
    m_points.clear();
    m_beginTime = _beginTime;
    m_minValue =  1e300;
    m_maxValue = -1e300;
    m_jobType = ValuesJob | PointsJob;

    if (_valueId == 0)
        m_worker.enqueue([=] { collectByName(_threadId, _valueName, _parentBlockId, _directParent); }, m_bInterrupt);
    else
        m_worker.enqueue([=] { collectById(_threadId, _valueId, _parentBlockId, _directParent); }, m_bInterrupt);
}

bool ArbitraryValuesCollection::calculatePoints(profiler::timestamp_t _beginTime)
{
    if (status() != Ready || m_values.empty())
        return false;

    m_worker.dequeue();

    setStatus(InProgress);
    m_points.clear();
    m_beginTime = _beginTime;
    m_minValue =  1e300;
    m_maxValue = -1e300;
    m_jobType = PointsJob;

    m_worker.enqueue([this]
    {
        getChartPoints(*this, m_points, m_minValue, m_maxValue);
        setStatus(Ready);
    }, m_bInterrupt);

    return true;
}

void ArbitraryValuesCollection::interrupt()
{
    m_worker.dequeue();
    setStatus(Idle);
    m_jobType = None;
    m_values.clear();
}

void ArbitraryValuesCollection::setStatus(JobStatus _status)
{
    m_status.store(static_cast<uint8_t>(_status), std::memory_order_release);
}

void ArbitraryValuesCollection::collectById(profiler::thread_id_t _threadId, profiler::vin_t _valueId
    , profiler::block_id_t _parentBlockId, bool _directParent)
{
    if (_threadId == 0)
    {
        const auto threadsCount = EASY_GLOBALS.profiler_blocks.size();
        const bool calculatePointsInner = (m_jobType & PointsJob) != 0 && threadsCount == 1;

        for (const auto& it : EASY_GLOBALS.profiler_blocks)
        {
            if (!collectByIdForThread(it.second, _valueId, calculatePointsInner, _parentBlockId, _directParent))
                return;
        }

        if ((m_jobType & PointsJob) != 0 && threadsCount > 1)
            getChartPoints(*this, m_points, m_minValue, m_maxValue);
    }
    else
    {
        const auto t = EASY_GLOBALS.profiler_blocks.find(_threadId);
        if (t != EASY_GLOBALS.profiler_blocks.end() &&
            !collectByIdForThread(t->second, _valueId, (m_jobType & PointsJob) != 0, _parentBlockId, _directParent))
        {
            return;
        }
    }

    setStatus(Ready);
}

bool ArbitraryValuesCollection::collectByIdForThread(const profiler::BlocksTreeRoot& _threadRoot
    , profiler::vin_t _valueId, bool _calculatePoints, profiler::block_id_t _parentBlockId, bool _directParent)
{
    auto& valuesList = m_values[_threadRoot.thread_id];

    for (auto i : _threadRoot.events)
    {
        if (m_bInterrupt.load(std::memory_order_acquire))
            return false;

        const auto& block = easyBlock(i).tree;
        const auto& desc = easyDescriptor(block.node->id());
        if (desc.type() != profiler::BlockType::Value)
            continue;

        const auto value = block.value;
        if (value->value_id() != _valueId)
            continue;

        valuesList.push_back(value);

        if (_calculatePoints)
        {
            const auto p = point(*block.value);

            if (p.y() > m_maxValue)
                m_maxValue = p.y();
            if (p.y() < m_minValue)
                m_minValue = p.y();

            m_points.push_back(p);
        }
    }

    return true;
}

void ArbitraryValuesCollection::collectByName(profiler::thread_id_t _threadId, const std::string _valueName
    , profiler::block_id_t _parentBlockId, bool _directParent)
{
    if (_threadId == 0)
    {
        const auto threadsCount = EASY_GLOBALS.profiler_blocks.size();
        const bool calculatePointsInner = (m_jobType & PointsJob) != 0 && threadsCount == 1;

        for (const auto& it : EASY_GLOBALS.profiler_blocks)
        {
            if (!collectByNameForThread(it.second, _valueName, calculatePointsInner, _parentBlockId, _directParent))
                return;
        }

        if ((m_jobType & PointsJob) != 0 && threadsCount > 1)
            getChartPoints(*this, m_points, m_minValue, m_maxValue);
    }
    else
    {
        const auto t = EASY_GLOBALS.profiler_blocks.find(_threadId);
        if (t != EASY_GLOBALS.profiler_blocks.end() &&
            !collectByNameForThread(t->second, _valueName, (m_jobType & PointsJob) != 0, _parentBlockId, _directParent))
        {
            return;
        }
    }

    setStatus(Ready);
}

bool ArbitraryValuesCollection::collectByNameForThread(const profiler::BlocksTreeRoot& _threadRoot
    , const std::string& _valueName, bool _calculatePoints, profiler::block_id_t _parentBlockId, bool _directParent)
{
    auto& valuesList = m_values[_threadRoot.thread_id];

    for (auto i : _threadRoot.events)
    {
        if (m_bInterrupt.load(std::memory_order_acquire))
            return false;

        const auto& block = easyBlock(i).tree;
        const auto& desc = easyDescriptor(block.node->id());
        if (desc.type() != profiler::BlockType::Value || _valueName != desc.name())
            continue;

        valuesList.push_back(block.value);

        if (_calculatePoints)
        {
            const auto p = point(*block.value);

            if (p.y() > m_maxValue)
                m_maxValue = p.y();
            if (p.y() < m_minValue)
                m_minValue = p.y();

            m_points.push_back(p);
        }
    }

    return true;
}

QPointF ArbitraryValuesCollection::point(const profiler::ArbitraryValue& _value) const
{
    const qreal x = PROF_MICROSECONDS(qreal(_value.begin() - m_beginTime));
    const qreal y = profiler_gui::value2real(_value);
    return {x, y};
}

//////////////////////////////////////////////////////////////////////////

ArbitraryValuesChartItem::ArbitraryValuesChartItem()
    : Parent()
    , m_workerMaxValue(0)
    , m_workerMinValue(0)
{
}

ArbitraryValuesChartItem::~ArbitraryValuesChartItem()
{

}

void ArbitraryValuesChartItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    const auto widget = static_cast<const GraphicsSliderArea*>(scene()->parent());
    const auto currentScale = widget->getWindowScale();
    const bool bindMode = widget->bindMode();
    const auto bottom = m_boundingRect.bottom();
    const auto width = m_boundingRect.width() * currentScale;

    _painter->save();
    _painter->setTransform(QTransform::fromScale(1.0 / currentScale, 1), true);

    if (!bindMode)
        paintImage(_painter);
    else
        paintImage(_painter, currentScale, widget->minimum(), widget->maximum(), widget->value(), widget->sliderWidth());

    const auto font_h = widget->fontHeight();
    QRectF rect(0, m_boundingRect.top() - widget->margin(), width - 3, m_boundingRect.height() + widget->margins());
    _painter->setPen(profiler_gui::TEXT_COLOR);
    _painter->drawText(rect, Qt::AlignLeft | Qt::AlignTop, bindMode ? " Mode: Zoom" : " Mode: Overview");

    if (!EASY_GLOBALS.scene.empty)
    {
        const auto range = bindMode ? widget->sliderWidth() : widget->range();
        paintMouseIndicator(_painter, m_boundingRect.top(), bottom, width,
                            m_boundingRect.height(), font_h, widget->value(), range);
    }

    _painter->setPen(Qt::darkGray);
    _painter->drawLine(QLineF(0, bottom, width, bottom));
    _painter->drawLine(QLineF(0, m_boundingRect.top(), width, m_boundingRect.top()));

    _painter->restore();
}

void ArbitraryValuesChartItem::paintMouseIndicator(QPainter* _painter, qreal _top, qreal _bottom, qreal _width, qreal _height,
                                                   int _font_h, qreal _visibleRegionLeft, qreal _visibleRegionWidth)
{
    if (_font_h == 0)
        return;

    const auto x = m_mousePos.x();
    auto y = m_mousePos.y();

    // Horizontal
    const bool visibleY = (_top < y && y < _bottom);
    y = estd::clamp(_top, y, _bottom);
    //if (visibleY)
    {
        _height -= ChartBounds;

        const int half_font_h = _font_h >> 1;
        const auto yvalue = estd::clamp(_top + ChartBound, y, _bottom - ChartBound);
        const auto value = m_minValue + ((_bottom - ChartBound - yvalue) / _height) * (m_maxValue - m_minValue);
        const auto mouseStr = QString::number(value, 'f', 3);
        const int textWidth = _painter->fontMetrics().width(mouseStr) + 3;
        const QRectF rect(0, y - _font_h - 2, _width - 3, 4 + (_font_h << 1));

        _painter->setPen(Qt::blue);

        qreal left = 0, right = _width - 3;
        const Qt::AlignmentFlag alignment = x < textWidth ? Qt::AlignRight : Qt::AlignLeft;

        if (y > _bottom - half_font_h)
        {
            _painter->drawText(rect, alignment | Qt::AlignTop, mouseStr);
        }
        else if (y < _top + half_font_h)
        {
            _painter->drawText(rect, alignment | Qt::AlignBottom, mouseStr);
        }
        else
        {
            _painter->drawText(rect, alignment | Qt::AlignVCenter, mouseStr);
            if (x < textWidth)
                right = _width - textWidth - 3;
            else
                left = textWidth;
        }

        if (visibleY)
            _painter->drawLine(QLineF(left, y, right, y));
    }

    // Vertical
    if (0 < x && x < m_boundingRect.width())
    {
        const auto value = _visibleRegionLeft + _visibleRegionWidth * x / _width;
        const auto mouseStr = profiler_gui::timeStringReal(EASY_GLOBALS.time_units, value, 3);
        const int textWidth = _painter->fontMetrics().width(mouseStr) + 6;
        const int textWidthHalf = textWidth >> 1;

        qreal left = x - textWidthHalf;
        if (x < textWidthHalf)
            left = 0;
        else if (x > (_width - textWidthHalf))
            left = _width - textWidth;

        //if (!visibleY)
        //    _painter->setPen(Qt::blue);

        const QRectF rect(left, _bottom + 2, textWidth, _font_h);
        _painter->drawText(rect, Qt::AlignCenter, mouseStr);
        _painter->drawLine(QLineF(x, _top, x, _bottom));
    }
}

bool ArbitraryValuesChartItem::updateImage()
{
    if (!Parent::updateImage())
        return false;

    const auto widget = static_cast<const GraphicsSliderArea*>(scene()->parent());

    m_imageScaleUpdate = widget->range() / widget->sliderWidth();
    m_imageOriginUpdate = widget->bindMode() ? (widget->value() - widget->sliderWidth() * 3) : widget->minimum();

    // Ugly, but doen't use exceeded count of threads
    const auto rect = m_boundingRect;
    const auto scale = widget->getWindowScale();
    const auto left = widget->minimum();
    const auto right = widget->maximum();
    const auto value = widget->value();
    const auto window = widget->sliderWidth();
    const auto bindMode = widget->bindMode();
    const auto beginTime = EASY_GLOBALS.begin_time;
    const auto autoHeight = EASY_GLOBALS.auto_adjust_chart_height;
    m_worker.enqueue([=] {
        updateImageAsync(rect, scale, left, right, right - left, value, window, bindMode,
                         beginTime, autoHeight);
    }, m_bReady);

    return true;
}

void ArbitraryValuesChartItem::onImageUpdated()
{
    m_maxValue = m_workerMaxValue;
    m_minValue = m_workerMinValue;
}

void ArbitraryValuesChartItem::updateImageAsync(QRectF _boundingRect, qreal _current_scale, qreal _minimum, qreal _maximum, qreal _range,
                                                qreal _value, qreal _width, bool _bindMode, profiler::timestamp_t _begin_time, bool _autoAdjust)
{
    const auto screenWidth = _boundingRect.width() * _current_scale;
    //const auto maxColumnHeight = _boundingRect.height();
    const auto viewScale = _range / _width;

    if (_bindMode)
    {
        m_workerImageScale = viewScale;
        m_workerImageOrigin = _value - _width * 3;
        m_workerImage = new QImage(screenWidth * 7 + 0.5, _boundingRect.height(), QImage::Format_ARGB32);
    }
    else
    {
        m_workerImageScale = 1;
        m_workerImageOrigin = _minimum;
        m_workerImage = new QImage(screenWidth + 0.5, _boundingRect.height(), QImage::Format_ARGB32);
    }

    m_workerImage->fill(0);
    QPainter p(m_workerImage);
    p.setBrush(Qt::NoBrush);
    p.setRenderHint(QPainter::Antialiasing, true);

    // Draw grid
    {
        auto pen = p.pen();
        pen.setColor(Qt::darkGray);
        pen.setStyle(Qt::DotLine);
        p.setPen(pen);

        const int left = 0;
        const int top = 0;
        const int w = m_workerImage->width();
        const int h = m_workerImage->height();

        const int hlines = h / 20;
        for (int i = 0; i < hlines; ++i)
        {
            const auto y = top + 20 + i * 20;
            p.drawLine(left, y, left + w, y);
        }

        const int vlines = w / 20;
        for (int i = 0; i < vlines; ++i)
        {
            const auto x = left + 20 + i * 20;
            p.drawLine(x, top, x, top + h);
        }

        p.setPen(Qt::SolidLine);
    }

    if (m_collections.empty() || isReady())
    {
        setReady(true);
        return;
    }

    using LeftBounds = std::vector<Points::const_iterator>;
    qreal realScale = _current_scale, offset = 0;

    LeftBounds leftBounds;
    leftBounds.reserve(m_collections.size());

    if (_bindMode)
    {
        _minimum = m_workerImageOrigin;
        _maximum = m_workerImageOrigin + _width * 7;
        realScale *= viewScale;
        offset = _minimum * realScale;
    }

    const auto right = std::min(_value + _width, _maximum);
    qreal minValue = 1e300, maxValue = -1e300;
    for (const auto& c : m_collections)
    {
        if (isReady())
            return;

        const auto& collection = *c.ptr;
        const auto& points = collection.points();

        if (points.empty())
        {
            leftBounds.emplace_back(points.end());
            continue;
        }

        if (_bindMode)
        {
            auto first = std::lower_bound(points.begin(), points.end(), _minimum, [](const QPointF& point, qreal x)
            {
                return point.x() < x;
            });

            if (first != points.end())
            {
                if (first != points.begin())
                    --first;
            }
            else
            {
                first = points.begin() + points.size() - 1;
            }

            leftBounds.emplace_back(first);

            if (_autoAdjust)
            {
                for (auto it = first; it != points.end() && it->x() < right; ++it)
                {
                    if (it->x() < _value)
                        continue;

                    const auto value = it->y();

                    if (minValue > value)
                        minValue = value;

                    if (maxValue < value)
                        maxValue = value;
                }

                continue;
            }
        }
        else
        {
            leftBounds.emplace_back(points.begin());
        }

        if (minValue > collection.minValue())
            minValue = collection.minValue();

        if (maxValue < collection.maxValue())
            maxValue = collection.maxValue();
    }

    if (minValue > maxValue)
    {
        // No points
        m_workerMinValue = 0;
        m_workerMaxValue = 0;
        setReady(true);
        return;
    }

    m_workerMinValue = minValue;
    m_workerMaxValue = maxValue;

    if (isReady())
        return;

    const bool singleValue = fabs(maxValue - minValue) < 2 * std::numeric_limits<qreal>::epsilon();
    const auto middle = _boundingRect.height() * 0.5;

    const qreal height = std::max(maxValue - minValue, 0.01);
    const auto gety = [&_boundingRect, maxValue, height, singleValue, middle] (qreal y)
    {
        if (singleValue)
        {
            y = middle;
        }
        else
        {
            y = maxValue - y;
            y /= height;
            y *= _boundingRect.height() - ChartBounds;
            y += ChartBound;
        }

        return y;
    };

    size_t i = 0;
    for (const auto& c : m_collections)
    {
        if (isReady())
            return;

        const auto& points = c.ptr->points();
        if (points.empty())
        {
            ++i;
            continue;
        }

        if (c.selected)
        {
            auto pen = p.pen();
            pen.setColor(QColor::fromRgba(c.color));
            pen.setWidth(2);
            p.setPen(pen);
        }
        else
        {
            p.setPen(QColor::fromRgba(c.color));
        }

        const auto first = leftBounds[i];

        if (c.chartType == ChartViewType::Points)
        {
            qreal prevX = 1e300, prevY = 1e300;
            for (auto it = first; it != points.end() && it->x() < _maximum; ++it)
            {
                if (it->x() < _minimum)
                    continue;

                if (isReady())
                    return;

                const qreal x = it->x() * realScale - offset;
                const qreal y = gety(it->y());
                const auto dx = fabs(x - prevX), dy = fabs(y - prevY);

                if (dx > 1 || dy > 1)
                {
                    p.drawPoint(QPointF{x, y});
                    prevX = x;
                    prevY = y;
                }
            }
        }
        else if (first != points.end() && first->x() < _maximum)
        {
            QPointF p1 = *first;
            qreal x = p1.x() * realScale - offset;
            qreal y = gety(p1.y());
            p1.setX(x);
            p1.setY(y);

            auto it = first;
            for (++it; it != points.end(); ++it)
            {
                if (isReady())
                    return;

                QPointF p2 = *it;
                x = p2.x() * realScale - offset;
                y = gety(p2.y());
                p2.setX(x);
                p2.setY(y);

                if (it->x() >= _minimum)
                {
                    const auto dx = fabs(x - p1.x()), dy = fabs(y - p1.y());
                    if (dx > 1 || dy > 1)
                        p.drawLine(p1, p2);
                    else
                        continue;
                }

                if (it->x() >= _maximum)
                    break;

                p1 = p2;
            }
        }

        if (c.selected)
        {
            auto color = profiler_gui::darken(c.color, 0.65f);
            if (profiler_gui::alpha(color) < 0xc0)
                p.setPen(QColor::fromRgba(profiler::colors::modify_alpha32(0xc0000000, color)));
            else
                p.setPen(QColor::fromRgba(color));
            p.setBrush(QColor::fromRgba(0xc8ffffff));

            qreal prevX = -offset * 2, prevY = -500;
            for (auto it = first; it != points.end() && it->x() < _maximum; ++it)
            {
                if (it->x() < _minimum)
                    continue;

                if (isReady())
                    return;

                const qreal x = it->x() * realScale - offset;
                const qreal y = gety(it->y());

                const auto dx = x - prevX, dy = y - prevY;
                const auto delta = estd::sqr(dx) + estd::sqr(dy);

                if (delta > 25)
                {
                    p.drawEllipse(QPointF {x, y}, 3, 3);
                    prevX = x;
                    prevY = y;
                }
            }
        }

        ++i;
    }

    setReady(true);
}

void ArbitraryValuesChartItem::clear()
{
    cancelAnyJob();
    m_boundaryTimer.stop();
    m_collections.clear();
    m_minValue = m_maxValue = 0;
}

void ArbitraryValuesChartItem::update(Collections _collections)
{
    cancelImageUpdate();
    m_collections = std::move(_collections);
    updateImage();
}

void ArbitraryValuesChartItem::update(const ArbitraryValuesCollection* _selected)
{
    cancelImageUpdate();

    for (auto& collection : m_collections)
        collection.selected = collection.ptr == _selected;

    updateImage();
}

//////////////////////////////////////////////////////////////////////////

GraphicsChart::GraphicsChart(QWidget* _parent)
    : Parent(_parent)
    , m_chartItem(new ArbitraryValuesChartItem())
{
    m_imageItem = m_chartItem;
    scene()->addItem(m_chartItem);

    const auto rect = scene()->sceneRect();
    m_chartItem->setPos(0, 0);
    m_chartItem->setBoundingRect(0, rect.top() + margin(), scene()->width(), rect.height() - margins() - 1);

    connect(&EASY_GLOBALS.events, &profiler_gui::GlobalSignals::autoAdjustChartChanged, this, &This::onAutoAdjustChartChanged);

    m_chartItem->updateImage();
}

GraphicsChart::~GraphicsChart()
{

}

void GraphicsChart::onAutoAdjustChartChanged()
{
    if (m_chartItem->isVisible())
        m_chartItem->onModeChanged();
}

void GraphicsChart::clear()
{
    cancelImageUpdate();
    Parent::clear();
}

void GraphicsChart::cancelImageUpdate()
{
    m_chartItem->clear();
}

void GraphicsChart::update(Collections _collections)
{
    m_chartItem->update(std::move(_collections));
}

void GraphicsChart::update(const ArbitraryValuesCollection* _selected)
{
    m_chartItem->update(_selected);
}

//////////////////////////////////////////////////////////////////////////

enum class ArbitraryColumns : uint8_t
{
    Name = 0,
    Type,
    Value,
    Vin,

    Count
};

EASY_CONSTEXPR auto CheckColumn = int_cast(ArbitraryColumns::Name);
EASY_CONSTEXPR auto StdItemType = QTreeWidgetItem::UserType;
EASY_CONSTEXPR auto ValueItemType = QTreeWidgetItem::UserType + 1;

struct UsedValueTypes {
    ArbitraryTreeWidgetItem* items[int_cast(profiler::DataType::TypesCount)];
    UsedValueTypes(int = 0) { memset(items, 0, sizeof(items)); }
};

//////////////////////////////////////////////////////////////////////////

ArbitraryTreeWidgetItem::ArbitraryTreeWidgetItem(QTreeWidgetItem* _parent, profiler::color_t _color, profiler::vin_t _vin)
    : Parent(_parent, ValueItemType)
    , m_vin(_vin)
    , m_color(_color)
    , m_widthHint(0)
{
    setFlags(flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    setCheckState(CheckColumn, Qt::Unchecked);
}

ArbitraryTreeWidgetItem::~ArbitraryTreeWidgetItem()
{
    interrupt();
}

QVariant ArbitraryTreeWidgetItem::data(int _column, int _role) const
{
    if (_column == CheckColumn && _role == Qt::SizeHintRole)
        return QSize(static_cast<int>(m_widthHint * m_font.bold() ? 1.2f : 1.f), 26);
    if (_role == Qt::FontRole)
        return m_font;
    return Parent::data(_column, _role);
}

void ArbitraryTreeWidgetItem::setWidthHint(int _width)
{
    m_widthHint = _width;
}

void ArbitraryTreeWidgetItem::setBold(bool _isBold)
{
    m_font.setBold(_isBold);
}

const ArbitraryValuesCollection* ArbitraryTreeWidgetItem::collection() const
{
    return m_collection.get();
}

ArbitraryValuesCollection* ArbitraryTreeWidgetItem::collection()
{
    return m_collection.get();
}

void ArbitraryTreeWidgetItem::collectValues(profiler::thread_id_t _threadId)
{
    if (!m_collection)
        m_collection = CollectionPtr(new ArbitraryValuesCollection);
    else
        m_collection->interrupt();

    auto parentItem = parent();
    auto parentBlockId = profiler_gui::numeric_max<profiler::block_id_t>();
    bool directParent = false;

    if (parentItem->data(int_cast(ArbitraryColumns::Type), Qt::UserRole).toInt() == 1)
    {
        parentBlockId = parentItem->data(int_cast(ArbitraryColumns::Vin), Qt::UserRole).toUInt();
        directParent = true;
    }

    m_collection->collectValues(_threadId, m_vin, text(int_cast(ArbitraryColumns::Name)).toStdString().c_str(),
                                EASY_GLOBALS.begin_time, parentBlockId, directParent);
}

void ArbitraryTreeWidgetItem::interrupt()
{
    if (!m_collection)
        return;

    m_collection->interrupt();
    m_collection.release();
}

profiler::color_t ArbitraryTreeWidgetItem::color() const
{
    return m_color;
}

//////////////////////////////////////////////////////////////////////////

ArbitraryValuesWidget::ArbitraryValuesWidget(QWidget* _parent)
    : Parent(_parent)
    , m_splitter(new QSplitter(Qt::Horizontal, this))
    , m_treeWidget(new QTreeWidget(this))
    , m_chart(new GraphicsChart(this))
    , m_boldItem(nullptr)
{
    m_splitter->setHandleWidth(1);
    m_splitter->setContentsMargins(0, 0, 0, 0);
    m_splitter->addWidget(m_treeWidget);
    m_splitter->addWidget(m_chart);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);

    auto tb = new QToolBar(this);
    tb->setIconSize(applicationIconsSize());
    auto refreshButton = tb->addAction(QIcon(imagePath("reload")), tr("Refresh values list"));
    refreshButton->setToolTip(tr("Refresh arbitrary values list."));
    connect(refreshButton, &QAction::triggered, this, &This::rebuild);

    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(tb);
    layout->addWidget(m_splitter);

    m_treeWidget->setAutoFillBackground(false);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setItemsExpandable(true);
    m_treeWidget->setAnimated(true);
    //m_treeWidget->setSortingEnabled(false);
    m_treeWidget->setColumnCount(int_cast(ArbitraryColumns::Count));
    m_treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeWidget->setItemDelegateForColumn(0, new TreeViewFirstColumnItemDelegate(this));

    auto headerItem = new QTreeWidgetItem();
    headerItem->setText(int_cast(ArbitraryColumns::Type), "Type");
    headerItem->setText(int_cast(ArbitraryColumns::Name), "Name");
    headerItem->setText(int_cast(ArbitraryColumns::Value), "Value");
    headerItem->setText(int_cast(ArbitraryColumns::Vin), "ID");
    m_treeWidget->setHeaderItem(headerItem);

    connect(&m_collectionsTimer, &QTimer::timeout, this, &This::onCollectionsTimeout);

    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &This::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::itemChanged, this, &This::onItemChanged);
    connect(m_treeWidget, &QTreeWidget::currentItemChanged, this, &This::onCurrentItemChanged);

    auto globalEvents = &EASY_GLOBALS.events;
    connect(globalEvents, &profiler_gui::GlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChanged);
    connect(globalEvents, &profiler_gui::GlobalSignals::selectedBlockIdChanged, this, &This::onSelectedBlockIdChanged);
    connect(globalEvents, &profiler_gui::GlobalSignals::fileOpened, this, &This::rebuild);

    loadSettings();

    rebuild();
}

ArbitraryValuesWidget::~ArbitraryValuesWidget()
{
    saveSettings();
}

void ArbitraryValuesWidget::clear()
{
    if (m_collectionsTimer.isActive())
        m_collectionsTimer.stop();
    m_chart->cancelImageUpdate();
    m_checkedItems.clear();
    m_treeWidget->clear();
    m_boldItem = nullptr;
}

void ArbitraryValuesWidget::onSelectedBlockChanged(uint32_t)
{
    if (profiler_gui::is_max(EASY_GLOBALS.selected_block))
    {
        onSelectedBlockIdChanged(EASY_GLOBALS.selected_block_id);
        return;
    }

    if (!profiler_gui::is_max(EASY_GLOBALS.selected_block))
        return;

    // TODO: find item corresponding to selected_block and make it bold
}

void ArbitraryValuesWidget::onSelectedBlockIdChanged(::profiler::block_id_t)
{
    if (!profiler_gui::is_max(EASY_GLOBALS.selected_block))
        return;

    if (profiler_gui::is_max(EASY_GLOBALS.selected_block_id))
    {
        if (m_boldItem != nullptr)
        {
            m_boldItem->setBold(false);
            m_boldItem = nullptr;
        }

        return;
    }

    // TODO: find item corresponding to selected_block_id and make it bold
}

void ArbitraryValuesWidget::onItemDoubleClicked(QTreeWidgetItem* _item, int)
{
    if (_item == nullptr || _item->type() != ValueItemType)
        return;

    _item->setCheckState(CheckColumn, _item->checkState(CheckColumn) == Qt::Checked ? Qt::Unchecked : Qt::Checked);
}

void ArbitraryValuesWidget::onItemChanged(QTreeWidgetItem* _item, int _column)
{
    if (_item == nullptr || _item->type() != ValueItemType || _column != CheckColumn)
        return;

    auto item = static_cast<ArbitraryTreeWidgetItem*>(_item);

    if (item->checkState(CheckColumn) == Qt::Checked)
    {
        m_checkedItems.push_back(item);
        item->collectValues(EASY_GLOBALS.selected_thread);
        if (!m_collectionsTimer.isActive())
            m_collectionsTimer.start(100);
    }
    else
    {
        m_checkedItems.removeOne(item);
        item->interrupt();
        onCollectionsTimeout();
    }
}

void ArbitraryValuesWidget::onCurrentItemChanged(QTreeWidgetItem* _current, QTreeWidgetItem*)
{
    if (_current == nullptr || _current->type() != ValueItemType)
    {
        m_chart->update(nullptr);
        return;
    }

    auto item = static_cast<const ArbitraryTreeWidgetItem*>(_current);
    m_chart->update(item->collection());
}

void ArbitraryValuesWidget::rebuild()
{
    clear();

    buildTree(EASY_GLOBALS.selected_thread, EASY_GLOBALS.selected_block, EASY_GLOBALS.selected_block_id);

    m_treeWidget->expandAll();
    for (int i = 0, columns = m_treeWidget->columnCount(); i < columns; ++i)
        m_treeWidget->resizeColumnToContents(i);
}

void ArbitraryValuesWidget::onCollectionsTimeout()
{
    if (m_checkedItems.isEmpty())
    {
        if (m_collectionsTimer.isActive())
            m_collectionsTimer.stop();
        m_chart->update(Collections {});
        return;
    }

    Collections collections;
    collections.reserve(m_checkedItems.size());
    for (auto item : m_checkedItems)
    {
        if (item->collection()->status() != ArbitraryValuesCollection::InProgress)
        {
            collections.push_back(CollectionPaintData {item->collection(), item->color(),
                ChartViewType::Line, item == m_treeWidget->currentItem()});
        }
    }

    if (collections.size() == m_checkedItems.size())
    {
        if (m_collectionsTimer.isActive())
            m_collectionsTimer.stop();
        m_chart->update(std::move(collections));
    }
}

void ArbitraryValuesWidget::buildTree(profiler::thread_id_t _threadId, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId)
{
    m_treeWidget->clear();
    m_treeWidget->setColumnHidden(int_cast(ArbitraryColumns::Value), profiler_gui::is_max(_blockIndex));

    if (_threadId != 0)
    {
        auto it = EASY_GLOBALS.profiler_blocks.find(_threadId);
        if (it != EASY_GLOBALS.profiler_blocks.end())
        {
            auto threadItem = buildTreeForThread(it->second, _blockIndex, _blockId);
            m_treeWidget->addTopLevelItem(threadItem);
        }
    }
    else
    {
        for (const auto& it : EASY_GLOBALS.profiler_blocks)
        {
            auto threadItem = buildTreeForThread(it.second, _blockIndex, _blockId);
            m_treeWidget->addTopLevelItem(threadItem);
        }
    }
}

QTreeWidgetItem* ArbitraryValuesWidget::buildTreeForThread(const profiler::BlocksTreeRoot& _threadRoot, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId)
{
    auto fm = m_treeWidget->fontMetrics();

    auto rootItem = new QTreeWidgetItem(StdItemType);
    rootItem->setText(int_cast(ArbitraryColumns::Type), QStringLiteral("Thread"));
    rootItem->setText(int_cast(ArbitraryColumns::Name),
        profiler_gui::decoratedThreadName(EASY_GLOBALS.use_decorated_thread_name, _threadRoot, EASY_GLOBALS.hex_thread_id));
    rootItem->setData(int_cast(ArbitraryColumns::Type), Qt::UserRole, 0);

    const bool hasConcreteBlock = !profiler_gui::is_max(_blockIndex);
    if (hasConcreteBlock)
    {
        const auto& block = easyBlocksTree(_blockIndex);
        const auto& desc = easyDescriptor(block.node->id());
        if (desc.type() == profiler::BlockType::Value)
        {
            auto valueItem = new ArbitraryTreeWidgetItem(rootItem, desc.color(), block.value->value_id());
            valueItem->setText(int_cast(ArbitraryColumns::Type), profiler_gui::valueTypeString(*block.value));
            valueItem->setText(int_cast(ArbitraryColumns::Name), desc.name());
            valueItem->setText(int_cast(ArbitraryColumns::Vin), QString("0x%1").arg(block.value->value_id(), 0, 16));
            valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*block.value));

            const auto sizeHintWidth = valueItem->sizeHint(CheckColumn).width();
            valueItem->setWidthHint(std::max(sizeHintWidth, fm.width(valueItem->text(CheckColumn))) + 32);

            return rootItem;
        }

        _blockId = block.node->id();
    }

    const bool noId = profiler_gui::is_max(_blockId);
    QTreeWidgetItem* blockItem = nullptr;
    if (!noId)
    {
        blockItem = new QTreeWidgetItem(rootItem, StdItemType);
        blockItem->setText(int_cast(ArbitraryColumns::Type), QStringLiteral("Block"));

        if (hasConcreteBlock)
            blockItem->setText(int_cast(ArbitraryColumns::Name), easyBlockName(_blockIndex));
        else
            blockItem->setText(int_cast(ArbitraryColumns::Name), easyDescriptor(_blockId).name());

        blockItem->setData(int_cast(ArbitraryColumns::Type), Qt::UserRole, 1);
        blockItem->setData(int_cast(ArbitraryColumns::Vin), Qt::UserRole, _blockId);
    }

    std::unordered_map<profiler::block_id_t, QTreeWidgetItem*, estd::hash<profiler::block_id_t> > blocks;
    std::unordered_map<profiler::vin_t, UsedValueTypes, estd::hash<profiler::vin_t> > vins;
    std::unordered_map<std::string, UsedValueTypes> names;

    std::vector<profiler::block_index_t> stack;
    for (auto childIndex : _threadRoot.children)
    {
        stack.push_back(childIndex);
        while (!stack.empty())
        {
            const auto i = stack.back();
            stack.pop_back();

            const auto& block = easyBlocksTree(i);
            if (noId || block.node->id() == _blockId || easyDescriptor(block.node->id()).id() == _blockId)
            {
                for (auto c : block.children)
                {
                    if (noId)
                        stack.push_back(c);

                    const auto& child = easyBlocksTree(c);
                    const auto& desc = easyDescriptor(child.node->id());
                    if (desc.type() != profiler::BlockType::Value)
                        continue;

                    if (blockItem == nullptr)
                    {
                        const auto id = block.node->id();
                        auto it = blocks.find(id);
                        if (it != blocks.end())
                        {
                            blockItem = it->second;
                        }
                        else
                        {
                            blockItem = new QTreeWidgetItem(rootItem, StdItemType);
                            blockItem->setText(int_cast(ArbitraryColumns::Type), QStringLiteral("Block"));
                            blockItem->setText(int_cast(ArbitraryColumns::Name), easyBlockName(block));
                            blockItem->setData(int_cast(ArbitraryColumns::Type), Qt::UserRole, 1);
                            blockItem->setData(int_cast(ArbitraryColumns::Vin), Qt::UserRole, id);
                            blocks.emplace(id, blockItem);
                        }
                    }

                    const auto typeIndex = int_cast(child.value->type());
                    auto vin = child.value->value_id();

                    ArbitraryTreeWidgetItem** usedItems = nullptr;
                    ArbitraryTreeWidgetItem* valueItem = nullptr;
                    if (vin == 0)
                    {
                        auto result = names.emplace(desc.name(), 0);
                        usedItems = result.first->second.items;
                        if (!result.second && (valueItem = *(usedItems + typeIndex)))
                        {
                            if (i == _blockIndex)
                                valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*child.value));
                            continue; // already in set
                        }
                    }
                    else
                    {
                        auto result = vins.emplace(vin, 0);
                        usedItems = result.first->second.items;
                        if (!result.second && (valueItem = *(usedItems + typeIndex)))
                        {
                            if (i == _blockIndex)
                                valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*child.value));
                            continue; // already in set
                        }
                    }

                    valueItem = new ArbitraryTreeWidgetItem(blockItem, desc.color(), vin);
                    valueItem->setText(int_cast(ArbitraryColumns::Type), profiler_gui::valueTypeString(*child.value));
                    valueItem->setText(int_cast(ArbitraryColumns::Name), desc.name());
                    valueItem->setText(int_cast(ArbitraryColumns::Vin), QString("0x%1").arg(vin, 0, 16));

                    if (i == _blockIndex)
                        valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*child.value));

                    const auto sizeHintWidth = valueItem->sizeHint(CheckColumn).width();
                    valueItem->setWidthHint(std::max(sizeHintWidth, fm.width(valueItem->text(CheckColumn))) + 32);

                    *(usedItems + typeIndex) = valueItem;
                }

                if (noId)
                    blockItem = nullptr;
            }
            else
            {
                for (auto c : block.children)
                    stack.push_back(c);
            }
        }
    }

    return rootItem;
}

void ArbitraryValuesWidget::loadSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("ArbitraryValuesWidget");

    auto geometry = settings.value("hsplitter/geometry").toByteArray();
    if (!geometry.isEmpty())
        m_splitter->restoreGeometry(geometry);

    auto state = settings.value("hsplitter/state").toByteArray();
    if (!state.isEmpty())
        m_splitter->restoreState(state);

    settings.endGroup();
}

void ArbitraryValuesWidget::saveSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("ArbitraryValuesWidget");
    settings.setValue("hsplitter/geometry", m_splitter->saveGeometry());
    settings.setValue("hsplitter/state", m_splitter->saveState());
    settings.endGroup();
}
