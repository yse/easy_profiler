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
#include <QScrollBar>
#include <QVBoxLayout>
#include <math.h>
#include <algorithm>
#include "blocks_graphics_view.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const qreal SCALING_COEFFICIENT = 1.25;
const qreal SCALING_COEFFICIENT_INV = 1.0 / SCALING_COEFFICIENT;

const unsigned short GRAPHICS_ROW_SIZE = 16;
const unsigned short GRAPHICS_ROW_SIZE_FULL = GRAPHICS_ROW_SIZE + 2;
const unsigned short ROW_SPACING = 4;
const QRgb DEFAULT_COLOR = 0x00f0e094;

const QRgb BORDERS_COLOR = 0x00a08080;
const QRgb BACKGROUND_1 = 0x00f8f8f8;
const QRgb BACKGROUND_2 = 0x00ffffff;// 0x00e0e0e0;

//////////////////////////////////////////////////////////////////////////

auto const sign = [](int _value) { return _value < 0 ? -1 : 1; };
auto const absmin = [](int _a, int _b) { return abs(_a) < abs(_b) ? _a : _b; };
auto const clamp = [](qreal _minValue, qreal _value, qreal _maxValue) { return _value < _minValue ? _minValue : (_value > _maxValue ? _maxValue : _value); };

//////////////////////////////////////////////////////////////////////////

QRgb BG1();
QRgb BG2();
QRgb (*GetBackgroundColor)() = BG1;

QRgb BG1()
{
    GetBackgroundColor = BG2;
    return BACKGROUND_1;
}

