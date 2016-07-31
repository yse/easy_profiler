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

#ifndef EASY_PROFILER__GRAPHICS_SCROLLBAR__H
#define EASY_PROFILER__GRAPHICS_SCROLLBAR__H

#include <stdlib.h>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsRectItem>

//////////////////////////////////////////////////////////////////////////

class GraphicsHorizontalSlider : public QGraphicsRectItem
{
    typedef QGraphicsRectItem        Parent;
    typedef GraphicsHorizontalSlider   This;

private:

    qreal m_halfwidth;

public:

    GraphicsHorizontalSlider();
    virtual ~GraphicsHorizontalSlider();

    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

    qreal width() const;
    qreal halfwidth() const;

    void setWidth(qreal _width);
    void setHalfwidth(qreal _halfwidth);

    void setColor(QRgb _color);

}; // END of class GraphicsHorizontalSlider.

//////////////////////////////////////////////////////////////////////////

class GraphicsHorizontalScrollbar : public QGraphicsView
{
    Q_OBJECT

private:

    typedef QGraphicsView     Parent;
    typedef GraphicsHorizontalScrollbar   This;

    qreal                     m_minimumValue;
    qreal                     m_maximumValue;
    qreal                            m_value;
    qreal                      m_windowScale;
    QPoint                   m_mousePressPos;
    Qt::MouseButtons          m_mouseButtons;
    GraphicsHorizontalSlider*       m_slider;
    bool                        m_bScrolling;

public:

    GraphicsHorizontalScrollbar(QWidget* _parent = nullptr);
    virtual ~GraphicsHorizontalScrollbar();

    void mousePressEvent(QMouseEvent* _event) override;
    void mouseReleaseEvent(QMouseEvent* _event) override;
    void mouseMoveEvent(QMouseEvent* _event) override;
    void resizeEvent(QResizeEvent* _event) override;

    qreal getWindowScale() const;

    qreal minimum() const;
    qreal maximum() const;
    qreal range() const;
    qreal value() const;

    void setValue(qreal _value);
    void setRange(qreal _minValue, qreal _maxValue);
    void setSliderWidth(qreal _width);

signals:

    void rangeChanged();
    void valueChanged(qreal _value);

private:

    void onWindowWidthChange(qreal _width);

}; // END of class GraphicsHorizontalScrollbar.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__GRAPHICS_SCROLLBAR__H
