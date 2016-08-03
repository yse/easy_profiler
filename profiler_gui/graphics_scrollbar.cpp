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
#include <QContextMenuEvent>
#include <QMenu>
#include "graphics_scrollbar.h"

//////////////////////////////////////////////////////////////////////////

const qreal SCALING_COEFFICIENT = 1.25;
const qreal SCALING_COEFFICIENT_INV = 1.0 / SCALING_COEFFICIENT;
const int DEFAULT_TOP = -40;
const int DEFAULT_HEIGHT = 80;

//////////////////////////////////////////////////////////////////////////

auto const clamp = [](qreal _minValue, qreal _value, qreal _maxValue)
{
    return (_value < _minValue ? _minValue : (_value > _maxValue ? _maxValue : _value));
};

//////////////////////////////////////////////////////////////////////////

ProfGraphicsSliderItem::ProfGraphicsSliderItem() : Parent(), m_halfwidth(0)
{
    setWidth(1);
    setBrush(Qt::SolidPattern);
}

ProfGraphicsSliderItem::~ProfGraphicsSliderItem()
{

}

void ProfGraphicsSliderItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    //Parent::paint(_painter, _option, _widget);
    const auto currentScale = static_cast<const ProfGraphicsScrollbar*>(scene()->parent())->getWindowScale();
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

qreal ProfGraphicsSliderItem::width() const
{
    return m_halfwidth * 2.0;
}

qreal ProfGraphicsSliderItem::halfwidth() const
{
    return m_halfwidth;
}

void ProfGraphicsSliderItem::setWidth(qreal _width)
{
    m_halfwidth = _width * 0.5;
    setRect(-m_halfwidth, DEFAULT_TOP, _width, DEFAULT_HEIGHT);
}

void ProfGraphicsSliderItem::setHalfwidth(qreal _halfwidth)
{
    m_halfwidth = _halfwidth;
    setRect(-m_halfwidth, DEFAULT_TOP, m_halfwidth * 2.0, DEFAULT_HEIGHT);
}

void ProfGraphicsSliderItem::setColor(QRgb _color)
{
    auto b = brush();
    b.setColor(QColor::fromRgba(_color));
    setBrush(b);
}

//////////////////////////////////////////////////////////////////////////

ProfMinimapItem::ProfMinimapItem() : Parent(), m_pSource(nullptr), m_maxDuration(0), m_threadId(0)
{

}

ProfMinimapItem::~ProfMinimapItem()
{

}

QRectF ProfMinimapItem::boundingRect() const
{
    return m_boundingRect;
}

void ProfMinimapItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    if (m_pSource == nullptr)
    {
        return;
    }

    const auto currentScale = static_cast<const ProfGraphicsScrollbar*>(scene()->parent())->getWindowScale();
    const auto bottom = m_boundingRect.bottom();
    const auto coeff = (m_boundingRect.height() - 5) / m_maxDuration;

    QRectF rect;
    QBrush brush(Qt::SolidPattern);
    QRgb previousColor = 0;

    brush.setColor(QColor::fromRgba(0x80808080));

    _painter->save();
    _painter->setPen(Qt::NoPen);
    _painter->setBrush(brush);
    _painter->setTransform(QTransform::fromScale(1.0 / currentScale, 1), true);

    auto& items = *m_pSource;
    for (const auto& item : items)
    {
        //if (previousColor != item.color)
        //{
        //    // Set background color brush for rectangle
        //    previousColor = item.color;
        //    brush.setColor(QColor::fromRgba(0x40000000 | item.color));
        //    _painter->setBrush(brush);
        //}

        // Draw rectangle
        auto h = 5 + item.width() * coeff;
        rect.setRect(item.left() * currentScale, bottom - h, ::std::max(item.width() * currentScale, 1.0), h);
        _painter->drawRect(rect);
    }

    _painter->restore();
}

::profiler::thread_id_t ProfMinimapItem::threadId() const
{
    return m_threadId;
}

void ProfMinimapItem::setBoundingRect(const QRectF& _rect)
{
    m_boundingRect = _rect;
}

