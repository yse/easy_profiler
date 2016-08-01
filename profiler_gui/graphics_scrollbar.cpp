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
#include <QWheelEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include "graphics_scrollbar.h"

//////////////////////////////////////////////////////////////////////////

const qreal SCALING_COEFFICIENT = 1.25;
const qreal SCALING_COEFFICIENT_INV = 1.0 / SCALING_COEFFICIENT;

//////////////////////////////////////////////////////////////////////////

auto const clamp = [](qreal _minValue, qreal _value, qreal _maxValue)
{
    return (_value < _minValue ? _minValue : (_value > _maxValue ? _maxValue : _value));
};

//////////////////////////////////////////////////////////////////////////

GraphicsHorizontalSlider::GraphicsHorizontalSlider() : Parent(), m_halfwidth(0)
{
    setWidth(40);
    setBrush(Qt::SolidPattern);
}

GraphicsHorizontalSlider::~GraphicsHorizontalSlider()
{

}

void GraphicsHorizontalSlider::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    //Parent::paint(_painter, _option, _widget);
    const auto currentScale = static_cast<const GraphicsHorizontalScrollbar*>(scene()->parent())->getWindowScale();
    const auto br = rect();

    qreal w = width() * currentScale;
    qreal dx = 0;

    if (w < 1.0)
    {
        dx = (w - 1.0) * 0.5;
        w = 1.0;
    }

    _painter->save();
    _painter->setTransform(QTransform::fromScale(1.0 / currentScale, 1), true);
    _painter->setBrush(brush());
    _painter->setPen(Qt::NoPen);
    _painter->drawRect(QRectF(dx + br.left() * currentScale, br.top(), w, br.height()));
    _painter->restore();
}

qreal GraphicsHorizontalSlider::width() const
{
    return m_halfwidth * 2.0;
}

qreal GraphicsHorizontalSlider::halfwidth() const
{
    return m_halfwidth;
}

void GraphicsHorizontalSlider::setWidth(qreal _width)
{
    m_halfwidth = _width * 0.5;
    setRect(-m_halfwidth, -10, _width, 20);
}

void GraphicsHorizontalSlider::setHalfwidth(qreal _halfwidth)
{
    m_halfwidth = _halfwidth;
    setRect(-m_halfwidth, -10, m_halfwidth * 2.0, 20);
}

void GraphicsHorizontalSlider::setColor(QRgb _color)
{
    QColor c(_color);
    c.setAlpha((_color & 0xff000000) >> 24);

    auto b = brush();
    b.setColor(c);
    setBrush(b);
}

//////////////////////////////////////////////////////////////////////////

GraphicsHorizontalScrollbar::GraphicsHorizontalScrollbar(QWidget* _parent)
    : Parent(_parent)
    , m_minimumValue(0)
    , m_maximumValue(500)
    , m_value(10)
    , m_windowScale(1)
    , m_mouseButtons(Qt::NoButton)
    , m_slider(nullptr)
    , m_bScrolling(false)
{
    setCacheMode(QGraphicsView::CacheNone);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setMinimumHeight(20);
    setMaximumHeight(20);

    auto selfScene = new QGraphicsScene(this);
    selfScene->setSceneRect(0, -10, 500, 20);
    setScene(selfScene);

    m_slider = new GraphicsHorizontalSlider();
    m_slider->setPos(10, 0);
    m_slider->setColor(0x80e00000);
    selfScene->addItem(m_slider);

    centerOn(0, 0);
}

GraphicsHorizontalScrollbar::~GraphicsHorizontalScrollbar()
{

}

//////////////////////////////////////////////////////////////////////////

qreal GraphicsHorizontalScrollbar::getWindowScale() const
{
    return m_windowScale;
}

qreal GraphicsHorizontalScrollbar::minimum() const
{
    return m_minimumValue;
}

qreal GraphicsHorizontalScrollbar::maximum() const
{
    return m_maximumValue;
}

qreal GraphicsHorizontalScrollbar::range() const
{
    return m_maximumValue - m_minimumValue;
}

qreal GraphicsHorizontalScrollbar::value() const
{
    return m_value;
}

//////////////////////////////////////////////////////////////////////////

void GraphicsHorizontalScrollbar::setValue(qreal _value)
{
    m_value = clamp(m_minimumValue, _value, ::std::max(m_minimumValue, m_maximumValue - m_slider->width()));
    m_slider->setX(m_value + m_slider->halfwidth());
    emit valueChanged(m_value);
}

void GraphicsHorizontalScrollbar::setRange(qreal _minValue, qreal _maxValue)
{
    const auto oldRange = range();
    const auto oldValue = oldRange < 1e-3 ? 0.0 : m_value / oldRange;

    m_minimumValue = _minValue;
    m_maximumValue = _maxValue;
    scene()->setSceneRect(_minValue, -10, _maxValue - _minValue, 20);
    emit rangeChanged();

    setValue(_minValue + oldValue * range());
    onWindowWidthChange(width());
}

void GraphicsHorizontalScrollbar::setSliderWidth(qreal _width)
{
    m_slider->setWidth(_width);
    setValue(m_value);
}

//////////////////////////////////////////////////////////////////////////

void GraphicsHorizontalScrollbar::mousePressEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();

    if (m_mouseButtons & Qt::LeftButton)
    {
        m_bScrolling = true;
        m_mousePressPos = _event->pos();
        setValue(mapToScene(m_mousePressPos).x() - m_minimumValue);
    }

    _event->accept();
    //QGraphicsView::mousePressEvent(_event);
}

void GraphicsHorizontalScrollbar::mouseReleaseEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();
    m_bScrolling = false;
    _event->accept();
    //QGraphicsView::mouseReleaseEvent(_event);
}

void GraphicsHorizontalScrollbar::mouseMoveEvent(QMouseEvent* _event)
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

void GraphicsHorizontalScrollbar::resizeEvent(QResizeEvent* _event)
{
    onWindowWidthChange(_event->size().width());
}

void GraphicsHorizontalScrollbar::onWindowWidthChange(qreal _width)
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
