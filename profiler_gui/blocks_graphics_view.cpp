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
*                   : * 2016/06/30 Victor Zarubkin: Replaced doubles with floats (in ProfBlockItem) for less memory consumption.
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
#include <algorithm>
#include "blocks_graphics_view.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const qreal GRAPHICS_ROW_SIZE = 16;
const qreal GRAPHICS_ROW_SIZE_FULL = GRAPHICS_ROW_SIZE + 2;
const qreal ROW_SPACING = 10;
const QRgb DEFAULT_COLOR = 0x00f0e094;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QRgb toRgb(unsigned int _red, unsigned int _green, unsigned int _blue)
{
    if (_red == _green == _blue == 0)
        return DEFAULT_COLOR;
    return (_red << 16) + (_green << 8) + _blue;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProfBlockItem::setRect(preal _x, preal _y, preal _w, preal _h) {
    x = _x;
    y = _y;
    w = _w;
    h = _h;
}

preal ProfBlockItem::left() const {
    return x;
}

preal ProfBlockItem::top() const {
    return y;
}

preal ProfBlockItem::width() const {
    return w;
}

preal ProfBlockItem::height() const {
    return h;
}

preal ProfBlockItem::right() const {
    return x + w;
}

preal ProfBlockItem::bottom() const {
    return y + h;
}

//////////////////////////////////////////////////////////////////////////

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

#if DRAW_METHOD == 0
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
            item.draw = !(item.left() > sceneRight || item.right() < sceneLeft);
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

            rect.setRect(item.x, item.y, item.w, item.h);
            _painter->drawRect(rect);
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

                auto w = item.width() * currentScale;
                if (w < 20)
                    continue;

                rect.setRect(item.left() * currentScale, item.top(), w, item.height());

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

                auto w = item.width() * currentScale;
                if (w < 20)
                    continue;

                rect.setRect(item.left() * currentScale, item.top(), w, item.height());

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
    }

    _painter->restore();
}
#else
void ProfGraphicsItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    if (m_levels.empty() || m_levels.front().empty())
    {
        return;
    }

    const auto selfX = x();
    const auto view = static_cast<ProfGraphicsView*>(scene()->parent());
    const auto visibleSceneRect = view->visibleSceneRect(); // Current visible scene rect
    const auto sceneLeft = visibleSceneRect.left() - selfX, sceneRight = visibleSceneRect.right() - selfX;

    const auto currentScale = view->currentScale(); // Current GraphicsView scale
    const auto scaleRevert = 1.0 / currentScale; // Multiplier for reverting current GraphicsView scale

    QRectF rect;
    QBrush brush;
    QRgb previousColor = 0;
    brush.setStyle(Qt::SolidPattern);

    _painter->save();
    //_painter->setPen(Qt::NoPen);
    _painter->setTransform(QTransform::fromScale(scaleRevert, 1), true);

    const int levelsNumber = static_cast<int>(m_levels.size());
    for (int i = 1; i < levelsNumber; ++i)
        m_levelsIndexes[i] = -1;

    // Searching for first visible top item
    auto& level0 = m_levels[0];
    auto first = ::std::lower_bound(level0.begin(), level0.end(), sceneLeft, [](const ProfBlockItem& _item, double _value)
    {
        return _item.left() < _value;
    });

    if (first != level0.end())
    {
        m_levelsIndexes[0] = first - level0.begin();
        if (m_levelsIndexes[0] > 0)
            m_levelsIndexes[0] -= 1;
    }
    else
    {
        m_levelsIndexes[0] = level0.size() - 1;
    }

    // Iterating through levels and draw visible items
    for (int l = 0; l < levelsNumber; ++l)
    {
        auto& level = m_levels[l];
        const auto next_level = l + 1;

        for (unsigned int i = m_levelsIndexes[l], end = level.size(); i < end; ++i)
        {
            auto& item = level[i];

            if (item.right() < sceneLeft)
            {
                ++m_levelsIndexes[l];
                continue;
            }

            auto w = item.width() * currentScale;
            if (l == 0)
            {
                if (w < 20)
                {
                    // Items which width is less than 20 will be painted as big rectangles which are hiding it's children
                    if (item.left() > sceneRight)
                        break;

                    if (previousColor != item.color)
                    {
                        previousColor = item.color;
                        brush.setColor(previousColor);
                        _painter->setBrush(brush);
                    }

                    //rect.setRect(selfX + item.left(), item.top(), item.width(), item.totalHeight);
                    rect.setRect((selfX + item.left()) * currentScale, item.top(), w, item.totalHeight);
                    _painter->drawRect(rect);
                    continue;
                }
            }

            if (next_level < levelsNumber && m_levelsIndexes[next_level] == -1)
                m_levelsIndexes[next_level] = item.children_begin;

            if (item.left() > sceneRight)
                break;
            
            if (previousColor != item.color)
            {
                previousColor = item.color;
                brush.setColor(previousColor);
                _painter->setBrush(brush);
            }

            if (w < 20)
                _painter->setPen(Qt::NoPen);

            //rect.setRect(selfX + item.left(), item.top(), item.width(), item.height());
            rect.setRect((selfX + item.left()) * currentScale, item.top(), w, item.height());
            _painter->drawRect(rect);

            if (w < 20)
            {
                _painter->setPen(Qt::SolidLine);
                continue;
            }

            if (m_bTest)
            {
                auto xtext = item.left();
                if (xtext < sceneLeft)
                {
                    w += xtext - sceneLeft;
                    xtext = sceneLeft;
                }
                xtext += selfX;

                rect.setRect(xtext * currentScale, item.top(), w, item.height());
                _painter->drawText(rect, 0, "NOT VERY LONG TEST TEXT");
            }
        }
    }

    //printf(" // iterations=%llu\n", iterations);
    _painter->restore();
}
#endif

