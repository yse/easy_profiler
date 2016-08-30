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

#ifndef EASY__GRAPHICS_VIEW__H_
#define EASY__GRAPHICS_VIEW__H_

#include <QGraphicsView>
#include <QGraphicsItem>
#include <QPoint>
#include <QTimer>
#include <QLabel>
#include <QLayout>
#include <stdlib.h>
#include "graphics_scrollbar.h"
#include "profiler/reader.h"
#include "common_types.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class EasyGraphicsView;

//////////////////////////////////////////////////////////////////////////

inline qreal units2microseconds(qreal _value)
{
    return _value;
    //return _value * 1e3;
}

inline qreal microseconds2units(qreal _value)
{
    return _value;
    //return _value * 1e-3;
}

//////////////////////////////////////////////////////////////////////////

class EasyGraphicsItem : public QGraphicsItem
{
    typedef ::profiler_gui::EasyItems       Children;
    typedef ::std::vector<unsigned int>  DrawIndexes;
    typedef ::std::vector<Children>        Sublevels;

    DrawIndexes               m_levelsIndexes; ///< Indexes of first item on each level from which we must start painting
    Sublevels                        m_levels; ///< Arrays of items for each level

    QRectF                     m_boundingRect; ///< boundingRect (see QGraphicsItem)
    const ::profiler::BlocksTreeRoot* m_pRoot; ///< Pointer to the root profiler block (thread block). Used by ProfTreeWidget to restore hierarchy.
    unsigned char                     m_index; ///< This item's index in the list of items of EasyGraphicsView

public:

    EasyGraphicsItem(unsigned char _index, const::profiler::BlocksTreeRoot* _root);
    virtual ~EasyGraphicsItem();

    // Public virtual methods

    QRectF boundingRect() const override;

    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    // Public non-virtual methods

    QRect getRect() const;

    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);
    void setBoundingRect(const QRectF& _rect);

    ::profiler::thread_id_t threadId() const;

    ///< Returns number of levels
    unsigned char levels() const;

    float levelY(unsigned char _level) const;

    /** \brief Sets number of levels.
    
    \note Must be set before doing anything else.
    
    \param _levels Desired number of levels */
    void setLevels(unsigned char _levels);

    /** \brief Reserves memory for desired number of items on specified level.
    
    \param _level Index of the level
    \param _items Desired number of items on this level */
    void reserve(unsigned char _level, unsigned int _items);

    /**\brief Returns reference to the array of items of specified level.
    
    \param _level Index of the level */
    const Children& items(unsigned char _level) const;

    /**\brief Returns reference to the item with required index on specified level.
    
    \param _level Index of the level
    \param _index Index of required item */
    const ::profiler_gui::EasyBlockItem& getItem(unsigned char _level, unsigned int _index) const;

    /**\brief Returns reference to the item with required index on specified level.

    \param _level Index of the level
    \param _index Index of required item */
    ::profiler_gui::EasyBlockItem& getItem(unsigned char _level, unsigned int _index);

    /** \brief Adds new item to required level.
    
    \param _level Index of the level
    
    \retval Index of the new created item */
    unsigned int addItem(unsigned char _level);

    /** \brief Finds top-level blocks which are intersects with required selection zone.

    \note Found blocks will be added into the array of selected blocks.
    
    \param _left Left bound of the selection zone
    \param _right Right bound of the selection zone
    \param _blocks Reference to the array of selected blocks */
    void getBlocks(qreal _left, qreal _right, ::profiler_gui::TreeBlocks& _blocks) const;

    const ::profiler_gui::EasyBlockItem* intersect(const QPointF& _pos) const;

private:

    ///< Returns pointer to the EasyGraphicsView widget.
    const EasyGraphicsView* view() const;

public:

    // Public inline methods

    ///< Returns this item's index in the list of graphics items of EasyGraphicsView
    inline unsigned char index() const
    {
        return m_index;
    }

}; // END of class EasyGraphicsItem.

//////////////////////////////////////////////////////////////////////////

