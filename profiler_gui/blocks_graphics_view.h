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
*                   :
*                   : * 2016/06/29 Victor Zarubkin: Highly optimized painting performance and memory consumption.
*                   :
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
#include <vector>
#include "profiler/reader.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)
struct ProfBlockItem
{
    QRectF             rect;
    qreal       totalHeight;
    const BlocksTree* block;
    QRgb              color;
    bool               draw;

    ProfBlockItem() : totalHeight(0), block(nullptr), color(0), draw(true)
    {
    }
};
#pragma pack(pop)

class ProfGraphicsItem : public QGraphicsItem
{
    typedef ::std::vector<ProfBlockItem> Children;
    Children   m_items;
    QRectF      m_rect;
    const bool m_bTest;

public:

    ProfGraphicsItem();
    ProfGraphicsItem(bool _test);
    virtual ~ProfGraphicsItem();

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);
    void setBoundingRect(const QRectF& _rect);

    void reserve(unsigned int _items);

    const Children& items() const;
    const ProfBlockItem& getItem(size_t _index) const;
    ProfBlockItem& getItem(size_t _index);

    size_t addItem();
    size_t addItem(const ProfBlockItem& _item);
    size_t addItem(ProfBlockItem&& _item);

}; // END of class ProfGraphicsItem.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsScene : public QGraphicsScene
{
    friend class ProfGraphicsView;

    Q_OBJECT

private:

    typedef ProfGraphicsScene This;

    ::profiler::timestamp_t m_beginTime;

public:

    ProfGraphicsScene(QGraphicsView* _parent, bool _test = false);
    ProfGraphicsScene(const thread_blocks_tree_t& _blocksTree, QGraphicsView* _parent);
    virtual ~ProfGraphicsScene();

private:

    void test(size_t _frames_number, size_t _total_items_number_estimate, int _depth);

    void clearSilent();

    void setTree(const thread_blocks_tree_t& _blocksTree);

    qreal setTreeInternal(const BlocksTree::children_t& _children, qreal& _height, qreal _y);
    qreal setTreeInternal(ProfGraphicsItem* _item, const BlocksTree::children_t& _children, qreal& _height, qreal _y);

    inline qreal time2position(const profiler::timestamp_t& _time) const
    {
        return qreal(_time - m_beginTime) * 1e-6;
    }

}; // END of class ProfGraphicsScene.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class ProfGraphicsView : public QGraphicsView
{
    Q_OBJECT

private:

    typedef ProfGraphicsView This;

    QRectF        m_visibleSceneRect;
    qreal                    m_scale;
    qreal               m_scaleCoeff;
    QPoint           m_mousePressPos;
    Qt::MouseButtons  m_mouseButtons;
    bool             m_bUpdatingRect;

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

    inline const QRectF& visibleSceneRect() const
    {
        return m_visibleSceneRect;
    }

    void setTree(const thread_blocks_tree_t& _blocksTree);
    void clearSilent();

    void test(size_t _frames_number, size_t _total_items_number_estimate, int _depth);

private:

    void initMode();
    void updateVisibleSceneRect();

private slots:

    void onScrollbarValueChange(int);

}; // END of class ProfGraphicsView.

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // MY____GRAPHICS___VIEW_H
