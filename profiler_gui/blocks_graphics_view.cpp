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
#include <QFontMetrics>
#include <math.h>
#include <algorithm>
#include "blocks_graphics_view.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

const qreal SCALING_COEFFICIENT = 1.25;
const qreal SCALING_COEFFICIENT_INV = 1.0 / SCALING_COEFFICIENT;
const qreal MIN_SCALE = pow(SCALING_COEFFICIENT_INV, 70);
const qreal MAX_SCALE = pow(SCALING_COEFFICIENT, 30); // ~800
const qreal BASE_SCALE = pow(SCALING_COEFFICIENT_INV, 25); // ~0.003

const unsigned short GRAPHICS_ROW_SIZE = 16;
const unsigned short GRAPHICS_ROW_SIZE_FULL = GRAPHICS_ROW_SIZE + 2;
const unsigned short ROW_SPACING = 4;

const QRgb BORDERS_COLOR = 0x00a07050;
const QRgb BACKGROUND_1 = 0x00dddddd;
const QRgb BACKGROUND_2 = 0x00ffffff;
const QColor CHRONOMETER_COLOR = QColor(64, 64, 64, 64);
const QRgb CHRONOMETER_TEXT_COLOR = 0xff302010;

const int TEST_PROGRESSION_BASE = 4;

//////////////////////////////////////////////////////////////////////////

auto const sign = [](int _value) { return _value < 0 ? -1 : 1; };
auto const absmin = [](int _a, int _b) { return abs(_a) < abs(_b) ? _a : _b; };
auto const clamp = [](qreal _minValue, qreal _value, qreal _maxValue) { return _value < _minValue ? _minValue : (_value > _maxValue ? _maxValue : _value); };

//////////////////////////////////////////////////////////////////////////

template <int N, class T>
inline T logn(T _value)
{
    static const double div = 1.0 / log2((double)N);
    return log2(_value) * div;
}

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

//////////////////////////////////////////////////////////////////////////

ProfGraphicsItem::ProfGraphicsItem() : ProfGraphicsItem(false)
{
}

ProfGraphicsItem::ProfGraphicsItem(bool _test) : QGraphicsItem(nullptr), m_bTest(_test), m_backgroundColor(0x00ffffff), m_pRoot(nullptr)
{
}

ProfGraphicsItem::ProfGraphicsItem(const ::profiler::BlocksTreeRoot* _root) : ProfGraphicsItem(false)
{
    m_pRoot = _root;
}

ProfGraphicsItem::~ProfGraphicsItem()
{
}

const ProfGraphicsView* ProfGraphicsItem::view() const
{
    return static_cast<const ProfGraphicsView*>(scene()->parent());
}

//////////////////////////////////////////////////////////////////////////

