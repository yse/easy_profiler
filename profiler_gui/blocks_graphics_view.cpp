/************************************************************************
* file name         : blocks_graphics_view.cpp
* ----------------- :
* creation time     : 2016/06/26
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of GraphicsScene and GraphicsView and
*                   : it's auxiliary classes for displyaing easy_profiler blocks tree.
* ----------------- :
* change log        : * 2016/06/26 Victor Zarubkin: Moved sources from graphics_view.h
*                   :       and renamed classes from My* to Prof*.
*                   :
*                   : * 2016/06/27 Victor Zarubkin: Added text shifting relatively to it's parent item.
*                   :       Disabled border lines painting because of vertical lines painting bug.
*                   :       Changed height of blocks. Variable thread-block height.
*                   :
*                   : * 2016/06/29 Victor Zarubkin: Highly optimized painting performance and memory consumption.
*                   :
*                   : * 
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#include <QWheelEvent>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QScrollBar>
#include <math.h>
#include "blocks_graphics_view.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const qreal GRAPHICS_ROW_SIZE = 16;
const qreal GRAPHICS_ROW_SIZE_FULL = GRAPHICS_ROW_SIZE + 1;
const qreal ROW_SPACING = 10;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProfGraphicsItem::ProfGraphicsItem() : QGraphicsItem(nullptr), m_bTest(false)
{
}

ProfGraphicsItem::ProfGraphicsItem(bool _test) : QGraphicsItem(nullptr), m_bTest(_test)
{
}

ProfGraphicsItem::~ProfGraphicsItem()
{
}

QRectF ProfGraphicsItem::boundingRect() const
{
    return m_rect;
}

void ProfGraphicsItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    const auto nitems = m_items.size();
    if (nitems == 0)
    {
        return;
    }

    const auto view = static_cast<ProfGraphicsView*>(scene()->parent());
    const auto visibleSceneRect = view->visibleSceneRect(); // Current visible scene rect
    const auto sceneLeft = visibleSceneRect.left() - x(), sceneRight = visibleSceneRect.right() - x();
    if (m_rect.left() > sceneRight || m_rect.right() < sceneLeft)
    {
        // this frame is not visible at all
        return;
    }

    const auto currentScale = view->currentScale(); // Current GraphicsView scale
    const auto scaleRevert = 1.0 / currentScale; // Multiplier for reverting current GraphicsView scale
    const auto screenWidth = m_rect.width() * currentScale; // Screen width of the top block

    QRectF rect;
    QBrush brush;
    QPen pen = _painter->pen();
    brush.setStyle(Qt::SolidPattern);

    _painter->save();

    _painter->setPen(Qt::NoPen);
    if (screenWidth > 20)
    {
        // TODO: Test performance for drawText inside one loop with drawRect, but with _painter->save() and restore()
        //       and drawText in separate loop.

        QRgb previousColor = 0;
        for (auto& item : m_items)
        {
            item.draw = !(item.rect.left() > sceneRight || item.rect.right() < sceneLeft);
            if (!item.draw)
            {
                // this item is fully out of scene visible rect
                continue;
            }

            if (previousColor != item.color)
            {
                previousColor = item.color;
                brush.setColor(previousColor);
                _painter->setBrush(brush);
            }

            _painter->drawRect(item.rect);
        }

        _painter->setPen(Qt::SolidLine);
        //_painter->setBrush(Qt::NoBrush);
        _painter->setTransform(QTransform::fromScale(scaleRevert, 1), true);
        if (m_bTest)
        {
            for (auto& item : m_items)
            {
                if (!item.draw)
                    continue;

                auto w = item.rect.width() * currentScale;
                if (w < 20)
                    continue;

                rect.setRect(item.rect.left() * currentScale, item.rect.top(), w, item.rect.height());

                _painter->drawText(rect, 0, "NOT VERY LONG TEST TEXT");
            }
        }
        else
        {
            previousColor = 0;
            for (auto& item : m_items)
            {
                if (!item.draw)
                    continue;

                auto w = item.rect.width() * currentScale;
                if (w < 20)
                    continue;

                rect.setRect(item.rect.left() * currentScale, item.rect.top(), w, item.rect.height());

                if (previousColor != item.color)
                {
                    previousColor = item.color;
                    pen.setColor(0x00ffffff - previousColor);
                    _painter->setPen(pen);
                }

                //_painter->drawRect(rect);
                _painter->drawText(rect, 0, item.block->node->getBlockName());
            }
        }
    }
    else
    {
        const auto& item = m_items.front();
        brush.setColor(item.color);
        _painter->setBrush(brush);
        _painter->drawRect(m_rect);

        //if (screenWidth > 10)
        //{
        //    _painter->setTransform(QTransform::fromScale(scaleRevert, 1), true);
        //    //_painter->setBrush(Qt::NoBrush);
        //    rect.setRect(m_rect.left() * currentScale, m_rect.top(), screenWidth, m_rect.height());
        //    if (m_bTest)
        //    {
        //        _painter->setPen(Qt::SolidLine);
        //        _painter->drawText(rect, 0, "NOT VERY LONG TEST TEXT");
        //    }
        //    else
        //    {
        //        pen.setColor(0x00ffffff - item.color);
        //        _painter->setPen(pen);
        //        //_painter->drawRect(rect);
        //        _painter->drawText(rect, 0, item.block->node->getBlockName());
        //    }
        //}
    }

    _painter->restore();
}

void ProfGraphicsItem::setBoundingRect(qreal x, qreal y, qreal w, qreal h)
{
    m_rect.setRect(x, y, w, h);
}

void ProfGraphicsItem::setBoundingRect(const QRectF& _rect)
{
    m_rect = _rect;
}

void ProfGraphicsItem::reserve(unsigned int _items)
{
    m_items.reserve(_items);
}

const ProfGraphicsItem::Children& ProfGraphicsItem::items() const
{
    return m_items;
}

const ProfBlockItem& ProfGraphicsItem::getItem(size_t _index) const
{
    return m_items[_index];
}

ProfBlockItem& ProfGraphicsItem::getItem(size_t _index)
{
    return m_items[_index];
}

size_t ProfGraphicsItem::addItem()
{
    m_items.emplace_back();
    return m_items.size() - 1;
}

size_t ProfGraphicsItem::addItem(const ProfBlockItem& _item)
{
    m_items.emplace_back(_item);
    return m_items.size() - 1;
}

size_t ProfGraphicsItem::addItem(ProfBlockItem&& _item)
{
    m_items.emplace_back(::std::forward<ProfBlockItem&&>(_item));
    return m_items.size() - 1;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProfGraphicsScene::ProfGraphicsScene(QGraphicsView* _parent, bool _test) : QGraphicsScene(_parent), m_beginTime(-1)
{
    if (_test)
    {
        test(10000, 20000000, 5);
    }
}

ProfGraphicsScene::ProfGraphicsScene(const thread_blocks_tree_t& _blocksTree, QGraphicsView* _parent) : QGraphicsScene(_parent), m_beginTime(-1)
{
    setTree(_blocksTree);
}

ProfGraphicsScene::~ProfGraphicsScene()
{
}

void fillChildren(ProfGraphicsItem* _item, qreal _x, qreal _y, size_t _childrenNumber, size_t& _total_items)
{
    size_t nchildren = _childrenNumber;
    _childrenNumber >>= 1;

    for (size_t i = 0; i < nchildren; ++i)
    {
        size_t j = _item->addItem();

        if (_childrenNumber > 0)
        {
            fillChildren(_item, _x, _y + 18, _childrenNumber, _total_items);

            auto& last = _item->items().back();
            auto& b = _item->getItem(j);
            b.color = ((30 + rand() % 225) << 16) + ((30 + rand() % 225) << 8) + (30 + rand() % 225);
            b.rect.setRect(_x, _y, last.rect.right() - _x, 15);
            b.totalHeight = last.rect.bottom() - _y;
            _x = b.rect.right();
        }
        else
        {
            auto& b = _item->getItem(j);
            b.color = ((30 + rand() % 225) << 16) + ((30 + rand() % 225) << 8) + (30 + rand() % 225);
            b.rect.setRect(_x, _y, 10 + rand() % 40, 15);
            b.totalHeight = 15;
            _x = b.rect.right();
        }

        ++_total_items;
    }
}

void ProfGraphicsScene::test(size_t _frames_number, size_t _total_items_number_estimate, int _depth)
{
    const auto children_per_frame = _total_items_number_estimate / _frames_number;
    const size_t first_level_children_count = sqrt(pow(2, _depth - 1)) * pow(children_per_frame, 1.0 / double(_depth)) + 0.5;

    size_t total_items = 0;
    qreal x = 0, y = 0, h = 0;
    for (unsigned int i = 0; i < _frames_number; ++i)
    {
        auto item = new ProfGraphicsItem(true);
        item->setPos(x, 0);

        size_t j = item->addItem();
        fillChildren(item, 0, y + 18, first_level_children_count, total_items);

        auto& last = item->items().back();
        auto& b = item->getItem(j);
        b.color = ((30 + rand() % 225) << 16) + ((30 + rand() % 225) << 8) + (30 + rand() % 225);
        b.rect.setRect(0, 0, last.rect.right(), 15);
        b.totalHeight = last.rect.bottom() - y;
        item->setBoundingRect(0, 0, b.rect.width(), b.totalHeight);
        x += b.rect.width() + 500;

        if (last.rect.bottom() > h)
        {
            h = last.rect.bottom();
        }

        item->addItem(b);
        ++total_items;

        addItem(item);
    }

    setSceneRect(0, 0, x, h);
}

void ProfGraphicsScene::clearSilent()
{
    const QSignalBlocker b(this); // block all scene signals (otherwise clear() would be extremely slow!)
    clear(); // clear would be VERY SLOW if signals would not be blocked!
    m_beginTime = -1;
}

void ProfGraphicsScene::setTree(const thread_blocks_tree_t& _blocksTree)
{
    // calculate scene size and fill it with items
    ::profiler::timestamp_t finish = 0;
    qreal y = ROW_SPACING;
    for (const auto& threadTree : _blocksTree)
    {
        const auto timestart = threadTree.second.children.front().node->block()->getBegin();
        const auto timefinish = threadTree.second.children.back().node->block()->getEnd();
        if (m_beginTime > timestart) m_beginTime = timestart;
        if (finish < timefinish) finish = timefinish;

        // fill scene with new items
        qreal h = 0;
        setTreeInternal(threadTree.second.children, h, y);
        y += h + ROW_SPACING;
    }

    const qreal endX = time2position(finish + 1000000);
    setSceneRect(QRectF(0, 0, endX, y + ROW_SPACING));
}

qreal ProfGraphicsScene::setTreeInternal(const BlocksTree::children_t& _children, qreal& _height, qreal _y)
{
    qreal total_duration = 0, prev_end = 0, maxh = 0;
    for (const auto& child : _children)
    {
        qreal xbegin = time2position(child.node->block()->getBegin());
        qreal duration = time2position(child.node->block()->getEnd()) - xbegin;

        const qreal dt = xbegin - prev_end;
        if (dt < 0)
        {
            duration += dt;
            xbegin -= dt;
        }

        if (duration < 0.1)
        {
            duration = 0.1;
        }

        auto item = new ProfGraphicsItem();
        item->setPos(xbegin, _y);
        item->reserve(child.total_children_number + 1);
        auto i = item->addItem();

        qreal h = 0;
        const auto children_duration = setTreeInternal(item, child.children, h, _y + GRAPHICS_ROW_SIZE_FULL);
        if (duration < children_duration)
        {
            duration = children_duration;
        }

        if (h > maxh)
        {
            maxh = h;
        }

        const auto color = child.node->block()->getColor();
        auto& b = item->getItem(i);
        b.block = &child;
        b.color = (::profiler::colors::get_red(color) << 16) + (::profiler::colors::get_green(color) << 8) + ::profiler::colors::get_blue(color);
        b.rect.setRect(0, 0, duration, GRAPHICS_ROW_SIZE);
        b.totalHeight = GRAPHICS_ROW_SIZE + h;

        item->setBoundingRect(0, 0, duration, b.totalHeight);
        addItem(item);

        total_duration += duration;

        prev_end = xbegin + duration;
    }

    _height += GRAPHICS_ROW_SIZE + maxh;

    return total_duration;
}

qreal ProfGraphicsScene::setTreeInternal(ProfGraphicsItem* _item, const BlocksTree::children_t& _children, qreal& _height, qreal _y)
{
    if (_children.empty())
    {
        return 0;
    }

    qreal total_duration = 0, prev_end = 0, maxh = 0;
    for (const auto& child : _children)
    {
        qreal xbegin = time2position(child.node->block()->getBegin());
        qreal duration = time2position(child.node->block()->getEnd()) - xbegin;

        const qreal dt = xbegin - prev_end;
        if (dt < 0)
        {
            duration += dt;
            xbegin -= dt;
        }

        if (duration < 0.1)
        {
            duration = 0.1;
        }

        auto i = _item->addItem();

        qreal h = 0;
        const auto children_duration = setTreeInternal(_item, child.children, h, _y + GRAPHICS_ROW_SIZE_FULL);
        if (duration < children_duration)
        {
            duration = children_duration;
        }

        if (h > maxh)
        {
            maxh = h;
        }

        const auto color = child.node->block()->getColor();
        auto& b = _item->getItem(i);
        b.block = &child;
        b.color = (::profiler::colors::get_red(color) << 16) + (::profiler::colors::get_green(color) << 8) + ::profiler::colors::get_blue(color);
        b.rect.setRect(xbegin - _item->x(), _y - _item->y(), duration, GRAPHICS_ROW_SIZE);
        b.totalHeight = GRAPHICS_ROW_SIZE + h;

        total_duration += duration;

        prev_end = xbegin + duration;
    }

    _height += GRAPHICS_ROW_SIZE_FULL + maxh;

    return total_duration;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProfGraphicsView::ProfGraphicsView(bool _test) : QGraphicsView(), m_scale(1), m_scaleCoeff(1.25), m_mouseButtons(Qt::NoButton), m_bUpdatingRect(false)
{
    initMode();
    setScene(new ProfGraphicsScene(this, _test));
    centerOn(0, 0);
    updateVisibleSceneRect();
}

ProfGraphicsView::ProfGraphicsView(const thread_blocks_tree_t& _blocksTree) : QGraphicsView(), m_scale(1), m_scaleCoeff(1.25), m_mouseButtons(Qt::NoButton), m_bUpdatingRect(false)
{
    initMode();
    setScene(new ProfGraphicsScene(_blocksTree, this));
    centerOn(0, 0);
    updateVisibleSceneRect();
}

ProfGraphicsView::~ProfGraphicsView()
{
}

void ProfGraphicsView::wheelEvent(QWheelEvent* _event)
{
    qreal scaleCoeff;
    if (_event->delta() > 0)
    {
        scaleCoeff = m_scaleCoeff;
    }
    else
    {
        scaleCoeff = 1. / m_scaleCoeff;
    }

    m_bUpdatingRect = true; // To be sure that updateVisibleSceneRect will not be called by scrollbar change
    scale(scaleCoeff, 1); // Scale scene
    m_scale *= scaleCoeff;
    m_bUpdatingRect = false;

    updateVisibleSceneRect(); // Update scene rect

    _event->accept();
    //QGraphicsView::wheelEvent(_event);
}

void ProfGraphicsView::mousePressEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();

    if (m_mouseButtons & Qt::LeftButton)
    {
        m_mousePressPos = _event->globalPos();
    }

    _event->accept();
    //QGraphicsView::mousePressEvent(_event);
}

void ProfGraphicsView::mouseReleaseEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();
    _event->accept();
    //QGraphicsView::mouseReleaseEvent(_event);
}

void ProfGraphicsView::mouseMoveEvent(QMouseEvent* _event)
{
    if (m_mouseButtons & Qt::LeftButton)
    {
        const auto pos = _event->globalPos();
        const auto delta = pos - m_mousePressPos;
        m_mousePressPos = pos;

        auto vbar = verticalScrollBar();
        auto hbar = horizontalScrollBar();

        m_bUpdatingRect = true; // Block scrollbars from updating scene rect to make it possible to do it only once
        vbar->setValue(vbar->value() - delta.y());        hbar->setValue(hbar->value() - delta.x());
        m_bUpdatingRect = false;
        // Seems like an ugly stub, but QSignalBlocker is also a bad decision
        // because if scrollbar does not emit valueChanged signal then viewport does not move

        updateVisibleSceneRect(); // Update scene visible rect only once
    }

    //_event->accept();
    QGraphicsView::mouseMoveEvent(_event); // This is to be able to use setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void ProfGraphicsView::initMode()
{
    // TODO: find mode with least number of bugs :)
    // There are always some display bugs...

    setCacheMode(QGraphicsView::CacheNone);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

    connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, &This::onScrollbarValueChange);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &This::onScrollbarValueChange);
}

void ProfGraphicsView::updateVisibleSceneRect()
{
    m_visibleSceneRect = mapToScene(rect()).boundingRect();
}

void ProfGraphicsView::setTree(const thread_blocks_tree_t& _blocksTree)
{
    // clear scene
    clearSilent();

    // set new blocks tree
    static_cast<ProfGraphicsScene*>(scene())->setTree(_blocksTree);

    // center view on the beginning of the scene
    centerOn(0, 0);
    updateVisibleSceneRect();
}

void ProfGraphicsView::clearSilent()
{
    // block all signals for faster scene recreation
    const QSignalBlocker b(this);

    // clear scene
    static_cast<ProfGraphicsScene*>(scene())->clearSilent();

    // scale back to initial 100% scale
    scale(1. / m_scale, 1);
    m_scale = 1;
}

void ProfGraphicsView::test(size_t _frames_number, size_t _total_items_number_estimate, int _depth)
{
    static_cast<ProfGraphicsScene*>(scene())->test(_frames_number, _total_items_number_estimate, _depth);
    updateVisibleSceneRect();
}

void ProfGraphicsView::onScrollbarValueChange(int)
{
    if (!m_bUpdatingRect)
        updateVisibleSceneRect();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
