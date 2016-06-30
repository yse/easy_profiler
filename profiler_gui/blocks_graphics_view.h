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
*                   : * 2016/06/30 Victor Zarubkin: Replaced doubles with floats (in ProfBlockItem) for less memory consumption.
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

#define PROF_BLOCK_ITEM_OPT_MEMORY

#ifdef PROF_BLOCK_ITEM_OPT_MEMORY
typedef float preal;
#else
typedef qreal preal;
#endif

#pragma pack(push, 1)
struct ProfBlockItem
{
#ifdef PROF_BLOCK_ITEM_OPT_MEMORY
    const BlocksTree* block;
    preal        x, y, w, h;
#else
    QRectF             rect;
    const BlocksTree* block;
#endif

    float       totalHeight;
    QRgb              color;
    bool               draw;

    void setRect(preal _x, preal _y, preal _w, preal _h);
    preal left() const;
    preal top() const;
    preal width() const;
    preal height() const;
    preal right() const;
    preal bottom() const;
};
#pragma pack(pop)

class ProfGraphicsItem : public QGraphicsItem
{
    typedef ::std::vector<ProfBlockItem> Children;
    //typedef ::std::vector<Children> Sublevels;

    Children   m_items;
    //Sublevels m_levels;
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


//     void setLevels(unsigned int _levels);
//     void reserve(unsigned short _level, unsigned int _items);
// 
//     const Children& items(unsigned short _level) const;
//     const ProfBlockItem& getItem(unsigned short _level, size_t _index) const;
//     ProfBlockItem& getItem(unsigned short _level, size_t _index);
// 
//     size_t addItem(unsigned short _level);
//     size_t addItem(unsigned short _level, const ProfBlockItem& _item);
//     size_t addItem(unsigned short _level, ProfBlockItem&& _item);

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
    void test2(size_t _frames_number, size_t _total_items_number_estimate, int _depth);

    void clearSilent();

    void setTree(const thread_blocks_tree_t& _blocksTree);

    qreal setTree(const BlocksTree::children_t& _children, qreal& _height, qreal _y);
    qreal setTree(ProfGraphicsItem* _item, const BlocksTree::children_t& _children, qreal& _height, qreal _y);

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