QRgb BG2()
{
    GetBackgroundColor = BG1;
    return BACKGROUND_2;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QRgb toRgb(unsigned int _red, unsigned int _green, unsigned int _blue)
{
    if (_red == 0 && _green == 0 && _blue == 0)
        return DEFAULT_COLOR;
    return (_red << 16) + (_green << 8) + _blue;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProfBlockItem::setRect(qreal _x, float _y, float _w, float _h) {
    x = _x;
    y = _y;
    w = _w;
    h = _h;
}

qreal ProfBlockItem::left() const {
    return x;
}

float ProfBlockItem::top() const {
    return y;
}

float ProfBlockItem::width() const {
    return w;
}

float ProfBlockItem::height() const {
    return h;
}

qreal ProfBlockItem::right() const {
    return x + w;
}

float ProfBlockItem::bottom() const {
    return y + h;
}

//////////////////////////////////////////////////////////////////////////

ProfGraphicsItem::ProfGraphicsItem() : ProfGraphicsItem(false)
{
}

ProfGraphicsItem::ProfGraphicsItem(bool _test) : QGraphicsItem(nullptr), m_bTest(_test), m_backgroundColor(0x00ffffff)
{
}

ProfGraphicsItem::~ProfGraphicsItem()
{
}

const ProfGraphicsView* ProfGraphicsItem::view() const
{
    return static_cast<const ProfGraphicsView*>(scene()->parent());
}

QRectF ProfGraphicsItem::boundingRect() const
{
    //const auto sceneView = view();
    //return QRectF(m_rect.left() - sceneView->offset() / sceneView->scale(), m_rect.top(), m_rect.width() * sceneView->scale(), m_rect.height());
    return m_rect;
}

QRgb ProfGraphicsItem::backgroundColor() const
{
    return m_backgroundColor;
}

void ProfGraphicsItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    if (m_levels.empty() || m_levels.front().empty())
    {
        return;
    }

    const auto sceneView = view();
    const auto currentScale = sceneView->scale(); // Current GraphicsView scale
    const auto visibleSceneRect = sceneView->visibleSceneRect(); // Current visible scene rect
    const auto offset = sceneView->offset();
    const auto sceneLeft = offset, sceneRight = offset + visibleSceneRect.width() / currentScale;

    //printf("VISIBLE = {%lf, %lf}\n", sceneLeft, sceneRight);

    QRectF rect;
    QBrush brush;
    QRgb previousColor = 0;
    Qt::PenStyle previousPenStyle = Qt::SolidLine;
    brush.setStyle(Qt::SolidPattern);

    _painter->save();


    // Reset indices of first visible item for each layer
    const auto levelsNumber = levels();
    for (unsigned short i = 1; i < levelsNumber; ++i)
        m_levelsIndexes[i] = -1;


    // Search for first visible top item
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


    // Draw background --------------------------
    brush.setColor(m_backgroundColor);
    _painter->setBrush(brush);
    _painter->setPen(Qt::NoPen);
    rect.setRect(0, m_rect.top() - (ROW_SPACING >> 1), visibleSceneRect.width(), m_rect.height() + ROW_SPACING);
    _painter->drawRect(rect);
    // END. Draw background. ~~~~~~~~~~~~~~~~~~~~


    // This is to make _painter->drawText() work properly
    // (it seems there is a bug in Qt5.6 when drawText called for big coordinates,
    // drawRect at the same time called for actually same coordinates
    // works fine without using this additional shifting)
    const auto dx = level0[m_levelsIndexes[0]].left() * currentScale;

    // Shifting coordinates to current screen offset
    _painter->setTransform(QTransform::fromTranslate(dx - offset * currentScale, -y()), true);

    _painter->setPen(BORDERS_COLOR);

    // Iterate through layers and draw visible items
    for (unsigned short l = 0; l < levelsNumber; ++l)
    {
        auto& level = m_levels[l];
        const auto next_level = l + 1;
        char state = 1;
        //bool changebrush = false;

        for (unsigned int i = m_levelsIndexes[l], end = level.size(); i < end; ++i)
        {
            auto& item = level[i];

            if (item.state != 0)
            {
                state = item.state;
            }

            if (item.right() < sceneLeft || state == -1)
            {
                ++m_levelsIndexes[l];
                continue;
            }

            auto w = ::std::max(item.width() * currentScale, 1.0);
            if (w < 20)
            {
                // Items which width is less than 20 will be painted as big rectangles which are hiding it's children
                if (item.left() > sceneRight)
                {
                    break;
                }

                const auto x = item.left() * currentScale - dx;
                //if (previousColor != item.color)
                //{
                //    changebrush = true;
                //    previousColor = item.color;
                //    QLinearGradient gradient(x - dx, item.top(), x - dx, item.top() + item.totalHeight);
                //    gradient.setColorAt(0, item.color);
                //    gradient.setColorAt(1, 0x00ffffff);
                //    brush = QBrush(gradient);
                //    _painter->setBrush(brush);
                //}

                if (previousColor != item.color)
                {
                    previousColor = item.color;
                    brush.setColor(previousColor);
                    _painter->setBrush(brush);
                }

                if (w < 3)
                {
                    if (previousPenStyle != Qt::NoPen)
                    {
                        previousPenStyle = Qt::NoPen;
                        _painter->setPen(Qt::NoPen);
                    }
                }
                else if (previousPenStyle != Qt::SolidLine)
                {
                    previousPenStyle = Qt::SolidLine;
                    _painter->setPen(BORDERS_COLOR);
                }

                rect.setRect(x, item.top(), w, item.totalHeight);
                _painter->drawRect(rect);

                if (next_level < levelsNumber && item.children_begin != -1)
                {
                    m_levels[next_level][item.children_begin].state = -1;
                }

                continue;
            }

            if (next_level < levelsNumber && item.children_begin != -1)
            {
                if (m_levelsIndexes[next_level] == -1)
                {
                    m_levelsIndexes[next_level] = item.children_begin;
                }

                m_levels[next_level][item.children_begin].state = 1;
            }

            if (item.left() > sceneRight)
            {
                break;
            }
            
            //if (changebrush)
            //{
            //    changebrush = false;
            //    previousColor = item.color;
            //    brush = QBrush(previousColor);
            //    _painter->setBrush(brush);
            //} else
            if (previousColor != item.color)
            {
                previousColor = item.color;
                brush.setColor(previousColor);
                _painter->setBrush(brush);
            }

            const auto x = item.left() * currentScale - dx;
            rect.setRect(x, item.top(), w, item.height());
            _painter->drawRect(rect);

            auto xtext = x;
            if (item.left() < sceneLeft)
            {
                w += (item.left() - sceneLeft) * currentScale;
                xtext = sceneLeft * currentScale - dx;
            }

            rect.setRect(xtext, item.top(), w, item.height());

            auto textColor = 0x00ffffff - previousColor;
            if (textColor == previousColor) textColor = 0;
            _painter->setPen(textColor); // Text is painted with inverse color

            if (m_bTest)
            {
                char text[128] = {0};
                sprintf(text, "ITEM_%u", i);
                _painter->drawText(rect, 0, text);
            }
            else
            {
                _painter->drawText(rect, 0, item.block->node->getBlockName());
            }

            if (previousPenStyle == Qt::NoPen)
            {
                _painter->setPen(Qt::NoPen);
            }
            else
            {
                _painter->setPen(BORDERS_COLOR); // restore pen for rectangle painting
            }
        }
    }

    _painter->restore();
}

void ProfGraphicsItem::setBackgroundColor(QRgb _color)
{
    m_backgroundColor = _color;
}

void ProfGraphicsItem::setBoundingRect(qreal x, qreal y, qreal w, qreal h)
{
    m_rect.setRect(x, y, w, h);
}

void ProfGraphicsItem::setBoundingRect(const QRectF& _rect)
{
    m_rect = _rect;
}

unsigned short ProfGraphicsItem::levels() const
{
    return static_cast<unsigned short>(m_levels.size());
}

void ProfGraphicsItem::setLevels(unsigned short _levels)
{
    m_levels.resize(_levels);
    m_levelsIndexes.resize(_levels, -1);
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

//////////////////////////////////////////////////////////////////////////

void fillChildren(ProfGraphicsItem* _item, int _level, qreal _x, qreal _y, size_t _childrenNumber, size_t& _total_items)
{
    size_t nchildren = _childrenNumber;
    _childrenNumber >>= 1;

    for (size_t i = 0; i < nchildren; ++i)
    {
        size_t j = _item->addItem(_level);
        auto& b = _item->getItem(_level, j);
        b.color = toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);
        b.state = 0;

        if (_childrenNumber > 0)
        {
            const auto& children = _item->items(_level + 1);
            b.children_begin = static_cast<unsigned int>(children.size());

            fillChildren(_item, _level + 1, _x, _y + GRAPHICS_ROW_SIZE_FULL, _childrenNumber, _total_items);

            const auto& last = children.back();
            b.setRect(_x, _y, last.right() - _x, GRAPHICS_ROW_SIZE);
            b.totalHeight = GRAPHICS_ROW_SIZE_FULL + last.totalHeight;
        }
        else
        {
            b.setRect(_x, _y, 10 + rand() % 40, GRAPHICS_ROW_SIZE);
            b.totalHeight = GRAPHICS_ROW_SIZE;
            b.children_begin = -1;
        }

        _x = b.right();
        ++_total_items;
    }
}

void ProfGraphicsScene::test(size_t _frames_number, size_t _total_items_number_estimate, int _depth)
{
    static const qreal X_BEGIN = 50;
    static const qreal Y_BEGIN = 0;

    const auto children_per_frame = _total_items_number_estimate / _frames_number;
    const size_t first_level_children_count = sqrt(pow(2, _depth - 1)) * pow(children_per_frame, 1.0 / double(_depth)) + 0.5;

    auto item = new ProfGraphicsItem(true);
    item->setPos(0, Y_BEGIN);

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
    qreal x = X_BEGIN;
    for (unsigned int i = 0; i < _frames_number; ++i)
    {
        size_t j = item->addItem(0);
        auto& b = item->getItem(0, j);
        b.color = toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);
        b.state = 0;

        const auto& children = item->items(1);
        b.children_begin = static_cast<unsigned int>(children.size());

        fillChildren(item, 1, x, Y_BEGIN + GRAPHICS_ROW_SIZE_FULL, first_level_children_count, total_items);

        const auto& last = children.back();
        b.setRect(x, Y_BEGIN, last.right() - x, GRAPHICS_ROW_SIZE);
        b.totalHeight = GRAPHICS_ROW_SIZE_FULL + last.totalHeight;

        x += b.width() + 500;

        ++total_items;
    }

    const auto h = item->getItem(0, 0).totalHeight;
    item->setBoundingRect(0, 0, x, h);
    addItem(item);

    setSceneRect(0, 0, x, Y_BEGIN + h);
}

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
    qreal y = 0;
    for (const auto& threadTree : _blocksTree)
    {
        const auto timestart = threadTree.second.children.front().node->block()->getBegin();
        const auto timefinish = threadTree.second.children.back().node->block()->getEnd();
        if (m_beginTime > timestart) m_beginTime = timestart;
        if (finish < timefinish) finish = timefinish;

        // fill scene with new items
        qreal h = 0, x = time2position(timestart);
        auto item = new ProfGraphicsItem();
        item->setLevels(threadTree.second.depth);
        item->setPos(0, y);

        const auto children_duration = setTree(item, threadTree.second.children, h, y, 0);

        item->setBoundingRect(0, 0, children_duration + x, h);
        item->setBackgroundColor(GetBackgroundColor());
        addItem(item);

        y += h + ROW_SPACING;
    }

    const qreal endX = time2position(finish + 1000000);
    setSceneRect(0, 0, endX, y);

    for (auto item : items())
    {
        auto profilerItem = static_cast<ProfGraphicsItem*>(item);
        profilerItem->setBoundingRect(0, 0, endX, profilerItem->boundingRect().height());
    }
}

