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
#include <QPoint>
#include <stdlib.h>
#include "profiler/reader.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsPolygonItem : public QGraphicsPolygonItem
{
    bool m_bDrawBorders;

public:

    ProfGraphicsPolygonItem(bool _drawBorders, QGraphicsItem* _parent = nullptr);
    virtual ~ProfGraphicsPolygonItem();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = Q_NULLPTR) override;

}; // END of class ProfGraphicsPolygonItem.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsTextItem : public QGraphicsSimpleTextItem
{
public:

    ProfGraphicsTextItem(const char* _text, QGraphicsItem* _parent = nullptr);
    virtual ~ProfGraphicsTextItem();

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = Q_NULLPTR) override;

}; // END of class ProfGraphicsTextItem.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsScene : public QGraphicsScene
{
    friend class ProfGraphicsView;

    Q_OBJECT

private:

    profiler::timestamp_t m_start;

public:

    ProfGraphicsScene(QGraphicsView* _parent, bool _test = false);
    ProfGraphicsScene(const thread_blocks_tree_t& _blocksTree, QGraphicsView* _parent);
    virtual ~ProfGraphicsScene();

private:

    void test();

    void clearSilent();

    void setTree(const thread_blocks_tree_t& _blocksTree);

    void setTreeInternal(const thread_blocks_tree_t& _blocksTree);
    void setTreeInternal(const BlocksTree::children_t& _children, qreal _y, int _level = 0);

    inline qreal time2position(const profiler::timestamp_t& _time) const
    {
        return qreal(_time - m_start) * 1e-6;
    }

}; // END of class ProfGraphicsScene.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsView : public QGraphicsView
{
    Q_OBJECT

private:

    qreal                    m_scale;
    qreal               m_scaleCoeff;
    QPoint           m_mousePressPos;
    Qt::MouseButtons  m_mouseButtons;

public:

    ProfGraphicsView(bool _test = false);
    ProfGraphicsView(const thread_blocks_tree_t& _blocksTree);
    virtual ~ProfGraphicsView();

    void wheelEvent(QWheelEvent* _event) override;
    void mousePressEvent(QMouseEvent* _event) override;
    void mouseReleaseEvent(QMouseEvent* _event) override;
    void mouseMoveEvent(QMouseEvent* _event) override;

    inline qreal currentScale() const
    {
        return m_scale;
    }

    void setTree(const thread_blocks_tree_t& _blocksTree);

    void initMode();

}; // END of class ProfGraphicsView.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // MY____GRAPHICS___VIEW_H