class EasyChronometerItem : public QGraphicsItem
{
    QPolygonF  m_indicator; ///< Indicator displayed when this chrono item is out of screen (displaying only for main item)
    QRectF  m_boundingRect; ///< boundingRect (see QGraphicsItem)
    QColor         m_color; ///< Color of the item
    qreal  m_left, m_right; ///< Left and right bounds of the selection zone
    bool           m_bMain; ///< Is this chronometer main (true, by default)
    bool        m_bReverse; ///< 
    bool m_bHoverIndicator; ///< Mouse hover above indicator

public:

    explicit EasyChronometerItem(bool _main = true);
    virtual ~EasyChronometerItem();

    // Public virtual methods

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    // Public non-virtual methods

    void setColor(const QColor& _color);

    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);
    void setBoundingRect(const QRectF& _rect);

    void setLeftRight(qreal _left, qreal _right);

    void setReverse(bool _reverse);

    void setHover(bool _hover);

    bool contains(const QPointF& _pos) const;

    inline bool hoverIndicator() const
    {
        return m_bHoverIndicator;
    }

    inline bool reverse() const
    {
        return m_bReverse;
    }

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

    ///< Returns pointer to the EasyGraphicsView widget.
    const EasyGraphicsView* view() const;

}; // END of class EasyChronometerItem.

//////////////////////////////////////////////////////////////////////////

#define EASY_QGRAPHICSITEM(ClassName) \
class ClassName : public QGraphicsItem { \
    QRectF m_boundingRect; \
public: \
    ClassName() : QGraphicsItem() {} \
    virtual ~ClassName() {} \
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override; \
    QRectF boundingRect() const override { return m_boundingRect; } \
    void setBoundingRect(qreal x, qreal y, qreal w, qreal h) { m_boundingRect.setRect(x, y, w, h); } \
    void setBoundingRect(const QRectF& _rect) { m_boundingRect = _rect; } \
}

EASY_QGRAPHICSITEM(EasyBackgroundItem);
EASY_QGRAPHICSITEM(EasyTimelineIndicatorItem);

#undef EASY_QGRAPHICSITEM

//////////////////////////////////////////////////////////////////////////

class EasyGraphicsView : public QGraphicsView
{
    Q_OBJECT

private:

    typedef EasyGraphicsView This;
    typedef ::std::vector<EasyGraphicsItem*> Items;

    Items                                   m_items; ///< Array of all EasyGraphicsItem items
    ::profiler_gui::TreeBlocks     m_selectedBlocks; ///< Array of items which were selected by selection zone (EasyChronometerItem)
    QTimer                           m_flickerTimer; ///< Timer for flicking behavior
    QRectF                       m_visibleSceneRect; ///< Visible scene rectangle
    ::profiler::timestamp_t             m_beginTime; ///< Begin time of profiler session. Used to reduce values of all begin and end times of profiler blocks.
    qreal                                   m_scale; ///< Current scale
    qreal                                  m_offset; ///< Have to use manual offset for all scene content instead of using scrollbars because QScrollBar::value is 32-bit integer :(
    qreal                            m_timelineStep; ///< 
    QPoint                          m_mousePressPos; ///< Last mouse global position (used by mousePressEvent and mouseMoveEvent)
    QPoint                          m_mouseMovePath; ///< Mouse move path between press and release of any button
    Qt::MouseButtons                 m_mouseButtons; ///< Pressed mouse buttons
    EasyGraphicsScrollbar*             m_pScrollbar; ///< Pointer to the graphics scrollbar widget
    EasyChronometerItem*          m_chronometerItem; ///< Pointer to the EasyChronometerItem which is displayed when you press right mouse button and move mouse left or right. This item is used to select blocks to display in tree widget.
    EasyChronometerItem*       m_chronometerItemAux; ///< Pointer to the EasyChronometerItem which is displayed when you double click left mouse button and move mouse left or right. This item is used only to measure time.
    int                             m_flickerSpeedX; ///< Current flicking speed x
    int                             m_flickerSpeedY; ///< Current flicking speed y
    bool                             m_bDoubleClick; ///< Is mouse buttons double clicked
    bool                            m_bUpdatingRect; ///< Stub flag which is used to avoid excess calculations on some scene update (flicking, scaling and so on)
    bool                                   m_bEmpty; ///< Indicates whether scene is empty and has no items

public:

    explicit EasyGraphicsView(QWidget* _parent = nullptr);
    virtual ~EasyGraphicsView();

    // Public virtual methods

    void wheelEvent(QWheelEvent* _event) override;
    void mousePressEvent(QMouseEvent* _event) override;
    void mouseDoubleClickEvent(QMouseEvent* _event) override;
    void mouseReleaseEvent(QMouseEvent* _event) override;
    void mouseMoveEvent(QMouseEvent* _event) override;
    void resizeEvent(QResizeEvent* _event) override;

public:

    // Public non-virtual methods

    void setScrollbar(EasyGraphicsScrollbar* _scrollbar);
    void clearSilent();

    void setTree(const ::profiler::thread_blocks_tree_t& _blocksTree);

    const Items& getItems() const;

signals:

    // Signals

    void intervalChanged(const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _session_begin_time, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict);

private:

    // Private non-virtual methods

    EasyChronometerItem* createChronometer(bool _main = true);
    bool moveChrono(EasyChronometerItem* _chronometerItem, qreal _mouseX);
    void initMode();
    void updateVisibleSceneRect();
    void updateTimelineStep(qreal _windowWidth);
    void scaleTo(qreal _scale);
    void onWheel(qreal _mouseX, int _wheelDelta);
    qreal setTree(EasyGraphicsItem* _item, const ::profiler::BlocksTree::children_t& _children, qreal& _height, qreal _y, short _level);

private slots:

    // Private Slots

    void updateScene();
    void onGraphicsScrollbarWheel(qreal _mouseX, int _wheelDelta);
    void onScrollbarValueChange(int);
    void onGraphicsScrollbarValueChange(qreal);
    void onFlickerTimeout();
    void onSelectedThreadChange(::profiler::thread_id_t _id);
    void onSelectedBlockChange(unsigned int _block_index);
    void onItemsEspandStateChange();

public:

    // Public inline methods

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

    inline qreal timelineStep() const
    {
        return m_timelineStep;
    }

private:

    // Private inline methods

    inline qreal time2position(const profiler::timestamp_t& _time) const
    {
        return PROF_MICROSECONDS(qreal(_time - m_beginTime));
        //return PROF_MILLISECONDS(qreal(_time - m_beginTime));
    }

    inline ::profiler::timestamp_t position2time(qreal _pos) const
    {
        return PROF_FROM_MICROSECONDS(_pos);
        //return PROF_FROM_MILLISECONDS(_pos);
    }

}; // END of class EasyGraphicsView.

//////////////////////////////////////////////////////////////////////////

class EasyThreadViewWidget : public QWidget
{
    Q_OBJECT
private:
    EasyGraphicsView*                 m_view;
    QLabel*                           m_label;
    typedef EasyThreadViewWidget This;

    QHBoxLayout *m_layout;

public:
   EasyThreadViewWidget(QWidget *parent, EasyGraphicsView* view);
   virtual ~EasyThreadViewWidget();
public slots:
   void onSelectedThreadChange(::profiler::thread_id_t _id);
};

//////////////////////////////////////////////////////////////////////////

class EasyGraphicsViewWidget : public QWidget
{
    Q_OBJECT

private:

    EasyGraphicsView*                 m_view;
    EasyGraphicsScrollbar* m_scrollbar;
    //EasyThreadViewWidget* m_threadWidget;

public:

    explicit EasyGraphicsViewWidget(QWidget* _parent = nullptr);
    virtual ~EasyGraphicsViewWidget();

    EasyGraphicsView* view();

private:

    void initWidget();

}; // END of class EasyGraphicsViewWidget.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // EASY__GRAPHICS_VIEW__H_