qreal ProfGraphicsScene::setTree(ProfGraphicsItem* _item, const BlocksTree::children_t& _children, qreal& _height, qreal _y, unsigned short _level)
{
    if (_children.empty())
    {
        return 0;
    }

    _item->reserve(_level, _children.size());

    const auto next_level = _level + 1;
    qreal total_duration = 0, prev_end = 0, maxh = 0;
    qreal start_time = -1;
    for (const auto& child : _children)
    {
        auto xbegin = time2position(child.node->block()->getBegin());
        if (start_time < 0)
        {
            start_time = xbegin;
        }

        auto duration = time2position(child.node->block()->getEnd()) - xbegin;

        const auto dt = xbegin - prev_end;
        if (dt < 0)
        {
            duration += dt;
            xbegin -= dt;
        }

        if (duration < 0.1)
        {
            duration = 0.1;
        }

        auto i = _item->addItem(_level);
        auto& b = _item->getItem(_level, i);

        if (next_level < _item->levels() && !child.children.empty())
        {
            b.children_begin = static_cast<unsigned int>(_item->items(next_level).size());
        }
        else
        {
            b.children_begin = -1;
        }

        qreal h = 0;
        const auto children_duration = setTree(_item, child.children, h, _y + GRAPHICS_ROW_SIZE_FULL, next_level);
        if (duration < children_duration)
        {
            duration = children_duration;
        }

        if (h > maxh)
        {
            maxh = h;
        }

        const auto color = child.node->block()->getColor();
        b.block = &child;
        b.color = toRgb(::profiler::colors::get_red(color), ::profiler::colors::get_green(color), ::profiler::colors::get_blue(color));
        b.setRect(xbegin, _y, duration, GRAPHICS_ROW_SIZE);
        b.totalHeight = GRAPHICS_ROW_SIZE + h;

        prev_end = xbegin + duration;
        total_duration = prev_end - start_time;
    }

    _height += GRAPHICS_ROW_SIZE_FULL + maxh;

    return total_duration;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProfGraphicsView::ProfGraphicsView(bool _test)
    : QGraphicsView()
    , m_scale(1)
    , m_offset(0)
    , m_mouseButtons(Qt::NoButton)
    , m_pScrollbar(nullptr)
    , m_flickerSpeed(0)
    , m_bUpdatingRect(false)
{
    initMode();
    setScene(new ProfGraphicsScene(this, _test));
    updateVisibleSceneRect();
}

ProfGraphicsView::ProfGraphicsView(const thread_blocks_tree_t& _blocksTree)
    : QGraphicsView()
    , m_scale(1)
    , m_offset(0)
    , m_mouseButtons(Qt::NoButton)
    , m_pScrollbar(nullptr)
    , m_flickerSpeed(0)
    , m_bUpdatingRect(false)
{
    initMode();
    setScene(new ProfGraphicsScene(_blocksTree, this));
    updateVisibleSceneRect();
}

ProfGraphicsView::~ProfGraphicsView()
{
}

void ProfGraphicsView::setScrollbar(GraphicsHorizontalScrollbar* _scrollbar)
{
    if (m_pScrollbar)
    {
        disconnect(m_pScrollbar, &GraphicsHorizontalScrollbar::valueChanged, this, &This::onGraphicsScrollbarValueChange);
    }

    m_pScrollbar = _scrollbar;
    m_pScrollbar->setRange(0, scene()->width());
    m_pScrollbar->setSliderWidth(m_visibleSceneRect.width());
    m_pScrollbar->setValue(0);
    connect(m_pScrollbar, &GraphicsHorizontalScrollbar::valueChanged, this, &This::onGraphicsScrollbarValueChange);
}

void ProfGraphicsView::updateVisibleSceneRect()
{
    m_visibleSceneRect = mapToScene(rect()).boundingRect();
}

void ProfGraphicsView::updateScene()
{
    scene()->update(m_visibleSceneRect);
}

void ProfGraphicsView::wheelEvent(QWheelEvent* _event)
{
    const decltype(m_scale) scaleCoeff = _event->delta() > 0 ? SCALING_COEFFICIENT : SCALING_COEFFICIENT_INV;

    // Remember current mouse position
    const auto mouseX = mapToScene(_event->pos()).x();
    const auto mousePosition = m_offset + mouseX / m_scale;

    m_bUpdatingRect = true; // To be sure that updateVisibleSceneRect will not be called by scrollbar change
    m_scale *= scaleCoeff;
    m_bUpdatingRect = false;

    updateVisibleSceneRect(); // Update scene rect

    // Update slider width for scrollbar
    const auto windowWidth = m_visibleSceneRect.width() / m_scale;
    m_pScrollbar->setSliderWidth(windowWidth);

    // Calculate new offset to simulate QGraphicsView::AnchorUnderMouse scaling behavior
    m_offset = clamp(0., mousePosition - mouseX / m_scale, scene()->width() - windowWidth);

    // Update slider position
    m_bUpdatingRect = true;
    m_pScrollbar->setValue(m_offset);
    m_bUpdatingRect = false;

    updateScene(); // repaint scene
    _event->accept();
}

void ProfGraphicsView::mousePressEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();

    if (m_mouseButtons & Qt::LeftButton)
    {
        m_mousePressPos = _event->globalPos();
    }

    _event->accept();
}

