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
#include "graphics_scrollbar.h"
#include "profiler/reader.h"
#include "common_types.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class ProfGraphicsView;

class ProfGraphicsItem : public QGraphicsItem
{
    typedef ::profiler_gui::ProfItems       Children;
    typedef ::std::vector<unsigned int>  DrawIndexes;
    typedef ::std::vector<Children>        Sublevels;

    DrawIndexes               m_levelsIndexes; ///< Indexes of first item on each level from which we must start painting
    Sublevels                        m_levels; ///< Arrays of items for each level

    QRectF                     m_boundingRect; ///< boundingRect (see QGraphicsItem)
    const ::profiler::BlocksTreeRoot* m_pRoot; ///< Pointer to the root profiler block (thread block). Used by ProfTreeWidget to restore hierarchy.
    QRgb                    m_backgroundColor; ///< Background color (to enable AlternateColors behavior like in QTreeWidget)
    const bool                        m_bTest; ///< If true then we are running test()

public:

    ProfGraphicsItem();
    ProfGraphicsItem(bool _test);
    ProfGraphicsItem(const ::profiler::BlocksTreeRoot* _root);
    virtual ~ProfGraphicsItem();

    // Public virtual methods

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    // Public non-virtual methods

    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);
    void setBoundingRect(const QRectF& _rect);

    void setBackgroundColor(QRgb _color);

    ::profiler::thread_id_t threadId() const;

    ///< Returns number of levels
    unsigned short levels() const;

    /** \brief Sets number of levels.
    
    \note Must be set before doing anything else.
    
    \param _levels Desired number of levels */
    void setLevels(unsigned short _levels);

    /** \brief Reserves memory for desired number of items on specified level.
    
    \param _level Index of the level
    \param _items Desired number of items on this level */
    void reserve(unsigned short _level, unsigned int _items);

    /**\brief Returns reference to the array of items of specified level.
    
    \param _level Index of the level */
    const Children& items(unsigned short _level) const;

    /**\brief Returns reference to the item with required index on specified level.
    
    \param _level Index of the level
    \param _index Index of required item */
    const ::profiler_gui::ProfBlockItem& getItem(unsigned short _level, unsigned int _index) const;

    /**\brief Returns reference to the item with required index on specified level.

    \param _level Index of the level
    \param _index Index of required item */
    ::profiler_gui::ProfBlockItem& getItem(unsigned short _level, unsigned int _index);

    /** \brief Adds new item to required level.
    
    \param _level Index of the level
    
    \retval Index of the new created item */
    unsigned int addItem(unsigned short _level);

    /** \brief Finds top-level blocks which are intersects with required selection zone.

    \note Found blocks will be added into the array of selected blocks.
    
    \param _left Left bound of the selection zone
    \param _right Right bound of the selection zone
    \param _blocks Reference to the array of selected blocks */
    void getBlocks(qreal _left, qreal _right, ::profiler_gui::TreeBlocks& _blocks) const;

private:

    ///< Returns pointer to the ProfGraphicsView widget.
    const ProfGraphicsView* view() const;

}; // END of class ProfGraphicsItem.

//////////////////////////////////////////////////////////////////////////

class ProfChronometerItem : public QGraphicsItem
{
    QFont           m_font; ///< Font which is used to draw text
    QRectF  m_boundingRect; ///< boundingRect (see QGraphicsItem)
    qreal  m_left, m_right; ///< Left and right bounds of the selection zone

public:

    ProfChronometerItem();
    virtual ~ProfChronometerItem();

    // Public virtual methods

    QRectF boundingRect() const override;
    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

public:

    // Public non-virtual methods

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

    ///< Returns pointer to the ProfGraphicsView widget.
    const ProfGraphicsView* view() const;

}; // END of class ProfChronometerItem.

//////////////////////////////////////////////////////////////////////////

class ProfGraphicsView : public QGraphicsView
{
    Q_OBJECT

private:

    typedef ProfGraphicsView This;
    typedef ::std::vector<ProfGraphicsItem*> Items;