void ProfGraphicsItem::setBoundingRect(qreal x, qreal y, qreal w, qreal h)
{
    m_rect.setRect(x, y, w, h);
}

void ProfGraphicsItem::setBoundingRect(const QRectF& _rect)
{
    m_rect = _rect;
}

#if DRAW_METHOD == 0
void ProfGraphicsItem::reserve(size_t _items)
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

#else

void ProfGraphicsItem::setLevels(unsigned short _levels)
{
    m_levels.resize(_levels);

#if DRAW_METHOD == 2
    m_levelsIndexes.resize(_levels, -1);
#endif
}

void ProfGraphicsItem::reserve(unsigned short _level, size_t _items)
{
    m_levels[_level].reserve(_items);
}

const ProfGraphicsItem::Children& ProfGraphicsItem::items(unsigned short _level) const
{
    return m_levels[_level];
}

const ProfBlockItem& ProfGraphicsItem::getItem(unsigned short _level, size_t _index) const
{
    return m_levels[_level][_index];
}

ProfBlockItem& ProfGraphicsItem::getItem(unsigned short _level, size_t _index)
{
    return m_levels[_level][_index];
}

size_t ProfGraphicsItem::addItem(unsigned short _level)
{
    m_levels[_level].emplace_back();
    return m_levels[_level].size() - 1;
}

size_t ProfGraphicsItem::addItem(unsigned short _level, const ProfBlockItem& _item)
{
    m_levels[_level].emplace_back(_item);
    return m_levels[_level].size() - 1;
}

size_t ProfGraphicsItem::addItem(unsigned short _level, ProfBlockItem&& _item)
{
    m_levels[_level].emplace_back(::std::forward<ProfBlockItem&&>(_item));
    return m_levels[_level].size() - 1;
}
#endif

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProfGraphicsScene::ProfGraphicsScene(QGraphicsView* _parent, bool _test) : QGraphicsScene(_parent), m_beginTime(-1)
{
    if (_test)
    {
        test(18000, 40000000, 5);
    }
}

