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

#define DRAW_METHOD 2

#pragma pack(push, 1)
struct ProfBlockItem
{
    const BlocksTree*    block;
    qreal                    x;
    float                    w;
    float                    y;
    float                    h;
    QRgb                 color;
#if DRAW_METHOD > 0
    unsigned int children_begin;
#endif
    unsigned short totalHeight;
#if DRAW_METHOD > 0
    char                 state;
#else
    bool                 state;
#endif

    void setRect(qreal _x, float _y, float _w, float _h);
    qreal left() const;
    float top() const;
    float width() const;
    float height() const;
    qreal right() const;
    float bottom() const;
};
#pragma pack(pop)

class ProfGraphicsItem : public QGraphicsItem
{
    typedef ::std::vector<ProfBlockItem> Children;

#if DRAW_METHOD == 2
    typedef ::std::vector<unsigned int> DrawIndexes;
    DrawIndexes m_levelsIndexes;
    typedef ::std::vector<Children> Sublevels;
    Sublevels m_levels;
#else
    Children   m_items;
#endif

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

#if DRAW_METHOD == 0
    void reserve(size_t _items);
    const Children& items() const;
    const ProfBlockItem& getItem(size_t _index) const;
    ProfBlockItem& getItem(size_t _index);
    size_t addItem();
    size_t addItem(const ProfBlockItem& _item);
    size_t addItem(ProfBlockItem&& _item);
#else
    unsigned short levels() const;
    void setLevels(unsigned short _levels);
    void reserve(unsigned short _level, size_t _items);
    const Children& items(unsigned short _level) const;
    const ProfBlockItem& getItem(unsigned short _level, size_t _index) const;
    ProfBlockItem& getItem(unsigned short _level, size_t _index);
    size_t addItem(unsigned short _level);
    size_t addItem(unsigned short _level, const ProfBlockItem& _item);
    size_t addItem(unsigned short _level, ProfBlockItem&& _item);
#endif

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

    qreal setTree(const BlocksTree::children_t& _children, qreal& _height, qreal _y);
    qreal setTree(ProfGraphicsItem* _item, const BlocksTree::children_t& _children, qreal& _height, qreal _y, unsigned short _level);

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
