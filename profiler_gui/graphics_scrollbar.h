/************************************************************************
* file name         : graphics_scrollbar.h
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

class EasyIdAction : public QAction
{
    Q_OBJECT

private:

    typedef QAction      Parent;
    typedef EasyIdAction   This;

    ::profiler::thread_id_t m_id;

public:

    EasyIdAction(const QString& _label, ::profiler::thread_id_t _id) : Parent(_label, nullptr), m_id(_id)
    {
        connect(this, &Parent::triggered, this, &This::onToggle);
    }

    EasyIdAction(const char* _label, ::profiler::thread_id_t _id) : Parent(_label, nullptr), m_id(_id)
    {
        connect(this, &Parent::triggered, this, &This::onToggle);
    }

    virtual ~EasyIdAction()
    {
    }

private:

    void onToggle(bool)
    {
        emit clicked(m_id);
    }

signals:

    void clicked(::profiler::thread_id_t _id);
};

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

    void onThreadActionClicked(::profiler::thread_id_t _id);
    void onWindowWidthChange(qreal _width);

}; // END of class EasyGraphicsScrollbar.

//////////////////////////////////////////////////////////////////////////

#endif // EASY__GRAPHICS_SCROLLBAR__H