ProfGraphicsScene::ProfGraphicsScene(const thread_blocks_tree_t& _blocksTree, QGraphicsView* _parent) : QGraphicsScene(_parent), m_beginTime(-1)
{
    setTree(_blocksTree);
}

ProfGraphicsScene::~ProfGraphicsScene()
{
}

#if DRAW_METHOD == 0
void fillChildren(ProfGraphicsItem* _item, qreal _x, qreal _y, size_t _childrenNumber, size_t& _total_items)
{
    size_t nchildren = _childrenNumber;
    _childrenNumber >>= 1;

    for (size_t i = 0; i < nchildren; ++i)
    {
        size_t j = _item->addItem();

        if (_childrenNumber > 0)
        {
            fillChildren(_item, _x, _y + GRAPHICS_ROW_SIZE + 2, _childrenNumber, _total_items);

            auto& last = _item->items().back();
            auto& b = _item->getItem(j);
            b.color = toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);
            b.setRect(_x, _y, last.right() - _x, GRAPHICS_ROW_SIZE);
            b.totalHeight = last.bottom() - _y;
            _x = b.right();
        }
        else
        {
            auto& b = _item->getItem(j);
            b.color = toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);
            b.setRect(_x, _y, 10 + rand() % 40, GRAPHICS_ROW_SIZE);
            b.totalHeight = GRAPHICS_ROW_SIZE;
            _x = b.right();
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
        fillChildren(item, 0, y + GRAPHICS_ROW_SIZE + 2, first_level_children_count, total_items);

        auto& last = item->items().back();
        auto& b = item->getItem(j);
        b.color = toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);
        b.setRect(0, 0, last.right(), GRAPHICS_ROW_SIZE);
        b.totalHeight = last.bottom() - y;
        item->setBoundingRect(0, 0, b.width(), b.totalHeight);
        x += b.width() + 500;

        if (last.bottom() > h)
        {
            h = last.bottom();
        }

        ++total_items;

        addItem(item);
    }

    setSceneRect(0, 0, x, h);
}

#else //////////////////////////////////////////////////////////////////////////

void fillChildren(ProfGraphicsItem* _item, int _level, qreal _x, qreal _y, size_t _childrenNumber, size_t& _total_items)
{
    size_t nchildren = _childrenNumber;
    _childrenNumber >>= 1;

    for (size_t i = 0; i < nchildren; ++i)
    {
        size_t j = _item->addItem(_level);
        auto& b = _item->getItem(_level, j);
        b.color = toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);

        if (_childrenNumber > 0)
        {
            const auto& children = _item->items(_level + 1);
            b.children_begin = static_cast<unsigned int>(children.size());

            fillChildren(_item, _level + 1, _x, _y + GRAPHICS_ROW_SIZE + 2, _childrenNumber, _total_items);

            const auto& last = children.back();
            b.setRect(_x, _y, last.right() - _x, GRAPHICS_ROW_SIZE);
            b.totalHeight = GRAPHICS_ROW_SIZE + last.totalHeight + 2;
        }
        else
        {
            b.setRect(_x, _y, 10 + rand() % 40, GRAPHICS_ROW_SIZE);
            b.totalHeight = GRAPHICS_ROW_SIZE;
            b.children_begin = 0;
        }

        _x = b.right();
        ++_total_items;
    }
}