    Items                                   m_items; ///< Array of all ProfGraphicsItem items
    ::profiler_gui::TreeBlocks     m_selectedBlocks; ///< Array of items which were selected by selection zone (ProfChronometerItem)
    QTimer                           m_flickerTimer; ///< Timer for flicking behavior
    QRectF                       m_visibleSceneRect; ///< Visible scene rectangle
    ::profiler::timestamp_t             m_beginTime; ///< Begin time of profiler session. Used to reduce values of all begin and end times of profiler blocks.
    qreal                                   m_scale; ///< Current scale
    qreal                                  m_offset; ///< Have to use manual offset for all scene content instead of using scrollbars because QScrollBar::value is 32-bit integer :(
    QPoint                          m_mousePressPos; ///< Last mouse global position (used by mousePressEvent and mouseMoveEvent)
    Qt::MouseButtons                 m_mouseButtons; ///< Pressed mouse buttons
    ProfGraphicsScrollbar*       m_pScrollbar; ///< Pointer to the graphics scrollbar widget
    ProfChronometerItem*          m_chronometerItem; ///< Pointer to the ProfChronometerItem which is displayed when you press right mouse button and move mouse left or right
    int                              m_flickerSpeed; ///< Current flicking speed
    bool                            m_bUpdatingRect; ///< Stub flag which is used to avoid excess calculations on some scene update (flicking, scaling and so on)
    bool                                    m_bTest; ///< Testing flag (true when test() is called)
    bool                                   m_bEmpty; ///< Indicates whether scene is empty and has no items
    bool                         m_bStrictSelection; ///< Strict selection flag used by ProfTreeWidget to interpret left and right bounds of selection zone in different ways

public:

    ProfGraphicsView(bool _test = false);
    ProfGraphicsView(const ::profiler::thread_blocks_tree_t& _blocksTree);
    virtual ~ProfGraphicsView();

    // Public virtual methods

    void wheelEvent(QWheelEvent* _event) override;
    void mousePressEvent(QMouseEvent* _event) override;
    void mouseReleaseEvent(QMouseEvent* _event) override;
    void mouseMoveEvent(QMouseEvent* _event) override;
    void resizeEvent(QResizeEvent* _event) override;

public:

    // Public non-virtual methods

    void setScrollbar(ProfGraphicsScrollbar* _scrollbar);
    void clearSilent();

    void test(unsigned int _frames_number, unsigned int _total_items_number_estimate, int _rows);
    void setTree(const ::profiler::thread_blocks_tree_t& _blocksTree);

signals:

    // Signals

    void intervalChanged(const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _session_begin_time, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict);

private:

    // Private non-virtual methods

    void initMode();
    void updateVisibleSceneRect();
    void updateScene();
    void scaleTo(qreal _scale);
    qreal setTree(ProfGraphicsItem* _item, const ::profiler::BlocksTree::children_t& _children, qreal& _height, qreal _y, unsigned short _level);
    void fillTestChildren(ProfGraphicsItem* _item, const int _maxlevel, int _level, qreal _x, qreal _y, unsigned int _childrenNumber, unsigned int& _total_items);

private slots:

    // Private Slots

    void onScrollbarValueChange(int);
    void onGraphicsScrollbarValueChange(qreal);
    void onFlickerTimeout();
    void onSelectedThreadChange(::profiler::thread_id_t _id);

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

    inline qreal to_microseconds(qreal _value) const
    {
        return _value;
        //return _value * 1e-3;
    }

    inline qreal to_milliseconds(qreal _value) const
    {
        return _value * 1e3;
        //return _value;
    }

}; // END of class ProfGraphicsView.

//////////////////////////////////////////////////////////////////////////

class ProfGraphicsViewWidget : public QWidget
{
    Q_OBJECT

private:

    ProfGraphicsView*                 m_view;
    ProfGraphicsScrollbar* m_scrollbar;

public:

    ProfGraphicsViewWidget(bool _test = false);
    ProfGraphicsViewWidget(const ::profiler::thread_blocks_tree_t& _blocksTree);
    virtual ~ProfGraphicsViewWidget();

    ProfGraphicsView* view();

private:

}; // END of class ProfGraphicsViewWidget.

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif // MY____GRAPHICS___VIEW_H