void ProfMinimapItem::setSource(::profiler::thread_id_t _thread_id, const ::profiler_gui::ProfItems* _items)
{
    m_pSource = _items;
    m_threadId = _thread_id;

    if (m_pSource)
    {
        if (m_pSource->empty())
        {
            m_pSource = nullptr;
        }

        m_maxDuration = 0;
        for (const auto& item : *m_pSource)
        {
            if (item.width() > m_maxDuration)
            {
                m_maxDuration = item.width();
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

ProfGraphicsScrollbar::ProfGraphicsScrollbar(QWidget* _parent)
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
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setContentsMargins(0, 0, 0, 0);
    setMinimumHeight(DEFAULT_HEIGHT + 2);
    setMaximumHeight(DEFAULT_HEIGHT + 2);

    auto selfScene = new QGraphicsScene(this);
    selfScene->setSceneRect(0, DEFAULT_TOP, 500, DEFAULT_HEIGHT);
    setScene(selfScene);

    m_slider = new ProfGraphicsSliderItem();
    m_slider->setPos(0, 0);
    m_slider->setZValue(5);
    m_slider->setColor(0x80e00000);
    selfScene->addItem(m_slider);

    m_chronometerIndicator = new ProfGraphicsSliderItem();
    m_chronometerIndicator->setPos(0, 0);
    m_chronometerIndicator->setZValue(10);
    m_chronometerIndicator->setColor(0x80404040);
    selfScene->addItem(m_chronometerIndicator);
    m_chronometerIndicator->hide();

    m_minimap = new ProfMinimapItem();
    m_minimap->setPos(0, 0);
    m_minimap->setBoundingRect(selfScene->sceneRect());
    selfScene->addItem(m_minimap);
    m_minimap->hide();

    centerOn(0, 0);
}

ProfGraphicsScrollbar::~ProfGraphicsScrollbar()
{

}

//////////////////////////////////////////////////////////////////////////

qreal ProfGraphicsScrollbar::getWindowScale() const
{
    return m_windowScale;
}

::profiler::thread_id_t ProfGraphicsScrollbar::minimapThread() const
{
    return m_minimap->threadId();
}

qreal ProfGraphicsScrollbar::minimum() const
{
    return m_minimumValue;
}

qreal ProfGraphicsScrollbar::maximum() const
{
    return m_maximumValue;
}

qreal ProfGraphicsScrollbar::range() const
{
    return m_maximumValue - m_minimumValue;
}

qreal ProfGraphicsScrollbar::value() const
{
    return m_value;
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsScrollbar::setValue(qreal _value)
{
    m_value = clamp(m_minimumValue, _value, ::std::max(m_minimumValue, m_maximumValue - m_slider->width()));
    m_slider->setX(m_value + m_slider->halfwidth());
    emit valueChanged(m_value);
}

void ProfGraphicsScrollbar::setRange(qreal _minValue, qreal _maxValue)
{
    const auto oldRange = range();
    const auto oldValue = oldRange < 1e-3 ? 0.0 : m_value / oldRange;

    m_minimumValue = _minValue;
    m_maximumValue = _maxValue;
    scene()->setSceneRect(_minValue, DEFAULT_TOP, _maxValue - _minValue, DEFAULT_HEIGHT);
    m_minimap->setBoundingRect(scene()->sceneRect());
    emit rangeChanged();

    setValue(_minValue + oldValue * range());
    onWindowWidthChange(width());
}

void ProfGraphicsScrollbar::setSliderWidth(qreal _width)
{
    m_slider->setWidth(_width);
    setValue(m_value);
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsScrollbar::setChronoPos(qreal _left, qreal _right)
{
    m_chronometerIndicator->setWidth(_right - _left);
    m_chronometerIndicator->setX(_left + m_chronometerIndicator->halfwidth());
}

void ProfGraphicsScrollbar::showChrono()
{
    m_chronometerIndicator->show();
}

void ProfGraphicsScrollbar::hideChrono()
{
    m_chronometerIndicator->hide();
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsScrollbar::setMinimapFrom(::profiler::thread_id_t _thread_id, const ::profiler_gui::ProfItems* _items)
{
    m_minimap->setSource(_thread_id, _items);
    scene()->update();
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsScrollbar::mousePressEvent(QMouseEvent* _event)
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

void ProfGraphicsScrollbar::mouseReleaseEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();
    m_bScrolling = false;
    _event->accept();
    //QGraphicsView::mouseReleaseEvent(_event);
}

void ProfGraphicsScrollbar::mouseMoveEvent(QMouseEvent* _event)
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

void ProfGraphicsScrollbar::resizeEvent(QResizeEvent* _event)
{
    onWindowWidthChange(_event->size().width());
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsScrollbar::contextMenuEvent(QContextMenuEvent* _event)
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

        auto action = new ProfIdAction(label, it.first);
        action->setCheckable(true);
        action->setChecked(it.first == ::profiler_gui::EASY_GLOBALS.selected_thread);
        connect(action, &ProfIdAction::clicked, this, &This::onThreadActionClicked);

        menu.addAction(action);
    }

    menu.exec(QCursor::pos());
    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsScrollbar::onThreadActionClicked(::profiler::thread_id_t _id)
{
    if (_id != m_minimap->threadId())
    {
        ::profiler_gui::EASY_GLOBALS.selected_thread = _id;
        emit ::profiler_gui::EASY_GLOBALS.events.selectedThreadChanged(_id);
    }
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsScrollbar::onWindowWidthChange(qreal _width)
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
