/************************************************************************
* file name         : easy_graphics_scrollbar.cpp
* ----------------- :
* creation time     : 2016/07/04
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : .
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

#include <algorithm>
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include "easy_graphics_scrollbar.h"
#include "globals.h"


// TODO: use profiler_core/spin_lock.h

#if defined(_WIN32) && defined(EASY_GUI_USE_CRITICAL_SECTION)
# include <Windows.h>
# ifdef min
#  undef min
# endif
# ifdef max
#  undef max
# endif

namespace profiler_gui {
    void spin_lock::lock() {
        EnterCriticalSection((CRITICAL_SECTION*)m_lock);
    }

    void spin_lock::unlock() {
        LeaveCriticalSection((CRITICAL_SECTION*)m_lock);
    }

    spin_lock::spin_lock() : m_lock(new CRITICAL_SECTION) {
        InitializeCriticalSection((CRITICAL_SECTION*)m_lock);
    }

    spin_lock::~spin_lock() {
        DeleteCriticalSection((CRITICAL_SECTION*)m_lock);
        delete ((CRITICAL_SECTION*)m_lock);
    }
}
#endif

//////////////////////////////////////////////////////////////////////////

const int DEFAULT_TOP = -40;
const int DEFAULT_HEIGHT = 80;
const int INDICATOR_SIZE = 6;
const int INDICATOR_SIZE_x2 = INDICATOR_SIZE << 1;
const int HYST_COLUMN_MIN_HEIGHT = 3;

//////////////////////////////////////////////////////////////////////////

inline qreal clamp(qreal _minValue, qreal _value, qreal _maxValue)
{
    return (_value < _minValue ? _minValue : (_value > _maxValue ? _maxValue : _value));
}

inline qreal sqr(qreal _value)
{
    return _value * _value;
}

inline qreal calculate_color1(qreal h, qreal, qreal k)
{
    return ::std::min(h * k, 0.9999999);
}

inline qreal calculate_color2(qreal, qreal duration, qreal k)
{
    return ::std::min(sqr(sqr(duration)) * k, 0.9999999);
}

//////////////////////////////////////////////////////////////////////////

EasyGraphicsSliderItem::EasyGraphicsSliderItem(bool _main) : Parent(), m_halfwidth(0), m_bMain(_main)
{
    m_indicator.reserve(3);

    if (_main)
    {
        m_indicator.push_back(QPointF(0, DEFAULT_TOP + INDICATOR_SIZE));
        m_indicator.push_back(QPointF(-INDICATOR_SIZE, DEFAULT_TOP));
        m_indicator.push_back(QPointF(INDICATOR_SIZE, DEFAULT_TOP));
    }
    else
    {
        m_indicator.push_back(QPointF(0, DEFAULT_TOP + DEFAULT_HEIGHT - INDICATOR_SIZE));
        m_indicator.push_back(QPointF(-INDICATOR_SIZE, DEFAULT_TOP + DEFAULT_HEIGHT));
        m_indicator.push_back(QPointF(INDICATOR_SIZE, DEFAULT_TOP + DEFAULT_HEIGHT));
    }

    setWidth(1);
    setBrush(Qt::SolidPattern);
}

EasyGraphicsSliderItem::~EasyGraphicsSliderItem()
{

}

void EasyGraphicsSliderItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    if (static_cast<const EasyGraphicsScrollbar*>(scene()->parent())->bindMode())
    {
        return;
    }

    const auto currentScale = static_cast<const EasyGraphicsScrollbar*>(scene()->parent())->getWindowScale();
    const auto br = rect();

    qreal w = width() * currentScale;
    QRectF r(br.left() * currentScale, br.top() + INDICATOR_SIZE, w, br.height() - INDICATOR_SIZE_x2);
    const auto r_right = r.right();
    const auto r_bottom = r.bottom();
    auto b = brush();

    _painter->save();
    _painter->setTransform(QTransform::fromScale(1.0 / currentScale, 1), true);
    _painter->setBrush(b);
    
    if (w > 1)
    {
        _painter->setPen(Qt::NoPen);
        _painter->drawRect(r);

        // Draw left and right borders
        auto cmode = _painter->compositionMode();
        if (m_bMain) _painter->setCompositionMode(QPainter::CompositionMode_Exclusion);
        _painter->setPen(QColor::fromRgba(0xe0000000 | b.color().rgb()));
        _painter->drawLine(QPointF(r.left(), r.top()), QPointF(r.left(), r_bottom));
        _painter->drawLine(QPointF(r_right, r.top()), QPointF(r_right, r_bottom));
        if (!m_bMain) _painter->setCompositionMode(cmode);
    }
    else
    {
        _painter->setPen(QColor::fromRgba(0xe0000000 | b.color().rgb()));
        _painter->drawLine(QPointF(r.left(), r.top()), QPointF(r.left(), r_bottom));
        if (m_bMain) _painter->setCompositionMode(QPainter::CompositionMode_Exclusion);
    }

    // Draw triangle indicators for small slider
    _painter->setTransform(QTransform::fromTranslate(r.left() + w * 0.5, 0), true);
    _painter->setPen(b.color().rgb());
    _painter->drawPolygon(m_indicator);

    _painter->restore();
}

qreal EasyGraphicsSliderItem::width() const
{
    return m_halfwidth * 2.0;
}

qreal EasyGraphicsSliderItem::halfwidth() const
{
    return m_halfwidth;
}

void EasyGraphicsSliderItem::setWidth(qreal _width)
{
    m_halfwidth = _width * 0.5;
    setRect(-m_halfwidth, DEFAULT_TOP, _width, DEFAULT_HEIGHT);
}

void EasyGraphicsSliderItem::setHalfwidth(qreal _halfwidth)
{
    m_halfwidth = _halfwidth;
    setRect(-m_halfwidth, DEFAULT_TOP, m_halfwidth * 2.0, DEFAULT_HEIGHT);
}

void EasyGraphicsSliderItem::setColor(QRgb _color)
{
    setColor(QColor::fromRgba(_color));
}

void EasyGraphicsSliderItem::setColor(const QColor& _color)
{
    auto b = brush();
    b.setColor(_color);
    setBrush(b);
}

//////////////////////////////////////////////////////////////////////////

EasyHystogramItem::EasyHystogramItem() : Parent(nullptr)
    , m_threadDuration(0)
    , m_threadProfiledTime(0)
    , m_threadWaitTime(0)
    , m_pSource(nullptr)
    , m_temporaryImage(nullptr)
    , m_maxDuration(0)
    , m_minDuration(0)
    , m_timer(::std::bind(&This::onTimeout, this))
    , m_pProfilerThread(nullptr)
    , m_threadId(0)
    , m_blockId(::profiler_gui::numeric_max<decltype(m_blockId)>())
    , m_timeouts(0)
    , m_timeUnits(::profiler_gui::TimeUnits_auto)
    , m_regime(Hyst_Pointer)
    , m_bUpdatingImage(false)
{
    m_bReady = ATOMIC_VAR_INIT(false);
}

EasyHystogramItem::~EasyHystogramItem()
{
    m_bReady.store(true, ::std::memory_order_release);
    if (m_workerThread.joinable())
        m_workerThread.join();
    delete m_temporaryImage;
}

QRectF EasyHystogramItem::boundingRect() const
{
    return m_boundingRect;
}

void EasyHystogramItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    if ((m_regime == Hyst_Pointer && m_pSource == nullptr) || (m_regime == Hyst_Id && (m_threadId == 0 || ::profiler_gui::is_max(m_blockId))))
        return;

    if (!m_bReady.load(::std::memory_order_acquire))
    {
        const auto currentScale = static_cast<const EasyGraphicsScrollbar*>(scene()->parent())->getWindowScale();
        const auto bottom = m_boundingRect.bottom();
        const auto width = m_boundingRect.width() * currentScale;

        _painter->save();
        _painter->setTransform(QTransform::fromScale(1.0 / currentScale, 1), true);

        const auto h = _painter->fontMetrics().height();

        _painter->setPen(Qt::black);
        _painter->drawText(QRectF(0, m_boundingRect.top(), m_boundingRect.width() * currentScale, m_boundingRect.height() - h),
                           Qt::AlignCenter, "Generating image");
        _painter->drawText(QRectF(0, m_boundingRect.top() + h, m_boundingRect.width() * currentScale, m_boundingRect.height() - h),
                           Qt::AlignCenter, QString(m_timeouts, QChar('.')));

        _painter->setPen(Qt::darkGray);
        _painter->drawLine(QLineF(0, bottom, width, bottom));
        _painter->drawLine(QLineF(0, m_boundingRect.top(), width, m_boundingRect.top()));

        _painter->restore();

        return;
    }

    if (m_regime == Hyst_Pointer)
        paintByPtr(_painter);
    else
        paintById(_painter);
}

void EasyHystogramItem::paintByPtr(QPainter* _painter)
{
    const auto widget = static_cast<const EasyGraphicsScrollbar*>(scene()->parent());
    const bool bindMode = widget->bindMode();
    const auto currentScale = widget->getWindowScale();
    const auto bottom = m_boundingRect.bottom();
    const auto width = m_boundingRect.width() * currentScale;
    const auto dtime = m_maxDuration - m_minDuration;
    const auto coeff = (m_boundingRect.height() - HYST_COLUMN_MIN_HEIGHT) / (dtime > 1e-3 ? dtime : 1.);

    QRectF rect;
    QBrush brush(Qt::SolidPattern);
    QRgb previousColor = 0;

    _painter->save();
    _painter->setPen(Qt::NoPen);
    _painter->setTransform(QTransform::fromScale(1.0 / currentScale, 1), true);

    if (!m_pSource->empty())
    {
        if (!bindMode)
            _painter->drawImage(0, m_boundingRect.top(), m_mainImage);
        else
        {
            const auto range = widget->sliderWidth();
            const auto minimum = widget->value();
            const auto slider_k = widget->range() / range;

            if (slider_k < 8)
            {
                _painter->setTransform(QTransform::fromScale(slider_k, 1), true);
                _painter->drawImage(-minimum * currentScale, m_boundingRect.top(), m_mainImage);
                _painter->setTransform(QTransform::fromScale(1./slider_k, 1), true);
            }
            else
            {
                const bool gotFrame = EASY_GLOBALS.frame_time > 1e-6f;
                qreal frameCoeff = 1;
                if (gotFrame)
                {
                    if (EASY_GLOBALS.frame_time <= m_minDuration)
                        frameCoeff = m_boundingRect.height();
                    else
                        frameCoeff = 0.9 / EASY_GLOBALS.frame_time;
                }

                auto const calculate_color = gotFrame ? calculate_color2 : calculate_color1;
                auto const k = gotFrame ? sqr(sqr(frameCoeff)) : 1.0 / m_boundingRect.height();

                const auto& items = *m_pSource;
                const auto maximum = minimum + range;
                const auto realScale = currentScale * slider_k;
                const auto offset = minimum * realScale;

                auto first = ::std::lower_bound(items.begin(), items.end(), minimum, [](const ::profiler_gui::EasyBlockItem& _item, qreal _value)
                {
                    return _item.left() < _value;
                });

                if (first != items.end())
                {
                    if (first != items.begin())
                        --first;
                }
                else
                {
                    first = items.begin() + items.size() - 1;
                }

                qreal previous_x = -1e30, previous_h = -1e30;
                for (auto it = first, end = items.end(); it != end; ++it)
                {
                    // Draw rectangle

                    if (it->left() > maximum)
                        break;

                    if (it->right() < minimum)
                        continue;

                    const qreal item_x = it->left() * realScale - offset;
                    const qreal item_w = ::std::max(it->width() * realScale, 1.0);
                    const qreal item_r = item_x + item_w;
                    const qreal h = it->width() > m_minDuration ? (HYST_COLUMN_MIN_HEIGHT + (it->width() - m_minDuration) * coeff) : HYST_COLUMN_MIN_HEIGHT;

                    if (h < previous_h && item_r < previous_x)
                        continue;

                    const auto col = calculate_color(h, it->width(), k);
                    const auto color = 0x00ffffff & QColor::fromHsvF((1.0 - col) * 0.375, 0.85, 0.85).rgb();

                    if (previousColor != color)
                    {
                        // Set background color brush for rectangle
                        previousColor = color;
                        brush.setColor(QColor::fromRgba(0xc0000000 | color));
                        _painter->setBrush(brush);
                    }

                    rect.setRect(item_x, bottom - h, item_w, h);
                    _painter->drawRect(rect);

                    previous_x = item_r;
                    previous_h = h;
                }
            }
        }
    }

    qreal top_width = width, bottom_width = width;
    int font_h = 0;
    if (!m_maxDurationStr.isEmpty())
    {
        rect.setRect(0, m_boundingRect.top() - INDICATOR_SIZE, width - 3, m_boundingRect.height() + INDICATOR_SIZE_x2);

        if (m_timeUnits != EASY_GLOBALS.time_units)
        {
            m_timeUnits = EASY_GLOBALS.time_units;
            m_maxDurationStr = ::profiler_gui::timeStringReal(m_timeUnits, m_maxDuration, 3);
            m_minDurationStr = ::profiler_gui::timeStringReal(m_timeUnits, m_minDuration, 3);
        }

        auto fm = _painter->fontMetrics();
        font_h = fm.height();
        //bottom_width -= fm.width(m_minDurationStr) + 7;
        top_width -= fm.width(m_maxDurationStr) + 7;

        _painter->setPen(Qt::black);
        _painter->drawText(rect, Qt::AlignRight | Qt::AlignTop, m_maxDurationStr);

        rect.setRect(0, bottom, width - 3, font_h);
        _painter->drawText(rect, Qt::AlignRight | Qt::AlignTop, m_minDurationStr);
    }

    _painter->setPen(Qt::darkGray);
    _painter->drawLine(QLineF(0, bottom, bottom_width, bottom));
    _painter->drawLine(QLineF(0, m_boundingRect.top(), top_width, m_boundingRect.top()));

    if (m_minDuration < EASY_GLOBALS.frame_time && EASY_GLOBALS.frame_time < m_maxDuration)
    {
        // Draw marker displaying required frame_time step
        const auto h = bottom - HYST_COLUMN_MIN_HEIGHT - (EASY_GLOBALS.frame_time - m_minDuration) * coeff;
        _painter->setPen(Qt::DashLine);

        auto w = width;
        const auto boundary = INDICATOR_SIZE - font_h;
        if (h < (m_boundingRect.top() - boundary))
            w = top_width;
        else if (h > (bottom + boundary))
            w = bottom_width;

        _painter->drawLine(QLineF(0, h, w, h));
    }

    _painter->setPen(Qt::black);
    rect.setRect(0, bottom + 2, width, widget->defaultFontHeight());
    _painter->drawText(rect, Qt::AlignHCenter | Qt::TextDontClip, QString("%1  |  duration: %2  |  profiled time: %3 (%4%)  |  wait time: %5 (%6%)  |  %7 blocks (%8 events)").arg(m_threadName)
                       .arg(::profiler_gui::timeStringRealNs(EASY_GLOBALS.time_units, m_threadDuration))
                       .arg(::profiler_gui::timeStringRealNs(EASY_GLOBALS.time_units, m_threadProfiledTime))
                       .arg(m_threadDuration ? QString::number(100. * (double)m_threadProfiledTime / (double)m_threadDuration, 'f', 2) : QString("0"))
                       .arg(::profiler_gui::timeStringRealNs(EASY_GLOBALS.time_units, m_threadWaitTime))
                       .arg(m_threadDuration ? QString::number(100. * (double)m_threadWaitTime / (double)m_threadDuration, 'f', 2) : QString("0"))
                       .arg(m_pProfilerThread->blocks_number)
                       .arg(m_pProfilerThread->events.size()));

    _painter->drawText(rect, Qt::AlignLeft, bindMode ? " MODE: zoom" : " MODE: overview");

    _painter->restore();
}

void EasyHystogramItem::paintById(QPainter* _painter)
{
    const auto widget = static_cast<const EasyGraphicsScrollbar*>(scene()->parent());
    const bool bindMode = widget->bindMode();
    const auto currentScale = widget->getWindowScale();
    const auto bottom = m_boundingRect.bottom();
    const auto width = m_boundingRect.width() * currentScale;
    const auto dtime = m_maxDuration - m_minDuration;
    const auto coeff = (m_boundingRect.height() - HYST_COLUMN_MIN_HEIGHT) / (dtime > 1e-3 ? dtime : 1.);

    QRectF rect;
    QBrush brush(Qt::SolidPattern);
    QRgb previousColor = 0;

    _painter->save();
    _painter->setPen(Qt::NoPen);
    _painter->setTransform(QTransform::fromScale(1.0 / currentScale, 1), true);

    const auto& items = m_selectedBlocks;
    if (!items.empty())
    {
        if (!bindMode)
            _painter->drawImage(0, m_boundingRect.top(), m_mainImage);
        else
        {
            const auto range = widget->sliderWidth();
            auto minimum = widget->value();
            const auto slider_k = widget->range() / range;

            if (slider_k < 8)
            {
                _painter->setTransform(QTransform::fromScale(slider_k, 1), true);
                _painter->drawImage(-minimum * currentScale, m_boundingRect.top(), m_mainImage);
                _painter->setTransform(QTransform::fromScale(1. / slider_k, 1), true);
            }
            else
            {
                minimum *= 1e3;
                const auto maximum = minimum + range * 1e3;
                const auto realScale = currentScale * slider_k;
                const auto offset = minimum * realScale;

                auto first = ::std::lower_bound(items.begin(), items.end(), minimum + EASY_GLOBALS.begin_time, [](::profiler::block_index_t _item, qreal _value)
                {
                    return easyBlock(_item).tree.node->begin() < _value;
                });

                if (first != items.end())
                {
                    if (first != items.begin())
                        --first;
                }
                else
                {
                    first = items.begin() + (items.size() - 1);
                }

                auto last = ::std::upper_bound(first, items.end(), maximum + EASY_GLOBALS.begin_time, [](qreal _value, ::profiler::block_index_t _item)
                {
                    return _value < easyBlock(_item).tree.node->begin();
                });

                const auto n = static_cast<uint32_t>(::std::distance(first, last));

                if (n > 0)
                {
                    const bool gotFrame = EASY_GLOBALS.frame_time > 1e-6f;
                    qreal frameCoeff = 1;
                    if (gotFrame)
                    {
                        if (EASY_GLOBALS.frame_time <= m_minDuration)
                            frameCoeff = m_boundingRect.height();
                        else
                            frameCoeff = 0.9 / EASY_GLOBALS.frame_time;
                    }

                    auto const calculate_color = gotFrame ? calculate_color2 : calculate_color1;
                    auto const k = gotFrame ? sqr(sqr(frameCoeff)) : 1.0 / m_boundingRect.height();

                    const auto draw = [this, &previousColor, &brush, &_painter](qreal x, qreal y, qreal w, qreal h, QRgb color)
                    {
                        m_spin.lock();

                        if (previousColor != color)
                        {
                            // Set background color brush for rectangle
                            previousColor = color;
                            brush.setColor(QColor::fromRgba(0xc0000000 | color));
                            _painter->setBrush(brush);
                        }

                        _painter->drawRect(QRectF(x, y, w, h));

                        m_spin.unlock();
                    };

                    ::std::vector<::std::thread> threads;
                    const auto n_threads = ::std::min(n, ::std::thread::hardware_concurrency());
                    threads.reserve(n_threads);
                    const auto n_items = n / n_threads;
                    for (uint32_t i = 0; i < n_threads; ++i)
                    {
                        auto begin = first + i * n_items;
                        threads.emplace_back([this, &draw, &maximum, &minimum, &realScale, &offset, &coeff, &calculate_color, &k, &bottom](decltype(begin) it, decltype(begin) end)
                        {
                            qreal previous_x = -1e30, previous_h = -1e30;

                            //for (auto it = first, end = items.end(); it != end; ++it)
                            for (; it != end; ++it)
                            {
                                // Draw rectangle
                                const auto item = easyBlock(*it).tree.node;

                                const auto beginTime = item->begin() - EASY_GLOBALS.begin_time;
                                if (beginTime > maximum)
                                    break;

                                const auto endTime = item->end() - EASY_GLOBALS.begin_time;
                                if (endTime < minimum)
                                    continue;

                                const qreal duration = item->duration() * 1e-3;
                                const qreal item_x = (beginTime * realScale - offset) * 1e-3;
                                const qreal item_w = ::std::max(duration * realScale, 1.0);
                                const qreal item_r = item_x + item_w;
                                const qreal h = duration > m_minDuration ? (HYST_COLUMN_MIN_HEIGHT + (duration - m_minDuration) * coeff) : HYST_COLUMN_MIN_HEIGHT;

                                if (h < previous_h && item_r < previous_x)
                                    continue;

                                const auto col = calculate_color(h, duration, k);
                                const auto color = 0x00ffffff & QColor::fromHsvF((1.0 - col) * 0.375, 0.85, 0.85).rgb();

                                draw(item_x, bottom - h, item_w, h, color);
                                //if (previousColor != color)
                                //{
                                //    // Set background color brush for rectangle
                                //    previousColor = color;
                                //    brush.setColor(QColor::fromRgba(0xc0000000 | color));
                                //    _painter->setBrush(brush);
                                //}

                                //rect.setRect(item_x, bottom - h, item_w, h);
                                //_painter->drawRect(rect);

                                previous_x = item_r;
                                previous_h = h;
                            }
                        }, begin, i == (n_threads - 1) ? items.end() : begin + n_items);
                    }

                    for (auto& t : threads)
                        t.join();
                }
            }
        }
    }

    qreal top_width = width, bottom_width = width;
    int font_h = 0;
    if (!m_maxDurationStr.isEmpty())
    {
        rect.setRect(0, m_boundingRect.top() - INDICATOR_SIZE, width - 3, m_boundingRect.height() + INDICATOR_SIZE_x2);

        if (m_timeUnits != EASY_GLOBALS.time_units)
        {
            m_timeUnits = EASY_GLOBALS.time_units;
            m_maxDurationStr = ::profiler_gui::timeStringReal(m_timeUnits, m_maxDuration, 3);
            m_minDurationStr = ::profiler_gui::timeStringReal(m_timeUnits, m_minDuration, 3);
        }

        auto fm = _painter->fontMetrics();
        font_h = fm.height();
        //bottom_width -= fm.width(m_minDurationStr) + 7;
        top_width -= fm.width(m_maxDurationStr) + 7;

        _painter->setPen(Qt::black);
        _painter->drawText(rect, Qt::AlignRight | Qt::AlignTop, m_maxDurationStr);

        rect.setRect(0, bottom, width - 3, font_h);
        _painter->drawText(rect, Qt::AlignRight | Qt::AlignTop, m_minDurationStr);
    }

    _painter->setPen(Qt::darkGray);
    _painter->drawLine(QLineF(0, bottom, bottom_width, bottom));
    _painter->drawLine(QLineF(0, m_boundingRect.top(), top_width, m_boundingRect.top()));

    if (m_minDuration < EASY_GLOBALS.frame_time && EASY_GLOBALS.frame_time < m_maxDuration)
    {
        // Draw marker displaying required frame_time step
        const auto h = bottom - HYST_COLUMN_MIN_HEIGHT - (EASY_GLOBALS.frame_time - m_minDuration) * coeff;
        _painter->setPen(Qt::DashLine);

        auto w = width;
        const auto boundary = INDICATOR_SIZE - font_h;
        if (h < (m_boundingRect.top() - boundary))
            w = top_width;
        else if (h >(bottom + boundary))
            w = bottom_width;

        _painter->drawLine(QLineF(0, h, w, h));
    }

    _painter->setPen(Qt::black);
    rect.setRect(0, bottom + 2, width, widget->defaultFontHeight());

    const auto* item = !::profiler_gui::is_max(EASY_GLOBALS.selected_block) ? &easyBlock(EASY_GLOBALS.selected_block) : (!m_selectedBlocks.empty() ? &easyBlock(m_selectedBlocks.front()) : nullptr);
    if (item != nullptr)
    {
        const auto name = *item->tree.node->name() != 0 ? item->tree.node->name() : easyDescriptor(item->tree.node->id()).name();
        if (item->tree.per_thread_stats != nullptr)
        {
            _painter->drawText(rect, Qt::AlignHCenter | Qt::TextDontClip, QString("%1  |  %2  |  %3 calls  |  %4% of thread profiled time").arg(m_threadName).arg(name)
                               .arg(item->tree.per_thread_stats->calls_number)
                               .arg(m_threadProfiledTime ? QString::number(100. * (double)item->tree.per_thread_stats->total_duration / (double)m_threadProfiledTime, 'f', 2) : QString("100")));
        }
        else
        {
            _painter->drawText(rect, Qt::AlignHCenter | Qt::TextDontClip, QString("%1  |  %2  |  %3 calls").arg(m_threadName).arg(name)
                               .arg(m_selectedBlocks.size()));
        }
    }
    else
    {
        _painter->drawText(rect, Qt::AlignHCenter | Qt::TextDontClip, QString("%1  |  %2  |  %3 calls").arg(m_threadName).arg(easyDescriptor(m_blockId).name())
                           .arg(m_selectedBlocks.size()));
    }

    _painter->drawText(rect, Qt::AlignLeft, bindMode ? " MODE: zoom" : " MODE: overview");

    _painter->restore();
}

::profiler::thread_id_t EasyHystogramItem::threadId() const
{
    return m_threadId;
}

void EasyHystogramItem::setBoundingRect(const QRectF& _rect)
{
    m_boundingRect = _rect;
}

void EasyHystogramItem::setBoundingRect(qreal x, qreal y, qreal w, qreal h)
{
    m_boundingRect.setRect(x, y, w, h);
}

void EasyHystogramItem::setSource(::profiler::thread_id_t _thread_id, const ::profiler_gui::EasyItems* _items)
{
    if (m_regime == Hyst_Pointer && m_threadId == _thread_id && m_pSource == _items)
        return;

    if (m_timer.isActive())
        m_timer.stop();

    m_bReady.store(true, ::std::memory_order_release);
    if (m_workerThread.joinable())
        m_workerThread.join();

    delete m_temporaryImage;
    m_temporaryImage = nullptr;

    m_selectedBlocks.clear();
    ::profiler::BlocksTree::children_t().swap(m_selectedBlocks);

    m_bUpdatingImage = true;
    m_regime = Hyst_Pointer;
    m_pSource = _items;
    m_threadId = _thread_id;
    ::profiler_gui::set_max(m_blockId);

    if (m_pSource != nullptr)
    {
        if (m_pSource->empty())
        {
            m_pSource = nullptr;
        }
        else
        {
            m_maxDuration = 0;
            m_minDuration = 1e30;
            for (const auto& item : *m_pSource)
            {
                const auto w = item.width();

                if (w > m_maxDuration)
                    m_maxDuration = w;

                if (w < m_minDuration && easyDescriptor(easyBlock(item.block).tree.node->id()).type() != ::profiler::BLOCK_TYPE_EVENT)
                    m_minDuration = w;
            }
        }
    }

    if (m_pSource == nullptr)
    {
        m_pProfilerThread = nullptr;
        m_maxDurationStr.clear();
        m_minDurationStr.clear();
        m_threadName.clear();
        hide();
    }
    else
    {
        const auto& root = EASY_GLOBALS.profiler_blocks[_thread_id];
        m_threadName = ::profiler_gui::decoratedThreadName(EASY_GLOBALS.use_decorated_thread_name, root);

        if (root.children.empty())
            m_threadDuration = 0;
        else
            m_threadDuration = easyBlock(root.children.back()).tree.node->end() - easyBlock(root.children.front()).tree.node->begin();
        m_threadProfiledTime = root.profiled_time;
        m_threadWaitTime = root.wait_time;

        m_pProfilerThread = &root;

        m_timeUnits = EASY_GLOBALS.time_units;
        m_maxDurationStr = ::profiler_gui::timeStringReal(m_timeUnits, m_maxDuration, 3);
        m_minDurationStr = ::profiler_gui::timeStringReal(m_timeUnits, m_minDuration, 3);
        updateImage();
        show();
    }
}

void EasyHystogramItem::setSource(::profiler::thread_id_t _thread_id, ::profiler::block_id_t _block_id)
{
    if (m_regime == Hyst_Id && m_threadId == _thread_id && m_blockId == _block_id)
        return;

    m_bUpdatingImage = false;
    m_regime = Hyst_Id;
    m_pSource = nullptr;
    m_maxDurationStr.clear();
    m_minDurationStr.clear();

    if (m_timer.isActive())
        m_timer.stop();

    m_bReady.store(true, ::std::memory_order_release);
    if (m_workerThread.joinable())
        m_workerThread.join();

    delete m_temporaryImage;
    m_temporaryImage = nullptr;

    m_selectedBlocks.clear();
    ::profiler::BlocksTree::children_t().swap(m_selectedBlocks);

    m_threadId = _thread_id;
    m_blockId = _block_id;

    if (m_threadId != 0 && !::profiler_gui::is_max(m_blockId))
    {
        const auto& root = EASY_GLOBALS.profiler_blocks[_thread_id];
        m_threadName = ::profiler_gui::decoratedThreadName(EASY_GLOBALS.use_decorated_thread_name, root);

        if (root.children.empty())
            m_threadDuration = 0;
        else
            m_threadDuration = easyBlock(root.children.back()).tree.node->end() - easyBlock(root.children.front()).tree.node->begin();
        m_threadProfiledTime = root.profiled_time;
        m_threadWaitTime = root.wait_time;

        m_pProfilerThread = &root;

        show();
        m_timeUnits = EASY_GLOBALS.time_units;
        m_bReady.store(false, ::std::memory_order_release);
        m_workerThread = ::std::thread([this](decltype(root) profiler_thread)
        {
            typedef ::std::vector<::std::pair<::profiler::block_index_t, ::profiler::block_index_t> > Stack;

            m_maxDuration = 0;
            m_minDuration = 1e30;
            //const auto& profiler_thread = EASY_GLOBALS.profiler_blocks[m_threadId];
            Stack stack;
            stack.reserve(profiler_thread.depth);

            for (auto frame : profiler_thread.children)
            {
                const auto& frame_block = easyBlock(frame).tree;
                if (frame_block.node->id() == m_blockId)
                {
                    m_selectedBlocks.push_back(frame);

                    const auto w = frame_block.node->duration();
                    if (w > m_maxDuration)
                        m_maxDuration = w;
                    if (w < m_minDuration)
                        m_minDuration = w;
                }

                stack.push_back(::std::make_pair(frame, 0U));
                while (!stack.empty() && !m_bReady.load(::std::memory_order_acquire))
                {
                    auto& top = stack.back();
                    const auto& top_children = easyBlock(top.first).tree.children;
                    const auto stack_size = stack.size();
                    for (auto end = top_children.size(); top.second < end && !m_bReady.load(::std::memory_order_acquire); ++top.second)
                    {
                        const auto child_index = top_children[top.second];
                        const auto& child = easyBlock(child_index).tree;
                        if (child.node->id() == m_blockId)
                        {
                            m_selectedBlocks.push_back(child_index);

                            const auto w = child.node->duration();
                            if (w > m_maxDuration)
                                m_maxDuration = w;
                            if (w < m_minDuration)
                                m_minDuration = w;
                        }

                        if (!child.children.empty())
                        {
                            ++top.second;
                            stack.push_back(::std::make_pair(child_index, 0U));
                            break;
                        }
                    }

                    if (stack_size == stack.size())
                    {
                        stack.pop_back();
                    }
                }
            }

            if (m_selectedBlocks.empty())
            {
                m_maxDurationStr.clear();
                m_minDurationStr.clear();
            }
            else
            {
                m_maxDuration *= 1e-3;
                m_minDuration *= 1e-3;
                m_maxDurationStr = ::profiler_gui::timeStringReal(m_timeUnits, m_maxDuration, 3);
                m_minDurationStr = ::profiler_gui::timeStringReal(m_timeUnits, m_minDuration, 3);
            }

            m_bReady.store(true, ::std::memory_order_release);
        }, std::ref(root));

        m_timeouts = 3;
        m_timer.start(500);
    }
    else
    {
        m_pProfilerThread = nullptr;
        m_threadName.clear();
        hide();
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyHystogramItem::validateName()
{
    if (m_threadName.isEmpty())
        return;
    m_threadName = ::profiler_gui::decoratedThreadName(EASY_GLOBALS.use_decorated_thread_name, EASY_GLOBALS.profiler_blocks[m_threadId]);
}

//////////////////////////////////////////////////////////////////////////

void EasyHystogramItem::onTimeout()
{
    if (!isVisible())
    {
        if (m_timer.isActive())
            m_timer.stop();
        return;
    }

    if (++m_timeouts > 8)
        m_timeouts = 3;

    if (m_bReady.load(::std::memory_order_acquire))
    {
        m_timer.stop();
        if (!m_bUpdatingImage)
        {
            m_bUpdatingImage = true;
            updateImage();
        }
        else
        {
            m_temporaryImage->swap(m_mainImage);
            delete m_temporaryImage;
            m_temporaryImage = nullptr;

            if (m_workerThread.joinable())
                m_workerThread.join();
        }
    }

    scene()->update();
}

//////////////////////////////////////////////////////////////////////////

void EasyHystogramItem::updateImage()
{
    if (!m_bUpdatingImage)
        return;

    const auto widget = static_cast<const EasyGraphicsScrollbar*>(scene()->parent());

    m_bReady.store(true, ::std::memory_order_release);
    if (m_workerThread.joinable())
        m_workerThread.join();
    m_bReady.store(false, ::std::memory_order_release);

    delete m_temporaryImage;
    m_temporaryImage = nullptr;

    m_workerThread = ::std::thread([this](HystRegime _regime, qreal _current_scale,
        qreal _minimum, qreal _maximum, qreal _range, qreal _value, qreal _width, bool _bindMode,
        float _frame_time, ::profiler::timestamp_t _begin_time)
    {
        updateImage(_regime, _current_scale, _minimum, _maximum, _range, _value, _width, _bindMode, _frame_time, _begin_time);
        m_bReady.store(true, ::std::memory_order_release);
    }, m_regime, widget->getWindowScale(), widget->minimum(), widget->maximum(), widget->range(), widget->value(), widget->sliderWidth(), widget->bindMode(), EASY_GLOBALS.frame_time, EASY_GLOBALS.begin_time);

    m_timeouts = 3;
    m_timer.start(500);
}

void EasyHystogramItem::updateImage(HystRegime _regime, qreal _current_scale,
                                    qreal _minimum, qreal _maximum, qreal _range,
                                    qreal _value, qreal _width, bool _bindMode,
                                    float _frame_time, ::profiler::timestamp_t _begin_time)
{
    const auto bottom = m_boundingRect.height();//m_boundingRect.bottom();
    const auto width = m_boundingRect.width() * _current_scale;
    const auto dtime = m_maxDuration - m_minDuration;
    const auto coeff = (m_boundingRect.height() - HYST_COLUMN_MIN_HEIGHT) / (dtime > 1e-3 ? dtime : 1.);

    m_temporaryImage = new QImage((_bindMode ? width * 2. : width) + 0.5, m_boundingRect.height(), QImage::Format_ARGB32);
    m_temporaryImage->fill(Qt::white);
    QPainter p(m_temporaryImage);
    p.setPen(Qt::NoPen);

    QRectF rect;
    QBrush brush(Qt::SolidPattern);
    QRgb previousColor = 0;

    qreal previous_x = -1e30, previous_h = -1e30, offset = 0.;
    auto realScale = _current_scale;
    auto minimum = _minimum;
    auto maximum = _maximum;

    const bool gotFrame = _frame_time > 1e-6f;
    qreal frameCoeff = 1;
    if (gotFrame)
    {
        if (_frame_time <= m_minDuration)
            frameCoeff = m_boundingRect.height();
        else
            frameCoeff = 0.9 / _frame_time;
    }

    auto const calculate_color = gotFrame ? calculate_color2 : calculate_color1;
    auto const k = gotFrame ? sqr(sqr(frameCoeff)) : 1.0 / m_boundingRect.height();

    if (_regime == Hyst_Pointer)
    {
        const auto& items = *m_pSource;
        if (items.empty())
            return;

        //if (_bindMode)
        //{
        //    minimum = _value;
        //    maximum = minimum + _width;
        //    realScale *= _range / _width;
        //    offset = minimum * realScale;

        //    auto first = ::std::lower_bound(items.begin(), items.end(), minimum, [](const ::profiler_gui::EasyBlockItem& _item, qreal _value)
        //    {
        //        return _item.left() < _value;
        //    });

        //    if (first != items.end())
        //    {
        //        if (first != items.begin())
        //            --first;
        //    }
        //    else
        //    {
        //        first = items.begin() + items.size() - 1;
        //    }
        //}

        for (auto it = items.begin(), end = items.end(); it != end; ++it)
        {
            // Draw rectangle
            if (it->left() > maximum)
                break;

            if (it->right() < minimum)
                continue;

            const qreal item_x = it->left() * realScale - offset;
            const qreal item_w = ::std::max(it->width() * realScale, 1.0);
            const qreal item_r = item_x + item_w;
            const qreal h = it->width() > m_minDuration ? (HYST_COLUMN_MIN_HEIGHT + (it->width() - m_minDuration) * coeff) : HYST_COLUMN_MIN_HEIGHT;

            if (h < previous_h && item_r < previous_x)
                continue;

            const auto col = calculate_color(h, it->width(), k);
            const auto color = 0x00ffffff & QColor::fromHsvF((1.0 - col) * 0.375, 0.85, 0.85).rgb();

            if (previousColor != color)
            {
                // Set background color brush for rectangle
                previousColor = color;
                brush.setColor(QColor::fromRgba(0xc0000000 | color));
                p.setBrush(brush);
            }

            rect.setRect(item_x, bottom - h, item_w, h);
            p.drawRect(rect);

            previous_x = item_r;
            previous_h = h;
        }
    }
    else
    {
        minimum *= 1e3;
        maximum *= 1e3;

        for (auto it = m_selectedBlocks.begin(), end = m_selectedBlocks.end(); it != end; ++it)
        {
            // Draw rectangle
            const auto item = easyBlock(*it).tree.node;

            const auto beginTime = item->begin() - _begin_time;
            if (beginTime > maximum)
                break;

            const auto endTime = item->end() - _begin_time;
            if (endTime < minimum)
                continue;

            const qreal duration = item->duration() * 1e-3;
            const qreal item_x = (beginTime * realScale - offset) * 1e-3;
            const qreal item_w = ::std::max(duration * realScale, 1.0);
            const qreal item_r = item_x + item_w;
            const auto h = duration > m_minDuration ? (HYST_COLUMN_MIN_HEIGHT + (duration - m_minDuration) * coeff) : HYST_COLUMN_MIN_HEIGHT;

            if (h < previous_h && item_r < previous_x)
                continue;

            const auto col = calculate_color(h, duration, k);
            const auto color = 0x00ffffff & QColor::fromHsvF((1.0 - col) * 0.375, 0.85, 0.85).rgb();

            if (previousColor != color)
            {
                // Set background color brush for rectangle
                previousColor = color;
                brush.setColor(QColor::fromRgba(0xc0000000 | color));
                p.setBrush(brush);
            }

            rect.setRect(item_x, bottom - h, item_w, h);
            p.drawRect(rect);

            previous_x = item_r;
            previous_h = h;
        }
    }
}

//////////////////////////////////////////////////////////////////////////

EasyGraphicsScrollbar::EasyGraphicsScrollbar(QWidget* _parent)
    : Parent(_parent)
    , m_minimumValue(0)
    , m_maximumValue(500)
    , m_value(10)
    , m_windowScale(1)
    , m_mouseButtons(Qt::NoButton)
    , m_slider(nullptr)
    , m_chronometerIndicator(nullptr)
    , m_hystogramItem(nullptr)
    , m_defaultFontHeight(0)
    , m_bScrolling(false)
    , m_bBindMode(false)
    , m_bLocked(false)
{
    setCacheMode(QGraphicsView::CacheNone);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    //setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setContentsMargins(0, 0, 0, 0);

    auto selfScene = new QGraphicsScene(this);
    m_defaultFontHeight = QFontMetrics(selfScene->font()).height();
    selfScene->setSceneRect(0, DEFAULT_TOP, 500, DEFAULT_HEIGHT + m_defaultFontHeight + 2);
    setFixedHeight(DEFAULT_HEIGHT + m_defaultFontHeight + 2);

    setScene(selfScene);

    m_hystogramItem = new EasyHystogramItem();
    m_hystogramItem->setPos(0, 0);
    m_hystogramItem->setBoundingRect(0, DEFAULT_TOP + INDICATOR_SIZE, scene()->width(), DEFAULT_HEIGHT - INDICATOR_SIZE_x2);
    selfScene->addItem(m_hystogramItem);
    m_hystogramItem->hide();

    m_chronometerIndicator = new EasyGraphicsSliderItem(false);
    m_chronometerIndicator->setPos(0, 0);
    m_chronometerIndicator->setColor(0x40000000 | ::profiler_gui::CHRONOMETER_COLOR.rgba());
    selfScene->addItem(m_chronometerIndicator);
    m_chronometerIndicator->hide();

    m_slider = new EasyGraphicsSliderItem(true);
    m_slider->setPos(0, 0);
    m_slider->setColor(0x40c0c0c0);
    selfScene->addItem(m_slider);
    m_slider->hide();

    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::timelineMarkerChanged, [this]()
    {
        if (m_hystogramItem->isVisible())
            m_hystogramItem->updateImage();
        scene()->update();
    });

    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::threadNameDecorationChanged, [this]()
    {
        if (!m_hystogramItem->isVisible())
            return;
        m_hystogramItem->validateName();
        scene()->update();
    });

    centerOn(0, 0);
}

EasyGraphicsScrollbar::~EasyGraphicsScrollbar()
{

}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::clear()
{
    setHystogramFrom(0, nullptr);
    hideChrono();
    setRange(0, 100);
    setSliderWidth(2);
    setValue(0);
}

//////////////////////////////////////////////////////////////////////////

bool EasyGraphicsScrollbar::bindMode() const
{
    return m_bBindMode;
}

qreal EasyGraphicsScrollbar::getWindowScale() const
{
    return m_windowScale;
}

::profiler::thread_id_t EasyGraphicsScrollbar::hystThread() const
{
    return m_hystogramItem->threadId();
}

qreal EasyGraphicsScrollbar::minimum() const
{
    return m_minimumValue;
}

qreal EasyGraphicsScrollbar::maximum() const
{
    return m_maximumValue;
}

qreal EasyGraphicsScrollbar::range() const
{
    return m_maximumValue - m_minimumValue;
}

qreal EasyGraphicsScrollbar::value() const
{
    return m_value;
}

qreal EasyGraphicsScrollbar::sliderWidth() const
{
    return m_slider->width();
}

qreal EasyGraphicsScrollbar::sliderHalfWidth() const
{
    return m_slider->halfwidth();
}

int EasyGraphicsScrollbar::defaultFontHeight() const
{
    return m_defaultFontHeight;
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::setValue(qreal _value)
{
    m_value = clamp(m_minimumValue, _value, ::std::max(m_minimumValue, m_maximumValue - m_slider->width()));
    m_slider->setX(m_value + m_slider->halfwidth());
    emit valueChanged(m_value);
}

void EasyGraphicsScrollbar::setRange(qreal _minValue, qreal _maxValue)
{
    const auto oldRange = range();
    const auto oldValue = oldRange < 1e-3 ? 0.0 : m_value / oldRange;

    m_minimumValue = _minValue;
    m_maximumValue = _maxValue;
    scene()->setSceneRect(_minValue, DEFAULT_TOP, _maxValue - _minValue, DEFAULT_HEIGHT + m_defaultFontHeight + 4);
    m_hystogramItem->setBoundingRect(_minValue, DEFAULT_TOP + INDICATOR_SIZE, _maxValue, DEFAULT_HEIGHT - INDICATOR_SIZE_x2);
    emit rangeChanged();

    setValue(_minValue + oldValue * range());
    onWindowWidthChange(width());
}

void EasyGraphicsScrollbar::setSliderWidth(qreal _width)
{
    m_slider->setWidth(_width);
    setValue(m_value);
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::setChronoPos(qreal _left, qreal _right)
{
    m_chronometerIndicator->setWidth(_right - _left);
    m_chronometerIndicator->setX(_left + m_chronometerIndicator->halfwidth());
}

void EasyGraphicsScrollbar::showChrono()
{
    m_chronometerIndicator->show();
}

void EasyGraphicsScrollbar::hideChrono()
{
    m_chronometerIndicator->hide();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::setHystogramFrom(::profiler::thread_id_t _thread_id, const ::profiler_gui::EasyItems* _items)
{
    if (m_bLocked)
        return;

    m_hystogramItem->setSource(_thread_id, _items);
    m_slider->setVisible(m_hystogramItem->isVisible());
    scene()->update();
}

void EasyGraphicsScrollbar::setHystogramFrom(::profiler::thread_id_t _thread_id, ::profiler::block_id_t _block_id)
{
    if (m_bLocked)
        return;

    m_hystogramItem->setSource(_thread_id, _block_id);
    m_slider->setVisible(m_hystogramItem->isVisible());
    scene()->update();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::mousePressEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();

    if (m_mouseButtons & Qt::RightButton)
    {
        m_bBindMode = !m_bBindMode;
        scene()->update();
    }

    if (m_mouseButtons & Qt::LeftButton)
    {
        m_bScrolling = true;
        m_mousePressPos = _event->pos();
        if (!m_bBindMode)
            setValue(mapToScene(m_mousePressPos).x() - m_minimumValue - m_slider->halfwidth());
    }

    _event->accept();
    //QGraphicsView::mousePressEvent(_event);
}

void EasyGraphicsScrollbar::mouseReleaseEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();
    m_bScrolling = false;
    _event->accept();
    //QGraphicsView::mouseReleaseEvent(_event);
}

void EasyGraphicsScrollbar::mouseMoveEvent(QMouseEvent* _event)
{
    if (m_mouseButtons & Qt::LeftButton)
    {
        const auto pos = _event->pos();
        const auto delta = pos - m_mousePressPos;
        m_mousePressPos = pos;

        if (m_bScrolling)
        {
            auto realScale = m_windowScale;
            if (m_bBindMode)
                realScale *= -range() / sliderWidth();
            setValue(m_value + delta.x() / realScale);
        }
    }
}

void EasyGraphicsScrollbar::wheelEvent(QWheelEvent* _event)
{
    _event->accept();

    if (!m_bBindMode)
    {
        const auto w = m_slider->halfwidth() * (_event->delta() < 0 ? ::profiler_gui::SCALING_COEFFICIENT : ::profiler_gui::SCALING_COEFFICIENT_INV);
        setValue(mapToScene(_event->pos()).x() - m_minimumValue - w);
        emit wheeled(w * m_windowScale, _event->delta());
    }
    else
    {
        const auto x = (mapToScene(_event->pos()).x() - m_minimumValue) * m_windowScale;
        emit wheeled(x, _event->delta());
    }
}

void EasyGraphicsScrollbar::resizeEvent(QResizeEvent* _event)
{
    onWindowWidthChange(_event->size().width());
}

//////////////////////////////////////////////////////////////////////////
/*
void EasyGraphicsScrollbar::contextMenuEvent(QContextMenuEvent* _event)
{
    if (EASY_GLOBALS.profiler_blocks.empty())
    {
        return;
    }

    QMenu menu;

    for (const auto& it : EASY_GLOBALS.profiler_blocks)
    {
        QString label;
        if (it.second.got_name())
            label = ::std::move(QString("%1 Thread %2").arg(it.second.name()).arg(it.first));
        else
            label = ::std::move(QString("Thread %1").arg(it.first));

        auto action = new QAction(label, nullptr);
        action->setData(it.first);
        action->setCheckable(true);
        action->setChecked(it.first == EASY_GLOBALS.selected_thread);
        connect(action, &QAction::triggered, this, &This::onThreadActionClicked);

        menu.addAction(action);
    }

    menu.exec(QCursor::pos());
    _event->accept();
}
*/
//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::onThreadActionClicked(bool)
{
    auto action = qobject_cast<QAction*>(sender());
    if (action == nullptr)
        return;

    const auto thread_id = action->data().toUInt();
    if (thread_id != EASY_GLOBALS.selected_thread)
    {
        EASY_GLOBALS.selected_thread = thread_id;
        emit EASY_GLOBALS.events.selectedThreadChanged(thread_id);
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::onWindowWidthChange(qreal _width)
{
    const auto oldScale = m_windowScale;
    const auto scrollingRange = range();

    if (scrollingRange < 1e-3)
    {
        m_windowScale = 1;
    }
    else
    {
        m_windowScale = _width / scrollingRange;
    }

    scale(m_windowScale / oldScale, 1);

    if (m_hystogramItem->isVisible())
        m_hystogramItem->updateImage();
}

//////////////////////////////////////////////////////////////////////////