void ProfGraphicsView::mouseReleaseEvent(QMouseEvent* _event)
{
    m_mouseButtons = _event->buttons();
    _event->accept();
}

void ProfGraphicsView::mouseMoveEvent(QMouseEvent* _event)
{
    if (m_mouseButtons & Qt::LeftButton)
    {
        const auto pos = _event->globalPos();
        const auto delta = pos - m_mousePressPos;
        m_mousePressPos = pos;

        auto vbar = verticalScrollBar();

        m_bUpdatingRect = true; // Block scrollbars from updating scene rect to make it possible to do it only once
        vbar->setValue(vbar->value() - delta.y());        m_pScrollbar->setValue(m_pScrollbar->value() - delta.x() / m_scale);
        m_bUpdatingRect = false;
        // Seems like an ugly stub, but QSignalBlocker is also a bad decision
        // because if scrollbar does not emit valueChanged signal then viewport does not move

        updateVisibleSceneRect(); // Update scene visible rect only once

        // Update flicker speed
        m_flickerSpeed += delta.x() >> 1;
        if (!m_flickerTimer.isActive())
        {
            // If flicker timer is not started, then start it
            m_flickerTimer.start(20);
        }

        updateScene(); // repaint scene
    }

    _event->accept();
}

void ProfGraphicsView::initMode()
{
    // TODO: find mode with least number of bugs :)
    // There are always some display bugs...

    setCacheMode(QGraphicsView::CacheNone);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &This::onScrollbarValueChange);
    connect(&m_flickerTimer, &QTimer::timeout, this, &This::onFlickerTimeout);
}

