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

#include <QScrollBar>
#include "graphics_scrollbar.h"

//////////////////////////////////////////////////////////////////////////

auto const clamp = [](double _minValue, double _value, double _maxValue)
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

double GraphicsHorizontalSlider::width() const
{
    return m_halfwidth * 2.0;
}

double GraphicsHorizontalSlider::halfwidth() const
{
    return m_halfwidth;
}

void GraphicsHorizontalSlider::setWidth(double _width)
{
    m_halfwidth = _width * 0.5;
    setPolygon(QRectF(-m_halfwidth, -10, _width, 20));
}

void GraphicsHorizontalSlider::setHalfwidth(double _halfwidth)
{
    m_halfwidth = _halfwidth;
    setPolygon(QRectF(-m_halfwidth, -10, m_halfwidth * 2.0, 20));
}

void GraphicsHorizontalSlider::setColor(QRgb _color)
{
    auto b = brush();
    b.setColor(_color);
    setBrush(b);
}

//////////////////////////////////////////////////////////////////////////

GraphicsHorizontalScrollbar::GraphicsHorizontalScrollbar(QWidget* _parent)
    : Parent(_parent)
    , m_minimumValue(0)
    , m_maximumValue(500)
    , m_value(10)
    , m_slider(nullptr)
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
    m_slider->setColor(0xffff0404);
    selfScene->addItem(m_slider);

    centerOn(10, 0);
}

GraphicsHorizontalScrollbar::~GraphicsHorizontalScrollbar()
{

}

//////////////////////////////////////////////////////////////////////////

double GraphicsHorizontalScrollbar::minimum() const
{
    return m_minimumValue;
}

double GraphicsHorizontalScrollbar::maximum() const
{
    return m_maximumValue;
}

double GraphicsHorizontalScrollbar::value() const
{
    return m_value;
}

//////////////////////////////////////////////////////////////////////////

void GraphicsHorizontalScrollbar::setValue(double _value)
{
    m_value = clamp(m_minimumValue + m_slider->halfwidth(), _value, m_maximumValue - m_slider->halfwidth());
    emit valueChanged(m_value);
}

void GraphicsHorizontalScrollbar::setRange(double _minValue, double _maxValue)
{
    m_minimumValue = _minValue;
    m_maximumValue = _maxValue;
    setValue(m_value);
}

//////////////////////////////////////////////////////////////////////////