QRectF ProfGraphicsItem::boundingRect() const
{
    //const auto sceneView = view();
    //return QRectF(m_boundingRect.left() - sceneView->offset() / sceneView->scale(), m_boundingRect.top(), m_boundingRect.width() * sceneView->scale(), m_boundingRect.height());
    return m_boundingRect;
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    if (m_levels.empty() || m_levels.front().empty())
    {
        return;
    }

    const auto sceneView = view();
    const auto visibleSceneRect = sceneView->visibleSceneRect(); // Current visible scene rect
    const auto currentScale = sceneView->scale(); // Current GraphicsView scale
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


    // Search for first visible top-level item
    auto& level0 = m_levels[0];
    auto first = ::std::lower_bound(level0.begin(), level0.end(), sceneLeft, [](const ::profiler_gui::ProfBlockItem& _item, qreal _value)
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
    if (!m_bTest && m_pRoot->thread_id == ::profiler_gui::EASY_GLOBALS.selected_thread && m_pRoot->thread_id != 0)
    {
        brush.setColor(::profiler_gui::SELECTED_THREAD_BACKGROUND);
    }
    else
    {
        brush.setColor(m_backgroundColor);
    }

    _painter->setBrush(brush);
    _painter->setPen(Qt::NoPen);
    rect.setRect(0, m_boundingRect.top() - (ROW_SPACING >> 1), visibleSceneRect.width(), m_boundingRect.height() + ROW_SPACING);
    _painter->drawRect(rect);
    // END. Draw background. ~~~~~~~~~~~~~~~~~~~~


    // This is to make _painter->drawText() work properly
    // (it seems there is a bug in Qt5.6 when drawText called for big coordinates,
    // drawRect at the same time called for actually same coordinates
    // works fine without using this additional shifting)
    auto dx = level0[m_levelsIndexes[0]].left() * currentScale;

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

            if (item.right() < sceneLeft || state == -1 || (l == 0 && (item.top() > visibleSceneRect.bottom() || (item.top() + item.totalHeight) < visibleSceneRect.top())))
            {
                // This item is not visible
                ++m_levelsIndexes[l];
                continue;
            }

            auto w = ::std::max(item.width() * currentScale, 1.0);
            if (w < 20)
            {
                // Items which width is less than 20 will be painted as big rectangles which are hiding it's children
                if (item.left() > sceneRight)
                {
                    // This is first totally invisible item. No need to check other items.
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
                    // Set background color brush for rectangle
                    previousColor = item.color;
                    brush.setColor(previousColor);
                    _painter->setBrush(brush);
                }

                if (w < 3)
                {
                    // Do not paint borders for very narrow items
                    if (previousPenStyle != Qt::NoPen)
                    {
                        previousPenStyle = Qt::NoPen;
                        _painter->setPen(Qt::NoPen);
                    }
                }
                else if (previousPenStyle != Qt::SolidLine)
                {
                    // Restore pen for item which is wide enough to paint borders
                    previousPenStyle = Qt::SolidLine;
                    _painter->setPen(BORDERS_COLOR);
                }

                // Draw rectangle
                rect.setRect(x, item.top(), w, item.totalHeight);
                _painter->drawRect(rect);

                if (next_level < levelsNumber && item.children_begin != -1)
                {
                    // Mark that we would not paint children of current item
                    m_levels[next_level][item.children_begin].state = -1;
                }

                continue;
            }

            if (next_level < levelsNumber && item.children_begin != -1)
            {
                if (m_levelsIndexes[next_level] == -1)
                {
                    // Mark first potentially visible child item on next sublevel
                    m_levelsIndexes[next_level] = item.children_begin;
                }

                // Mark children items that we want to draw them
                m_levels[next_level][item.children_begin].state = 1;
            }

            if (item.left() > sceneRight)
            {
                // This is first totally invisible item. No need to check other items.
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
                // Set background color brush for rectangle
                previousColor = item.color;
                brush.setColor(previousColor);
                _painter->setBrush(brush);
            }

            // Draw text-----------------------------------
            // calculating text coordinates
            const auto x = item.left() * currentScale - dx;
            rect.setRect(x, item.top(), w, item.height());
            _painter->drawRect(rect);

            auto xtext = x;
            if (item.left() < sceneLeft)
            {
                // if item left border is out of screen then attach text to the left border of the screen
                // to ensure text is always visible for items presenting on the screen.
                w += (item.left() - sceneLeft) * currentScale;
                xtext = sceneLeft * currentScale - dx;
            }

            rect.setRect(xtext + 1, item.top(), w - 1, item.height());

            // text will be painted with inverse color
            auto textColor = 0x00ffffff - previousColor;
            if (textColor == previousColor) textColor = 0;
            _painter->setPen(textColor);

            // drawing text
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

            // restore previous pen color
            if (previousPenStyle == Qt::NoPen)
            {
                _painter->setPen(Qt::NoPen);
            }
            else
            {
                _painter->setPen(BORDERS_COLOR); // restore pen for rectangle painting
            }
            // END Draw text~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        }
    }

    _painter->restore();
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsItem::getBlocks(qreal _left, qreal _right, ::profiler_gui::TreeBlocks& _blocks) const
{
    //if (m_bTest)
    //{
    //    return;
    //}

    // Search for first visible top-level item
    auto& level0 = m_levels[0];
    auto first = ::std::lower_bound(level0.begin(), level0.end(), _left, [](const ::profiler_gui::ProfBlockItem& _item, qreal _value)
    {
        return _item.left() < _value;
    });

    size_t itemIndex = 0;
    if (first != level0.end())
    {
        itemIndex = first - level0.begin();
        if (itemIndex > 0)
            itemIndex -= 1;
    }
    else
    {
        itemIndex = level0.size() - 1;
    }

    // Add all visible top-level items into array of visible blocks
    for (size_t i = itemIndex, end = level0.size(); i < end; ++i)
    {
        const auto& item = level0[i];

        if (item.left() > _right)
        {
            // First invisible item. No need to check other items.
            break;
        }

        if (item.right() < _left)
        {
            // This item is not visible yet
            // This is just to be sure
            continue;
        }

        _blocks.emplace_back(m_pRoot, item.block);
    }
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsItem::setBackgroundColor(QRgb _color)
{
    m_backgroundColor = _color;
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsItem::setBoundingRect(qreal x, qreal y, qreal w, qreal h)
{
    m_boundingRect.setRect(x, y, w, h);
}

void ProfGraphicsItem::setBoundingRect(const QRectF& _rect)
{
    m_boundingRect = _rect;
}

//////////////////////////////////////////////////////////////////////////

::profiler::thread_id_t ProfGraphicsItem::threadId() const
{
    return m_pRoot->thread_id;
}

//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////

const ProfGraphicsItem::Children& ProfGraphicsItem::items(unsigned short _level) const
{
    return m_levels[_level];
}

const ::profiler_gui::ProfBlockItem& ProfGraphicsItem::getItem(unsigned short _level, size_t _index) const
{
    return m_levels[_level][_index];
}

::profiler_gui::ProfBlockItem& ProfGraphicsItem::getItem(unsigned short _level, size_t _index)
{
    return m_levels[_level][_index];
}

size_t ProfGraphicsItem::addItem(unsigned short _level)
{
    m_levels[_level].emplace_back();
    return m_levels[_level].size() - 1;
}

size_t ProfGraphicsItem::addItem(unsigned short _level, const ::profiler_gui::ProfBlockItem& _item)
{
    m_levels[_level].emplace_back(_item);
    return m_levels[_level].size() - 1;
}

size_t ProfGraphicsItem::addItem(unsigned short _level, ::profiler_gui::ProfBlockItem&& _item)
{
    m_levels[_level].emplace_back(::std::forward<::profiler_gui::ProfBlockItem&&>(_item));
    return m_levels[_level].size() - 1;
}

//////////////////////////////////////////////////////////////////////////

ProfChronometerItem::ProfChronometerItem() : QGraphicsItem(), m_left(0), m_right(0), m_font(QFont("CourierNew", 16, 2))
{
    setZValue(10);
}

ProfChronometerItem::~ProfChronometerItem()
{
}

QRectF ProfChronometerItem::boundingRect() const
{
    return m_boundingRect;
}

void ProfChronometerItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget)
{
    const auto sceneView = view();
    const auto currentScale = sceneView->scale();
    const auto offset = sceneView->offset();
    const auto visibleSceneRect = sceneView->visibleSceneRect();
    auto sceneLeft = offset, sceneRight = offset + visibleSceneRect.width() / currentScale;

    if (m_left > sceneRight || m_right < sceneLeft)
    {
        // This item is out of screen
        return;
    }

    const auto selectedInterval = width();
    QRectF rect((m_left - offset) * currentScale, visibleSceneRect.top(), ::std::max(selectedInterval * currentScale, 1.0), visibleSceneRect.height());

    QString text; // Displayed text
    if (selectedInterval < 1.0) // interval in nanoseconds
        text = ::std::move(QString("%1 ns").arg(selectedInterval * 1e3, 0, 'f', 1));
    else if (selectedInterval < 1e3) // interval in microseconds
        text = ::std::move(QString("%1 us").arg(selectedInterval, 0, 'f', 1));
    else if (selectedInterval < 1e6) // interval in milliseconds
        text = ::std::move(QString("%1 ms").arg(selectedInterval * 1e-3, 0, 'f', 1));
    else // interval in seconds
        text = ::std::move(QString("%1 sec").arg(selectedInterval * 1e-6, 0, 'f', 1));

    const auto textRect = QFontMetrics(m_font).boundingRect(text); // Calculate displayed text boundingRect

    // Paint!--------------------------
    _painter->save();

    // instead of scrollbar we're using manual offset
    _painter->setTransform(QTransform::fromTranslate(-x(), -y()), true);

    // draw transparent rectangle
    _painter->setBrush(CHRONOMETER_COLOR);
    _painter->setPen(Qt::NoPen);
    _painter->drawRect(rect);

    // draw text
    _painter->setPen(CHRONOMETER_TEXT_COLOR);
    _painter->setFont(m_font);

    if (m_left < sceneLeft)
    {
        rect.setLeft(0);
    }

    if (m_right > sceneRight)
    {
        rect.setWidth((sceneRight - offset) * currentScale - rect.left());
    }

    if (textRect.width() < rect.width())
    {
        // Text will be drawed inside rectangle
        _painter->drawText(rect, Qt::AlignCenter, text);
        _painter->restore();
        return;
    }

    sceneLeft -= offset;
    sceneRight -= offset;
    if (rect.right() + textRect.width() < sceneRight)
    {
        // Text will be drawed to the right of rectangle
        _painter->drawText(QPointF(rect.right(), rect.top() + rect.height() * 0.5 + textRect.height() * 0.33), text);
    }
    else if (rect.left() - textRect.width() > sceneLeft)
    {
        // Text will be drawed to the left of rectangle
        _painter->drawText(QPointF(rect.left() - textRect.width(), rect.top() + rect.height() * 0.5 + textRect.height() * 0.33), text);
    }
    else
    {
        // Text will be drawed inside rectangle
        _painter->drawText(rect, Qt::AlignCenter | Qt::TextDontClip, text);
    }

    _painter->restore();
    // END Paint!~~~~~~~~~~~~~~~~~~~~~~
}

void ProfChronometerItem::setBoundingRect(qreal x, qreal y, qreal w, qreal h)
{
    m_boundingRect.setRect(x, y, w, h);
}

void ProfChronometerItem::setBoundingRect(const QRectF& _rect)
{
    m_boundingRect = _rect;
}

void ProfChronometerItem::setLeftRight(qreal _left, qreal _right)
{
    if (_left < _right)
    {
        m_left = _left;
        m_right = _right;
    }
    else
    {
        m_left = _right;
        m_right = _left;
    }
}

const ProfGraphicsView* ProfChronometerItem::view() const
{
    return static_cast<const ProfGraphicsView*>(scene()->parent());
}

//////////////////////////////////////////////////////////////////////////

ProfGraphicsView::ProfGraphicsView(bool _test)
    : QGraphicsView()
    , m_beginTime(-1)
    , m_scale(1)
    , m_offset(0)
    , m_mouseButtons(Qt::NoButton)
    , m_pScrollbar(nullptr)
    , m_chronometerItem(nullptr)
    , m_flickerSpeed(0)
    , m_bUpdatingRect(false)
    , m_bTest(_test)
    , m_bEmpty(true)
    , m_bStrictSelection(false)
{
    initMode();
    setScene(new QGraphicsScene(this));

    if (_test)
    {
        test(18000, 40000000, 2);
    }

    updateVisibleSceneRect();
}

ProfGraphicsView::ProfGraphicsView(const ::profiler::thread_blocks_tree_t& _blocksTree)
    : QGraphicsView()
    , m_beginTime(-1)
    , m_scale(1)
    , m_offset(0)
    , m_mouseButtons(Qt::NoButton)
    , m_pScrollbar(nullptr)
    , m_chronometerItem(nullptr)
    , m_flickerSpeed(0)
    , m_bUpdatingRect(false)
    , m_bTest(false)
    , m_bEmpty(true)
    , m_bStrictSelection(false)
{
    initMode();
    setScene(new QGraphicsScene(this));
    setTree(_blocksTree);
    updateVisibleSceneRect();
}

ProfGraphicsView::~ProfGraphicsView()
{
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::fillTestChildren(ProfGraphicsItem* _item, const int _maxlevel, int _level, qreal _x, qreal _y, size_t _childrenNumber, size_t& _total_items)
{
    size_t nchildren = _childrenNumber;
    _childrenNumber = TEST_PROGRESSION_BASE;

    for (size_t i = 0; i < nchildren; ++i)
    {
        size_t j = _item->addItem(_level);
        auto& b = _item->getItem(_level, j);
        b.color = ::profiler_gui::toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);
        b.state = 0;

        if (_level < _maxlevel)
        {
            const auto& children = _item->items(_level + 1);
            b.children_begin = static_cast<unsigned int>(children.size());

            fillTestChildren(_item, _maxlevel, _level + 1, _x, _y + GRAPHICS_ROW_SIZE_FULL, _childrenNumber, _total_items);

            const auto& last = children.back();
            b.setRect(_x, _y, last.right() - _x, GRAPHICS_ROW_SIZE);
            b.totalHeight = GRAPHICS_ROW_SIZE_FULL + last.totalHeight;
        }
        else
        {
            b.setRect(_x, _y, to_microseconds(10 + rand() % 190), GRAPHICS_ROW_SIZE);
            b.totalHeight = GRAPHICS_ROW_SIZE;
            b.children_begin = -1;
        }

        _x = b.right();
        ++_total_items;
    }
}

void ProfGraphicsView::test(size_t _frames_number, size_t _total_items_number_estimate, int _rows)
{
    static const qreal X_BEGIN = 50;
    static const qreal Y_BEGIN = 0;

    clearSilent(); // Clear scene

    // Calculate items number for first level
    _rows = ::std::max(1, _rows);
    const size_t children_per_frame = static_cast<size_t>(0.5 + static_cast<double>(_total_items_number_estimate) / static_cast<double>(_rows * _frames_number));
    const int max_depth = logn<TEST_PROGRESSION_BASE>(children_per_frame * (TEST_PROGRESSION_BASE - 1) * 0.5 + 1);
    const size_t first_level_children_count = children_per_frame * (1.0 - TEST_PROGRESSION_BASE) / (1.0 - pow(TEST_PROGRESSION_BASE, max_depth)) + 0.5;

    ::std::vector<ProfGraphicsItem*> thread_items(_rows);
    for (int i = 0; i < _rows; ++i)
    {
        auto item = new ProfGraphicsItem(true);
        thread_items[i] = item;

        item->setPos(0, Y_BEGIN + i * (max_depth * GRAPHICS_ROW_SIZE_FULL + ROW_SPACING * 5));

        item->setLevels(max_depth + 1);
        item->reserve(0, _frames_number);
    }

    // Calculate items number for each sublevel
    size_t chldrn = first_level_children_count;
    for (int i = 1; i <= max_depth; ++i)
    {
        for (int i = 0; i < _rows; ++i)
        {
            auto item = thread_items[i];
            item->reserve(i, chldrn * _frames_number);
        }

        chldrn *= TEST_PROGRESSION_BASE;
    }

    // Create required number of items
    size_t total_items = 0;
    qreal maxX = 0;
    const ProfGraphicsItem* longestItem = nullptr;
    for (int i = 0; i < _rows; ++i)
    {
        auto item = thread_items[i];
        qreal x = X_BEGIN, y = item->y();
        for (unsigned int i = 0; i < _frames_number; ++i)
        {
            size_t j = item->addItem(0);
            auto& b = item->getItem(0, j);
            b.color = ::profiler_gui::toRgb(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225);
            b.state = 0;

            const auto& children = item->items(1);
            b.children_begin = static_cast<unsigned int>(children.size());

            fillTestChildren(item, max_depth, 1, x, y + GRAPHICS_ROW_SIZE_FULL, first_level_children_count, total_items);

            const auto& last = children.back();
            b.setRect(x, y, last.right() - x, GRAPHICS_ROW_SIZE);
            b.totalHeight = GRAPHICS_ROW_SIZE_FULL + last.totalHeight;

            x += b.width() * 1.2;

            ++total_items;
        }

        const auto h = item->getItem(0, 0).totalHeight;
        item->setBoundingRect(0, 0, x, h);
        if (_rows > 1)
        {
            item->setBackgroundColor(GetBackgroundColor());
        }

        m_items.push_back(item);
        scene()->addItem(item);

        if (maxX < x)
        {
            maxX = x;
            longestItem = item;
        }
    }

    printf("TOTAL ITEMS = %llu\n", total_items);

    // Calculate scene rect
    auto item = thread_items.back();
    scene()->setSceneRect(0, 0, maxX, item->y() + item->getItem(0, 0).totalHeight);

    // Reset necessary values
    m_offset = 0;
    updateVisibleSceneRect();
    setScrollbar(m_pScrollbar);
    m_pScrollbar->setMinimapFrom(0, longestItem->items(0));
    ::profiler_gui::EASY_GLOBALS.selected_thread = 0;
    emit ::profiler_gui::EASY_GLOBALS.events.selectedThreadChanged(0);

    // Create new chronometer item (previous item was destroyed by scene on scene()->clear()).
    // It will be shown on mouse right button click.
    m_chronometerItem = new ProfChronometerItem();
    m_chronometerItem->setBoundingRect(scene()->sceneRect());
    m_chronometerItem->hide();
    scene()->addItem(m_chronometerItem);

    // Set necessary flags
    m_bTest = true;
    m_bEmpty = false;

    scaleTo(BASE_SCALE);
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::clearSilent()
{
    const QSignalBlocker blocker(this), sceneBlocker(scene()); // block all scene signals (otherwise clear() would be extremely slow!)

    GetBackgroundColor = BG1; // reset background color

    // Stop flicking
    m_flickerTimer.stop();
    m_flickerSpeed = 0;

    // Clear all items
    scene()->clear();
    m_items.clear();
    m_selectedBlocks.clear();

    m_beginTime = -1; // reset begin time
    m_scale = 1; // scale back to initial 100% scale
    m_offset = 0; // scroll back to the beginning of the scene

    // Reset necessary flags
    m_bTest = false;
    m_bEmpty = true;

    // notify ProfTreeWidget that selection was reset
    emit intervalChanged(m_selectedBlocks, m_beginTime, 0, 0, false);
}

void ProfGraphicsView::setTree(const ::profiler::thread_blocks_tree_t& _blocksTree)
{
    // clear scene
    clearSilent();

    // set new blocks tree
    // calculate scene size and fill it with items

    // Calculating start and end time
    ::profiler::timestamp_t finish = 0;
    const ::profiler::BlocksTree* longestTree = nullptr;
    const ProfGraphicsItem* longestItem = nullptr;
    for (const auto& threadTree : _blocksTree)
    {
        const auto& tree = threadTree.second.tree;
        const auto timestart = tree.children.front().node->block()->getBegin();
        const auto timefinish = tree.children.back().node->block()->getEnd();

        if (m_beginTime > timestart)
        {
            m_beginTime = timestart;
        }

        if (finish < timefinish)
        {
            finish = timefinish;
            longestTree = &tree;
        }
    }

    // Filling scene with items
    m_items.reserve(_blocksTree.size());
    qreal y = 0;
    for (const auto& threadTree : _blocksTree)
    {
        // fill scene with new items
        const auto& tree = threadTree.second.tree;
        qreal h = 0, x = time2position(tree.children.front().node->block()->getBegin());
        auto item = new ProfGraphicsItem(&threadTree.second);
        item->setLevels(tree.depth);
        item->setPos(0, y);

        const auto children_duration = setTree(item, tree.children, h, y, 0);

        item->setBoundingRect(0, 0, children_duration + x, h);
        item->setBackgroundColor(GetBackgroundColor());
        m_items.push_back(item);
        scene()->addItem(item);

        y += h + ROW_SPACING;

        if (longestTree == &tree)
        {
            longestItem = item;
        }
    }

    // Calculating scene rect
    const qreal endX = time2position(finish) + 1.0;
    scene()->setSceneRect(0, 0, endX, y);

    // Stub! :O
    for (auto item : m_items)
    {
        item->setBoundingRect(0, 0, endX, item->boundingRect().height());
    }

    // Center view on the beginning of the scene
    updateVisibleSceneRect();
    setScrollbar(m_pScrollbar);
    m_pScrollbar->setMinimapFrom(longestItem->threadId(), longestItem->items(0));
    ::profiler_gui::EASY_GLOBALS.selected_thread = longestItem->threadId();
    emit ::profiler_gui::EASY_GLOBALS.events.selectedThreadChanged(longestItem->threadId());

    // Create new chronometer item (previous item was destroyed by scene on scene()->clear()).
    // It will be shown on mouse right button click.
    m_chronometerItem = new ProfChronometerItem();
    m_chronometerItem->setBoundingRect(scene()->sceneRect());
    m_chronometerItem->hide();
    scene()->addItem(m_chronometerItem);

    // Setting flags
    m_bTest = false;
    m_bEmpty = false;

    scaleTo(BASE_SCALE);
}

qreal ProfGraphicsView::setTree(ProfGraphicsItem* _item, const ::profiler::BlocksTree::children_t& _children, qreal& _height, qreal _y, unsigned short _level)
{
    static const qreal MIN_DURATION = 0.25;

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

        if (duration < MIN_DURATION)
        {
            duration = MIN_DURATION;
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
        b.color = ::profiler_gui::toRgb(::profiler::colors::get_red(color), ::profiler::colors::get_green(color), ::profiler::colors::get_blue(color));
        b.setRect(xbegin, _y, duration, GRAPHICS_ROW_SIZE);
        b.totalHeight = GRAPHICS_ROW_SIZE + h;

        prev_end = xbegin + duration;
        total_duration = prev_end - start_time;
    }

    _height += GRAPHICS_ROW_SIZE_FULL + maxh;

    return total_duration;
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::setScrollbar(ProfGraphicsScrollbar* _scrollbar)
{
    if (m_pScrollbar)
    {
        disconnect(m_pScrollbar, &ProfGraphicsScrollbar::valueChanged, this, &This::onGraphicsScrollbarValueChange);
    }

    m_pScrollbar = _scrollbar;
    m_pScrollbar->setMinimapFrom(0, nullptr);
    m_pScrollbar->hideChrono();
    m_pScrollbar->setRange(0, scene()->width());
    m_pScrollbar->setSliderWidth(m_visibleSceneRect.width());
    m_pScrollbar->setValue(0);
    connect(m_pScrollbar, &ProfGraphicsScrollbar::valueChanged, this, &This::onGraphicsScrollbarValueChange);

    ::profiler_gui::EASY_GLOBALS.selected_thread = 0;
    emit ::profiler_gui::EASY_GLOBALS.events.selectedThreadChanged(0);
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::updateVisibleSceneRect()
{
    m_visibleSceneRect = mapToScene(rect()).boundingRect();
}

void ProfGraphicsView::updateScene()
{
    scene()->update(m_visibleSceneRect);
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::scaleTo(qreal _scale)
{
    if (m_bEmpty)
    {
        return;
    }

    // have to limit scale because of Qt's QPainter feature: it doesn't draw text
    // with very big coordinates (but it draw rectangles with the same coordinates good).
    m_scale = clamp(MIN_SCALE, _scale, MAX_SCALE); 
    updateVisibleSceneRect();

    // Update slider width for scrollbar
    const auto windowWidth = m_visibleSceneRect.width() / m_scale;
    m_pScrollbar->setSliderWidth(windowWidth);

    updateScene();
}

void ProfGraphicsView::wheelEvent(QWheelEvent* _event)
{
    if (m_bEmpty)
    {
        _event->accept();
        return;
    }

    const decltype(m_scale) scaleCoeff = _event->delta() > 0 ? SCALING_COEFFICIENT : SCALING_COEFFICIENT_INV;

    // Remember current mouse position
    const auto mouseX = mapToScene(_event->pos()).x();
    const auto mousePosition = m_offset + mouseX / m_scale;

    // have to limit scale because of Qt's QPainter feature: it doesn't draw text
    // with very big coordinates (but it draw rectangles with the same coordinates good).
    m_scale = clamp(MIN_SCALE, m_scale * scaleCoeff, MAX_SCALE);

    updateVisibleSceneRect(); // Update scene rect

    // Update slider width for scrollbar
    const auto windowWidth = m_visibleSceneRect.width() / m_scale;
    m_pScrollbar->setSliderWidth(windowWidth);

    // Calculate new offset to simulate QGraphicsView::AnchorUnderMouse scaling behavior
    m_offset = clamp(0., mousePosition - mouseX / m_scale, scene()->width() - windowWidth);

    // Update slider position
    m_bUpdatingRect = true; // To be sure that updateVisibleSceneRect will not be called by scrollbar change
    m_pScrollbar->setValue(m_offset);
    m_bUpdatingRect = false;

    updateScene(); // repaint scene
    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::mousePressEvent(QMouseEvent* _event)
{
    if (m_bEmpty)
    {
        _event->accept();
        return;
    }

    m_mouseButtons = _event->buttons();

    if (m_mouseButtons & Qt::LeftButton)
    {
        m_mousePressPos = _event->globalPos();
    }

    if (m_mouseButtons & Qt::RightButton)
    {
        m_bStrictSelection = false;
        const auto mouseX = m_offset + mapToScene(_event->pos()).x() / m_scale;
        m_chronometerItem->setLeftRight(mouseX, mouseX);
        m_chronometerItem->hide();
        m_pScrollbar->hideChrono();
    }

    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::mouseReleaseEvent(QMouseEvent* _event)
{
    if (m_bEmpty)
    {
        _event->accept();
        return;
    }

    bool changedSelection = false;
    if (m_mouseButtons & Qt::RightButton)
    {
        if (m_chronometerItem->isVisible() && m_chronometerItem->width() < 1e-6)
        {
            m_chronometerItem->hide();
            m_pScrollbar->hideChrono();
        }

        if (!m_selectedBlocks.empty())
        {
            changedSelection = true;
            m_selectedBlocks.clear();
        }

        if (!m_bTest && m_chronometerItem->isVisible())
        {
            //printf("INTERVAL: {%lf, %lf} ms\n", m_chronometerItem->left(), m_chronometerItem->right());

            for (auto item : m_items)
            {
                item->getBlocks(m_chronometerItem->left(), m_chronometerItem->right(), m_selectedBlocks);
            }

            if (!m_selectedBlocks.empty())
            {
                changedSelection = true;
            }
        }
    }

    m_mouseButtons = _event->buttons();
    _event->accept();

    if (changedSelection)
    {
        emit intervalChanged(m_selectedBlocks, m_beginTime, position2time(m_chronometerItem->left()), position2time(m_chronometerItem->right()), m_bStrictSelection);
    }

    m_bStrictSelection = false;
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::mouseMoveEvent(QMouseEvent* _event)
{
    if (m_bEmpty)
    {
        _event->accept();
        return;
    }

    bool needUpdate = false;

    if (m_mouseButtons & Qt::RightButton)
    {
        const auto mouseX = m_offset + mapToScene(_event->pos()).x() / m_scale;
        if (m_bStrictSelection)
        {
            if (mouseX > m_chronometerItem->right())
            {
                m_bStrictSelection = false;
                m_chronometerItem->setLeftRight(m_chronometerItem->right(), mouseX);
            }
            else
            {
                m_chronometerItem->setLeftRight(mouseX, m_chronometerItem->right());
            }
        }
        else
        {
            if (mouseX < m_chronometerItem->left())
            {
                m_bStrictSelection = true;
                m_chronometerItem->setLeftRight(mouseX, m_chronometerItem->left());
            }
            else
            {
                m_chronometerItem->setLeftRight(m_chronometerItem->left(), mouseX);
            }
        }

        m_pScrollbar->setChronoPos(m_chronometerItem->left(), m_chronometerItem->right());

        if (!m_chronometerItem->isVisible() && m_chronometerItem->width() > 1e-6)
        {
            m_chronometerItem->show();
            m_pScrollbar->showChrono();
        }

        needUpdate = true;
    }

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

        needUpdate = true;
    }

    if (needUpdate)
    {
        updateScene(); // repaint scene
    }

    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::resizeEvent(QResizeEvent* _event)
{
    updateVisibleSceneRect(); // Update scene visible rect only once
    updateScene(); // repaint scene
}

//////////////////////////////////////////////////////////////////////////

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
    connect(&::profiler_gui::EASY_GLOBALS.events, &::profiler_gui::ProfGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChange);
}

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::onScrollbarValueChange(int)
{
    if (!m_bUpdatingRect && !m_bEmpty)
        updateVisibleSceneRect();
}

void ProfGraphicsView::onGraphicsScrollbarValueChange(qreal _value)
{
    if (!m_bEmpty)
    {
        m_offset = _value;
        if (!m_bUpdatingRect)
        {
            updateVisibleSceneRect();
            updateScene();
        }
    }
}

//////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////

void ProfGraphicsView::onSelectedThreadChange(::profiler::thread_id_t _id)
{
    if (m_pScrollbar == nullptr || m_pScrollbar->minimapThread() == _id || m_bTest)
    {
        return;
    }

    if (_id == 0)
    {
        m_pScrollbar->setMinimapFrom(0, nullptr);
        return;
    }

    for (auto item : m_items)
    {
        if (item->threadId() == _id)
        {
            m_pScrollbar->setMinimapFrom(_id, item->items(0));
            updateScene();
            return;
        }
    }

    m_pScrollbar->setMinimapFrom(0, nullptr);
    updateScene();
}

//////////////////////////////////////////////////////////////////////////

ProfGraphicsViewWidget::ProfGraphicsViewWidget(bool _test)
    : QWidget(nullptr)
    , m_scrollbar(new ProfGraphicsScrollbar(nullptr))
    , m_view(new ProfGraphicsView(_test))
{
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(1, 0, 1, 0);
    lay->addWidget(m_view);
    lay->setSpacing(1);
    lay->addWidget(m_scrollbar);
    setLayout(lay);
    m_view->setScrollbar(m_scrollbar);
}

ProfGraphicsViewWidget::ProfGraphicsViewWidget(const ::profiler::thread_blocks_tree_t& _blocksTree)
    : QWidget(nullptr)
    , m_scrollbar(new ProfGraphicsScrollbar(nullptr))
    , m_view(new ProfGraphicsView(_blocksTree))
{
    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(1, 0, 1, 0);
    lay->addWidget(m_view);
    lay->setSpacing(1);
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