void ProfGraphicsView::setTree(const thread_blocks_tree_t& _blocksTree)
{
    // clear scene
    clearSilent();

    // set new blocks tree
    static_cast<ProfGraphicsScene*>(scene())->setTree(_blocksTree);

    // center view on the beginning of the scene
    m_offset = 0;
    updateVisibleSceneRect();
    setScrollbar(m_pScrollbar);
}

void ProfGraphicsView::clearSilent()
{
    // block all signals for faster scene recreation
    const QSignalBlocker b(this);

    // clear scene
    static_cast<ProfGraphicsScene*>(scene())->clearSilent();

    // scale back to initial 100% scale
    m_scale = 1;
}

void ProfGraphicsView::test(size_t _frames_number, size_t _total_items_number_estimate, int _depth)
{
    static_cast<ProfGraphicsScene*>(scene())->test(_frames_number, _total_items_number_estimate, _depth);
    m_offset = 0;
    updateVisibleSceneRect();
    setScrollbar(m_pScrollbar);
}

void ProfGraphicsView::onScrollbarValueChange(int)
{
    if (!m_bUpdatingRect)
        updateVisibleSceneRect();
}

void ProfGraphicsView::onGraphicsScrollbarValueChange(qreal _value)
{
    m_offset = _value;
    if (!m_bUpdatingRect)
    {
        updateVisibleSceneRect();
        updateScene();
    }
}

