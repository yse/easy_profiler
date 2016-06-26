/************************************************************************
* file name         : blocks_graphics_view.h
* ----------------- :
* creation time     : 2016/06/26
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of GraphicsScene and GraphicsView and
*                   : it's auxiliary classes for displyaing easy_profiler blocks tree.
* ----------------- :
* change log        : * 2016/06/26 Victor Zarubkin: moved sources from graphics_view.h
*                   : *
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#ifndef MY____GRAPHICS___VIEW_H
#define MY____GRAPHICS___VIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPolygonItem>
#include <QGraphicsSimpleTextItem>
#include <stdlib.h>
#include "profiler/reader.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class GlobalSignals : public QObject
{
    Q_OBJECT

public:

    GlobalSignals() : QObject() {}
    virtual ~GlobalSignals() {}

signals:

    void scaleIncreased(qreal _scale) const;
    void scaleDecreased(qreal _scale) const;

}; // END of class GlobalSignals.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class MyPolygon : public QGraphicsPolygonItem
{
public:

    MyPolygon(QGraphicsItem* _parent = nullptr);
    virtual ~MyPolygon();

}; // END of class MyPolygon.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class MyText : public QObject, public QGraphicsSimpleTextItem
{
    Q_OBJECT

public:

    MyText(const char* _text, QGraphicsItem* _parent = nullptr);
    virtual ~MyText();

private slots:

    void onScaleIncrease(qreal _scale);

    void onScaleDecrease(qreal _scale);

}; // END of class MyText.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class MyGraphicsScene : public QGraphicsScene
{
    Q_OBJECT

private:

    profiler::timestamp_t m_start;

public:

    int items_couinter = 0;

    MyGraphicsScene(QGraphicsView* _parent);
    MyGraphicsScene(const thread_blocks_tree_t& _blocksTree, QGraphicsView* _parent);
    virtual ~MyGraphicsScene();

    void test();

    void setTree(const thread_blocks_tree_t& _blocksTree);

private:

    inline qreal time2position(const profiler::timestamp_t& _time) const
    {
        return qreal(_time - m_start) * 1e-6;
    }

    void setTree(const BlocksTree::children_t& _children, qreal _y, int _level = 0);

}; // END of class MyGraphicsScene.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class MyGraphicsView : public QGraphicsView
{
    Q_OBJECT

private:

    qreal m_scale;
    qreal m_scaleCoeff;

public:

    MyGraphicsView();
    MyGraphicsView(const thread_blocks_tree_t& _blocksTree);
    virtual ~MyGraphicsView();

    void initMode();

    void wheelEvent(QWheelEvent* _event);

}; // END of class MyGraphicsView.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // MY____GRAPHICS___VIEW_H
