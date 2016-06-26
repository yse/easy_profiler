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
*                   :       and renamed classes from My* to Prof*.
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

class ProfViewGlobalSignals : public QObject
{
    Q_OBJECT

public:

    ProfViewGlobalSignals() : QObject() {}
    virtual ~ProfViewGlobalSignals() {}

signals:

    void scaleIncreased(qreal _scale) const;
    void scaleDecreased(qreal _scale) const;

}; // END of class ProfViewGlobalSignals.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsPolygonItem : public QGraphicsPolygonItem
{
public:

    ProfGraphicsPolygonItem(QGraphicsItem* _parent = nullptr);
    virtual ~ProfGraphicsPolygonItem();

}; // END of class ProfGraphicsPolygonItem.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsTextItem : public QObject, public QGraphicsSimpleTextItem
{
    Q_OBJECT

public:

    ProfGraphicsTextItem(const char* _text, QGraphicsItem* _parent = nullptr);
    virtual ~ProfGraphicsTextItem();

private slots:

    void onScaleIncrease(qreal _scale);

    void onScaleDecrease(qreal _scale);

}; // END of class ProfGraphicsTextItem.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsScene : public QGraphicsScene
{
    Q_OBJECT

private:

    profiler::timestamp_t m_start;

public:

    int items_couinter = 0;

    ProfGraphicsScene(QGraphicsView* _parent);
    ProfGraphicsScene(const thread_blocks_tree_t& _blocksTree, QGraphicsView* _parent);
    virtual ~ProfGraphicsScene();

    void test();

    void setTree(const thread_blocks_tree_t& _blocksTree);

private:

    inline qreal time2position(const profiler::timestamp_t& _time) const
    {
        return qreal(_time - m_start) * 1e-6;
    }

    void setTree(const BlocksTree::children_t& _children, qreal _y, int _level = 0);

}; // END of class ProfGraphicsScene.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsView : public QGraphicsView
{
    Q_OBJECT

private:

    qreal m_scale;
    qreal m_scaleCoeff;

public:

    ProfGraphicsView();
    ProfGraphicsView(const thread_blocks_tree_t& _blocksTree);
    virtual ~ProfGraphicsView();

    void initMode();

    void wheelEvent(QWheelEvent* _event);

}; // END of class ProfGraphicsView.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // MY____GRAPHICS___VIEW_H