void ProfGraphicsScene::test(size_t _frames_number, size_t _total_items_number_estimate, int _depth)
{
    const auto children_per_frame = _total_items_number_estimate / _frames_number;
    const size_t first_level_children_count = sqrt(pow(2, _depth - 1)) * pow(children_per_frame, 1.0 / double(_depth)) + 0.5;

    auto item = new ProfGraphicsItem(true);
    item->setPos(0, 0);

    item->setLevels(_depth + 1);
    item->reserve(0, _frames_number);
    size_t chldrn = first_level_children_count;
    size_t tot = first_level_children_count;
    for (int i = 1; i <= _depth; ++i)
    {
        item->reserve(i, tot * _frames_number);
        chldrn >>= 1;
        tot *= chldrn;
    }

    size_t total_items = 0;
    qreal x = 0, y = 0, h = 0;
    for (unsigned int i = 0; i < _frames_number; ++i)
    {
        size_t j = item->addItem(0);
        auto& b = item->getItem(0, j);
        b.color = toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);

        const auto& children = item->items(1);
        b.children_begin = static_cast<unsigned int>(children.size());

        fillChildren(item, 1, x, y + GRAPHICS_ROW_SIZE + 2, first_level_children_count, total_items);

        const auto& last = children.back();
        b.setRect(x, 0, last.right() - x, GRAPHICS_ROW_SIZE);
        b.totalHeight = GRAPHICS_ROW_SIZE + last.totalHeight + 2;

        x += b.width() + 500;

        if (last.bottom() > h)
        {
            h = last.bottom();
        }

        ++total_items;
    }

    item->setBoundingRect(0, 0, x, item->getItem(0, 0).totalHeight);
    addItem(item);

    setSceneRect(0, 0, x, h);
}
#endif

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsScene::clearSilent()
{
    const QSignalBlocker b(this); // block all scene signals (otherwise clear() would be extremely slow!)
    clear();
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
#if DRAW_METHOD == 0
        setTree(threadTree.second.children, h, y);
#else
        auto item = new ProfGraphicsItem();
        item->setLevels(threadTree.second.sublevels);
        const auto children_duration = setTree(item, threadTree.second.children, h, y, 0);
        addItem(item);
#endif
        y += h + ROW_SPACING;
    }

    const qreal endX = time2position(finish + 1000000);
    setSceneRect(0, 0, endX, y + ROW_SPACING);
}

qreal ProfGraphicsScene::setTree(const BlocksTree::children_t& _children, qreal& _height, qreal _y)
{
#if DRAW_METHOD > 0
    return 0;
#else
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
        const auto children_duration = setTree(item, child.children, h, _y + GRAPHICS_ROW_SIZE_FULL, 1);
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
        b.color = toRgb(::profiler::colors::get_red(color), ::profiler::colors::get_green(color), ::profiler::colors::get_blue(color));
        b.setRect(0, 0, duration, GRAPHICS_ROW_SIZE);
        b.totalHeight = GRAPHICS_ROW_SIZE + h;

        item->setBoundingRect(0, 0, duration, b.totalHeight);
        addItem(item);

        total_duration += duration;

        prev_end = xbegin + duration;
    }

    _height += GRAPHICS_ROW_SIZE + maxh;

    return total_duration;
#endif
}

qreal ProfGraphicsScene::setTree(ProfGraphicsItem* _item, const BlocksTree::children_t& _children, qreal& _height, qreal _y, unsigned int _level)
{
    if (_children.empty())
    {
        return 0;
    }

#if DRAW_METHOD > 0
    _item->reserve(_level, _children.size());
#endif

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

#if DRAW_METHOD == 0
        auto i = _item->addItem();
#else
        auto i = _item->addItem(_level);
#endif

        qreal h = 0;
        const auto children_duration = setTree(_item, child.children, h, _y + GRAPHICS_ROW_SIZE_FULL, _level + 1);
        if (duration < children_duration)
        {
            duration = children_duration;
        }

        if (h > maxh)
        {
            maxh = h;
        }

        const auto color = child.node->block()->getColor();
#if DRAW_METHOD == 0
        auto& b = _item->getItem(i);
#else
        auto& b = _item->getItem(_level, i);
#endif
        b.block = &child;
        b.color = toRgb(::profiler::colors::get_red(color), ::profiler::colors::get_green(color), ::profiler::colors::get_blue(color));
        b.setRect(xbegin - _item->x(), _y - _item->y(), duration, GRAPHICS_ROW_SIZE);
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
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

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
