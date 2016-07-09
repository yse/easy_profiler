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
#include <QGraphicsPolygonItem>

//////////////////////////////////////////////////////////////////////////

class GraphicsHorizontalSlider : public QGraphicsPolygonItem
{
    typedef QGraphicsPolygonItem     Parent;
    typedef GraphicsHorizontalSlider   This;

private:

    double m_halfwidth;

public:

    GraphicsHorizontalSlider();
    virtual ~GraphicsHorizontalSlider();

    double width() const;
    double halfwidth() const;

    void setWidth(double _width);
    void setHalfwidth(double _halfwidth);

    void setColor(QRgb _color);

}; // END of class GraphicsHorizontalSlider.

//////////////////////////////////////////////////////////////////////////

class GraphicsHorizontalScrollbar : public QGraphicsView
{
    Q_OBJECT

private:

    typedef QGraphicsView     Parent;
    typedef GraphicsHorizontalScrollbar   This;

    double m_minimumValue, m_maximumValue, m_value;
    GraphicsHorizontalSlider* m_slider;

public:

    GraphicsHorizontalScrollbar(QWidget* _parent = nullptr);
    virtual ~GraphicsHorizontalScrollbar();

    double minimum() const;
    double maximum() const;
    double value() const;

    void setValue(double _value);
    void setRange(double _minValue, double _maxValue);

signals:

    void valueChanged(double _value) const;

}; // END of class GraphicsHorizontalScrollbar.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__GRAPHICS_SCROLLBAR__H
