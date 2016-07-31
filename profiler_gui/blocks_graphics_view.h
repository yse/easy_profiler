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
#include <QGraphicsItem>
#include <QFont>
#include <QPoint>
#include <QTimer>
#include <stdlib.h>
#include <vector>
#include "graphics_scrollbar.h"
#include "profiler/reader.h"
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#pragma pack(push, 1)
struct ProfBlockItem
{
    const BlocksTree*       block;
    qreal                       x;
    float                       w;
    float                       y;
    float                       h;
    QRgb                    color;
    unsigned int   children_begin;
    unsigned short    totalHeight;
    char                    state;

    void setRect(qreal _x, float _y, float _w, float _h);
    qreal left() const;
    float top() const;
    float width() const;
    float height() const;
    qreal right() const;
    float bottom() const;
};
#pragma pack(pop)

//////////////////////////////////////////////////////////////////////////

class ProfGraphicsView;

class ProfGraphicsItem : public QGraphicsItem
{
    typedef ::std::vector<ProfBlockItem>    Children;
    typedef ::std::vector<unsigned int>  DrawIndexes;
    typedef ::std::vector<Children>        Sublevels;

    DrawIndexes  m_levelsIndexes;
    Sublevels           m_levels;

    QRectF               m_boundingRect;
    const BlocksTree*           m_pRoot;
    ::profiler::thread_id_t m_thread_id;
    QRgb              m_backgroundColor;
    const bool                  m_bTest;

public:

    ProfGraphicsItem();
    ProfGraphicsItem(bool _test);
    ProfGraphicsItem(::profiler::thread_id_t _thread_id, const BlocksTree* _root);
    virtual ~ProfGraphicsItem();

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);
    void setBoundingRect(const QRectF& _rect);

    void setBackgroundColor(QRgb _color);

    unsigned short levels() const;
    void setLevels(unsigned short _levels);
    void reserve(unsigned short _level, size_t _items);
    const Children& items(unsigned short _level) const;
    const ProfBlockItem& getItem(unsigned short _level, size_t _index) const;
    ProfBlockItem& getItem(unsigned short _level, size_t _index);
    size_t addItem(unsigned short _level);
    size_t addItem(unsigned short _level, const ProfBlockItem& _item);
    size_t addItem(unsigned short _level, ProfBlockItem&& _item);

    void getBlocks(qreal _left, qreal _right, TreeBlocks& _blocks) const;

private:

    const ProfGraphicsView* view() const;

}; // END of class ProfGraphicsItem.

//////////////////////////////////////////////////////////////////////////

class ProfChronometerItem : public QGraphicsItem
{
    QFont           m_font;
    QRectF  m_boundingRect;
    qreal  m_left, m_right;

public:

    ProfChronometerItem();
    virtual ~ProfChronometerItem();

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);
    void setBoundingRect(const QRectF& _rect);
    void setLeftRight(qreal _left, qreal _right);

    inline qreal left() const
    {
        return m_left;
    }

    inline qreal right() const
    {
        return m_right;
    }

    inline qreal width() const
    {
        return m_right - m_left;
    }

private:

    const ProfGraphicsView* view() const;

}; // END of class ProfChronometerItem.

//////////////////////////////////////////////////////////////////////////

class ProfGraphicsView : public QGraphicsView
{
    Q_OBJECT

private:

    typedef ProfGraphicsView This;
    typedef ::std::vector<ProfGraphicsItem*> Items;

    Items                                   m_items;
    TreeBlocks                     m_selectedBlocks;
    QTimer                           m_flickerTimer;
    QRectF                       m_visibleSceneRect;
    ::profiler::timestamp_t             m_beginTime;
    qreal                                   m_scale;
    qreal                                  m_offset; ///< Have to use manual offset for all scene content instead of using scrollbars because QScrollBar::value is 32-bit integer :(
    QPoint                          m_mousePressPos;
    Qt::MouseButtons                 m_mouseButtons;
    GraphicsHorizontalScrollbar*       m_pScrollbar;
    ProfChronometerItem*          m_chronometerItem;
    int                              m_flickerSpeed;
    bool                            m_bUpdatingRect;
    bool                                    m_bTest;
    bool                                   m_bEmpty;

public:

    ProfGraphicsView(bool _test = false);
    ProfGraphicsView(const thread_blocks_tree_t& _blocksTree);
    virtual ~ProfGraphicsView();

    void wheelEvent(QWheelEvent* _event) override;
    void mousePressEvent(QMouseEvent* _event) override;
    void mouseReleaseEvent(QMouseEvent* _event) override;
    void mouseMoveEvent(QMouseEvent* _event) override;
    void resizeEvent(QResizeEvent* _event) override;

    inline qreal scale() const
    {
        return m_scale;
    }

    inline qreal offset() const
    {
        return m_offset;
    }

    inline const QRectF& visibleSceneRect() const
    {
        return m_visibleSceneRect;
    }

    inline qreal time2position(const profiler::timestamp_t& _time) const
    {
        return qreal(_time - m_beginTime) * 1e-6;
    }

    void setScrollbar(GraphicsHorizontalScrollbar* _scrollbar);
    void clearSilent();

    void test(size_t _frames_number, size_t _total_items_number_estimate, int _depth);
    void setTree(const thread_blocks_tree_t& _blocksTree);

signals:

    void treeblocksChanged(const TreeBlocks& _blocks, ::profiler::timestamp_t _begin_time);

private:

    void initMode();
    void updateVisibleSceneRect();
    void updateScene();
    qreal setTree(ProfGraphicsItem* _item, const BlocksTree::children_t& _children, qreal& _height, qreal _y, unsigned short _level);

private slots:

    void onScrollbarValueChange(int);
    void onGraphicsScrollbarValueChange(qreal);
    void onFlickerTimeout();

}; // END of class ProfGraphicsView.

//////////////////////////////////////////////////////////////////////////

class ProfGraphicsViewWidget : public QWidget
{
    Q_OBJECT

private:

    ProfGraphicsView*                 m_view;
    GraphicsHorizontalScrollbar* m_scrollbar;

public:

    ProfGraphicsViewWidget(bool _test = false);
    ProfGraphicsViewWidget(const thread_blocks_tree_t& _blocksTree);
    virtual ~ProfGraphicsViewWidget();

    ProfGraphicsView* view();

private:

}; // END of class ProfGraphicsViewWidget.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // MY____GRAPHICS___VIEW_H
