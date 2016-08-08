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
#include <QAction>
#include "common_types.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////

class ProfGraphicsSliderItem : public QGraphicsRectItem
{
    typedef QGraphicsRectItem      Parent;
    typedef ProfGraphicsSliderItem   This;

private:

    qreal m_halfwidth;

public:

    ProfGraphicsSliderItem();
    virtual ~ProfGraphicsSliderItem();

    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

    qreal width() const;
    qreal halfwidth() const;

    void setWidth(qreal _width);
    void setHalfwidth(qreal _halfwidth);

    void setColor(QRgb _color);
    void setColor(const QColor& _color);

}; // END of class ProfGraphicsSliderItem.

//////////////////////////////////////////////////////////////////////////

class ProfMinimapItem : public QGraphicsItem
{
    typedef QGraphicsItem Parent;
    typedef ProfMinimapItem This;

    QRectF                           m_boundingRect;
    qreal                             m_maxDuration;
    qreal                             m_minDuration;
    const ::profiler_gui::ProfItems*      m_pSource;
    ::profiler::thread_id_t              m_threadId;

public:

    ProfMinimapItem();
    virtual ~ProfMinimapItem();

    // Public virtual methods

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    // Public non-virtual methods

    ::profiler::thread_id_t threadId() const;

    void setBoundingRect(const QRectF& _rect);

    void setSource(::profiler::thread_id_t _thread_id, const ::profiler_gui::ProfItems* _items);

}; // END of class ProfMinimapItem.

//////////////////////////////////////////////////////////////////////////

class ProfIdAction : public QAction
{
    Q_OBJECT

private:

    typedef QAction      Parent;
    typedef ProfIdAction   This;

    ::profiler::thread_id_t m_id;

public:

    ProfIdAction(const QString& _label, ::profiler::thread_id_t _id) : Parent(_label, nullptr), m_id(_id)
    {
        connect(this, &Parent::triggered, this, &This::onToggle);
    }

    ProfIdAction(const char* _label, ::profiler::thread_id_t _id) : Parent(_label, nullptr), m_id(_id)
    {
        connect(this, &Parent::triggered, this, &This::onToggle);
    }

    virtual ~ProfIdAction()
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

class ProfGraphicsScrollbar : public QGraphicsView
{
    Q_OBJECT

private:

    typedef QGraphicsView         Parent;
    typedef ProfGraphicsScrollbar   This;

    qreal                           m_minimumValue;
    qreal                           m_maximumValue;
    qreal                                  m_value;
    qreal                            m_windowScale;
    QPoint                         m_mousePressPos;
    Qt::MouseButtons                m_mouseButtons;
    ProfGraphicsSliderItem*               m_slider;
    ProfGraphicsSliderItem* m_chronometerIndicator;
    ProfMinimapItem*                     m_minimap;
    bool                              m_bScrolling;

public:

    ProfGraphicsScrollbar(QWidget* _parent = nullptr);
    virtual ~ProfGraphicsScrollbar();

    // Public virtual methods

    void mousePressEvent(QMouseEvent* _event) override;
    void mouseReleaseEvent(QMouseEvent* _event) override;
    void mouseMoveEvent(QMouseEvent* _event) override;
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

    void setMinimapFrom(::profiler::thread_id_t _thread_id, const::profiler_gui::ProfItems* _items);

    inline void setMinimapFrom(::profiler::thread_id_t _thread_id, const ::profiler_gui::ProfItems& _items)
    {
        setMinimapFrom(_thread_id, &_items);
    }

signals:

    void rangeChanged();
    void valueChanged(qreal _value);

private slots:

    void onThreadActionClicked(::profiler::thread_id_t _id);
    void onWindowWidthChange(qreal _width);

}; // END of class ProfGraphicsScrollbar.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER__GRAPHICS_SCROLLBAR__H