void ProfGraphicsView::onFlickerTimeout()
{
    if (m_mouseButtons & Qt::LeftButton)
    {
        // Fast slow-down and stop if mouse button is pressed, no flicking.
        m_flickerSpeed >>= 1;
    }
    else
    {
        // Flick when mouse button is not pressed

        m_bUpdatingRect = true; // Block scrollbars from updating scene rect to make it possible to do it only once
        m_pScrollbar->setValue(m_pScrollbar->value() - m_flickerSpeed / m_scale);
        m_bUpdatingRect = false;
        // Seems like an ugly stub, but QSignalBlocker is also a bad decision
        // because if scrollbar does not emit valueChanged signal then viewport does not move

        updateVisibleSceneRect(); // Update scene visible rect only once
        updateScene(); // repaint scene
        m_flickerSpeed -= absmin(3 * sign(m_flickerSpeed), m_flickerSpeed);
    }

    if (m_flickerSpeed == 0)
    {
        // Flicker stopped, no timer needed.
        m_flickerTimer.stop();
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProfGraphicsViewWidget::ProfGraphicsViewWidget(bool _test)
    : QWidget(nullptr)
    , m_scrollbar(new GraphicsHorizontalScrollbar(nullptr))
    , m_view(new ProfGraphicsView(_test))
{
    auto lay = new QVBoxLayout(this);
    lay->addWidget(m_view);
    lay->addWidget(m_scrollbar);
    setLayout(lay);
    m_view->setScrollbar(m_scrollbar);
}

ProfGraphicsViewWidget::ProfGraphicsViewWidget(const thread_blocks_tree_t& _blocksTree)
    : QWidget(false)
    , m_scrollbar(new GraphicsHorizontalScrollbar(nullptr))
    , m_view(new ProfGraphicsView(_blocksTree))
{
    auto lay = new QVBoxLayout(this);
    lay->addWidget(m_view);
    lay->addWidget(m_scrollbar);
    setLayout(lay);
    m_view->setScrollbar(m_scrollbar);
}

ProfGraphicsViewWidget::~ProfGraphicsViewWidget()
{

}

ProfGraphicsView* ProfGraphicsViewWidget::view()
{
    return m_view;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
