/************************************************************************
* file name         : graphics_scrollbar.cpp
* ----------------- :
* creation time     : 2016/07/04
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : .
* ----------------- :
* change log        : * 2016/07/04 Victor Zarubkin: Initial commit.
*                   :
*                   : *
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#include <algorithm>
#include <QGraphicsScene>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include "graphics_scrollbar.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////

const qreal SCALING_COEFFICIENT = 1.25;
const qreal SCALING_COEFFICIENT_INV = 1.0 / SCALING_COEFFICIENT;
const int DEFAULT_TOP = -40;
const int DEFAULT_HEIGHT = 80;
const int INDICATOR_SIZE = 8;

//////////////////////////////////////////////////////////////////////////

auto const clamp = [](qreal _minValue, qreal _value, qreal _maxValue)
{
    return (_value < _minValue ? _minValue : (_value > _maxValue ? _maxValue : _value));
};

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
    //Parent::paint(_painter, _option, _widget);
    const auto currentScale = static_cast<const EasyGraphicsScrollbar*>(scene()->parent())->getWindowScale();
    const auto br = rect();

    qreal w = width() * currentScale;
    qreal dx = 0;

//     if (w < 1.0)
//     {
//         dx = (w - 1.0) * 0.5;
//         w = 1.0;
//     }

    QRectF r(dx + br.left() * currentScale, br.top() + INDICATOR_SIZE, w, br.height() - (INDICATOR_SIZE << 1));
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

EasyMinimapItem::EasyMinimapItem() : Parent(), m_pSource(nullptr), m_maxDuration(0), m_minDuration(0), m_threadId(0)
{

}

EasyMinimapItem::~EasyMinimapItem()
{

}

QRectF EasyMinimapItem::boundingRect() const
{
    return m_boundingRect;
}

void EasyMinimapItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    if (m_pSource == nullptr)
    {
        return;
    }

    const auto currentScale = static_cast<const EasyGraphicsScrollbar*>(scene()->parent())->getWindowScale();
    const auto bottom = m_boundingRect.bottom();
    const auto coeff = m_boundingRect.height() / (m_maxDuration - m_minDuration);
    const auto heightRevert = 1.0 / m_boundingRect.height();

    QRectF rect;
    QBrush brush(Qt::SolidPattern);
    QRgb previousColor = 0;

    //brush.setColor(QColor::fromRgba(0x80808080));

    _painter->save();
    _painter->setPen(Qt::NoPen);
    //_painter->setBrush(brush);
    _painter->setTransform(QTransform::fromScale(1.0 / currentScale, 1), true);

    auto& items = *m_pSource;
    for (const auto& item : items)
    {
        // Draw rectangle

        const auto h = ::std::max((item.width() - m_minDuration) * coeff, 5.0);
        const auto col = ::std::min(h * heightRevert, 0.9999999);
        //const auto color = ::profiler_gui::toRgb(col * 255, (1.0 - col) * 255, 0); // item.color;
        const auto color = 0x00ffffff & QColor::fromHsvF((1.0 - col) * 0.35, 0.85, 0.85).rgb();

        if (previousColor != color)
        {
            // Set background color brush for rectangle
            previousColor = color;
            brush.setColor(QColor::fromRgba(0xc0000000 | color));
            _painter->setBrush(brush);
        }

        rect.setRect(item.left() * currentScale, bottom - h, ::std::max(item.width() * currentScale, 1.0), h);
        _painter->drawRect(rect);
    }

    _painter->setPen(Qt::darkGray);
    _painter->drawLine(0, bottom, m_boundingRect.width(), bottom);
    _painter->drawLine(0, m_boundingRect.top(), m_boundingRect.width(), m_boundingRect.top());

    _painter->restore();
}

::profiler::thread_id_t EasyMinimapItem::threadId() const
{
    return m_threadId;
}

void EasyMinimapItem::setBoundingRect(const QRectF& _rect)
{
    m_boundingRect = _rect;
}

void EasyMinimapItem::setBoundingRect(qreal x, qreal y, qreal w, qreal h)
{
    m_boundingRect.setRect(x, y, w, h);
}

void EasyMinimapItem::setSource(::profiler::thread_id_t _thread_id, const ::profiler_gui::ProfItems* _items)
{
    m_pSource = _items;
    m_threadId = _thread_id;

    if (m_pSource)
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
                auto w = item.width();

                if (w > m_maxDuration)
                {
                    m_maxDuration = item.width();
                }

                if (w < m_minDuration)
                {
                    m_minDuration = w;
                }
            }
        }
    }

    if (m_pSource == nullptr)
    {
        hide();
    }
    else
    {
        show();
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
    , m_minimap(nullptr)
    , m_bScrolling(false)
{
    setCacheMode(QGraphicsView::CacheNone);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    //setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setContentsMargins(0, 0, 0, 0);
    setFixedHeight(DEFAULT_HEIGHT + 2);

    auto selfScene = new QGraphicsScene(this);
    selfScene->setSceneRect(0, DEFAULT_TOP, 500, DEFAULT_HEIGHT);
    setScene(selfScene);

    m_minimap = new EasyMinimapItem();
    m_minimap->setPos(0, 0);
    m_minimap->setBoundingRect(0, DEFAULT_TOP + INDICATOR_SIZE, scene()->width(), DEFAULT_HEIGHT - (INDICATOR_SIZE << 1));
    selfScene->addItem(m_minimap);
    m_minimap->hide();

    m_chronometerIndicator = new EasyGraphicsSliderItem(false);
    m_chronometerIndicator->setPos(0, 0);
    m_chronometerIndicator->setColor(0x40000000 | ::profiler_gui::CHRONOMETER_COLOR.rgba());
    selfScene->addItem(m_chronometerIndicator);
    m_chronometerIndicator->hide();

    m_slider = new EasyGraphicsSliderItem(true);
    m_slider->setPos(0, 0);
    m_slider->setColor(0x40c0c0c0);
    selfScene->addItem(m_slider);

    centerOn(0, 0);
}

EasyGraphicsScrollbar::~EasyGraphicsScrollbar()
{

}

//////////////////////////////////////////////////////////////////////////

qreal EasyGraphicsScrollbar::getWindowScale() const
{
    return m_windowScale;
}

::profiler::thread_id_t EasyGraphicsScrollbar::minimapThread() const
{
    return m_minimap->threadId();
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
    scene()->setSceneRect(_minValue, DEFAULT_TOP, _maxValue - _minValue, DEFAULT_HEIGHT);
    m_minimap->setBoundingRect(_minValue, DEFAULT_TOP + INDICATOR_SIZE, _maxValue, DEFAULT_HEIGHT - (INDICATOR_SIZE << 1));
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

void EasyGraphicsScrollbar::setMinimapFrom(::profiler::thread_id_t _thread_id, const ::profiler_gui::ProfItems* _items)
{
    m_minimap->setSource(_thread_id, _items);
    scene()->update();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::mousePressEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();

    if (m_mouseButtons & Qt::LeftButton)
    {
        m_bScrolling = true;
        m_mousePressPos = _event->pos();
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
            setValue(m_value + delta.x() / m_windowScale);
        }
    }
}

void EasyGraphicsScrollbar::wheelEvent(QWheelEvent* _event)
{
    qreal deltaSign = _event->delta() < 0 ? -1 : 1;
    auto w = m_slider->halfwidth() * (_event->delta() < 0 ? ::profiler_gui::SCALING_COEFFICIENT : ::profiler_gui::SCALING_COEFFICIENT_INV);
    setValue(mapToScene(_event->pos()).x() - m_minimumValue - w);
    emit wheeled(w * m_windowScale, _event->delta());
    _event->accept();
}

void EasyGraphicsScrollbar::resizeEvent(QResizeEvent* _event)
{
    onWindowWidthChange(_event->size().width());
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::contextMenuEvent(QContextMenuEvent* _event)
{
    if (::profiler_gui::EASY_GLOBALS.profiler_blocks.empty())
    {
        return;
    }

    QMenu menu;

    for (const auto& it : ::profiler_gui::EASY_GLOBALS.profiler_blocks)
    {
        QString label;
        if (it.second.thread_name && it.second.thread_name[0] != 0)
        {
            label = ::std::move(QString("%1 Thread %2").arg(it.second.thread_name).arg(it.first));
        }
        else
        {
            label = ::std::move(QString("Thread %1").arg(it.first));
        }

        auto action = new EasyIdAction(label, it.first);
        action->setCheckable(true);
        action->setChecked(it.first == ::profiler_gui::EASY_GLOBALS.selected_thread);
        connect(action, &EasyIdAction::clicked, this, &This::onThreadActionClicked);

        menu.addAction(action);
    }

    menu.exec(QCursor::pos());
    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void EasyGraphicsScrollbar::onThreadActionClicked(::profiler::thread_id_t _id)
{
    if (_id != m_minimap->threadId())
    {
        ::profiler_gui::EASY_GLOBALS.selected_thread = _id;
        emit ::profiler_gui::EASY_GLOBALS.events.selectedThreadChanged(_id);
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
}

//////////////////////////////////////////////////////////////////////////
