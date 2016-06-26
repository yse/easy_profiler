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
* change log        : * 2016/06/26 Victor Zarubkin: moved sources from graphics_view.h
*                   :       and renamed classes from My* to Prof*.
*                   : *
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#include <QWheelEvent>
#include <QMouseEvent>
#include <QSignalBlocker>
#include <QScrollBar>
#include "blocks_graphics_view.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProfGraphicsPolygonItem::ProfGraphicsPolygonItem(bool _drawBorders, QGraphicsItem* _parent)
    : QGraphicsPolygonItem(_parent)
    , m_bDrawBorders(_drawBorders)
{
    if (!_drawBorders)
    {
        setPen(QPen(Qt::NoPen));
    }
}

ProfGraphicsPolygonItem::~ProfGraphicsPolygonItem()
{
}

void ProfGraphicsPolygonItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    if (m_bDrawBorders)
    {
        const auto currentScale = static_cast<ProfGraphicsView*>(scene()->parent())->currentScale();
        if (boundingRect().width() * currentScale < 5)
        {
            auto linePen = pen();
            if (linePen.style() != Qt::NoPen)
            {
                linePen.setStyle(Qt::NoPen);
                setPen(linePen);
            }
        }
        else
        {
            auto linePen = pen();
            linePen.setWidthF(1. / currentScale);
            setPen(linePen);
        }
    }

    QGraphicsPolygonItem::paint(painter, option, widget);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProfGraphicsTextItem::ProfGraphicsTextItem(const char* _text, QGraphicsItem* _parent)
    : QGraphicsSimpleTextItem(_text, _parent)
{
}

ProfGraphicsTextItem::~ProfGraphicsTextItem()
{
}

void ProfGraphicsTextItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
{
    const auto currentScale = static_cast<ProfGraphicsView*>(parentItem()->scene()->parent())->currentScale();
    if (boundingRect().width() < parentItem()->boundingRect().width() * currentScale)
    {
        painter->setTransform(QTransform::fromScale(1. / currentScale, 1), true);
        //setScale(1. / currentScale);
        QGraphicsSimpleTextItem::paint(painter, option, widget);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const qreal LEFT = -2500000;
const qreal WIDTH = 5000000;
const int N_ITEMS = 200000;

ProfGraphicsScene::ProfGraphicsScene(QGraphicsView* _parent, bool _test) : QGraphicsScene(_parent), m_start(-1)
{
    if (_test)
    {
        setSceneRect(QRectF(LEFT, -200, WIDTH, 220));
        test();
    }
}

ProfGraphicsScene::ProfGraphicsScene(const thread_blocks_tree_t& _blocksTree, QGraphicsView* _parent) : QGraphicsScene(_parent), m_start(-1)
{
    setTreeInternal(_blocksTree);
}

ProfGraphicsScene::~ProfGraphicsScene()
{
}

void ProfGraphicsScene::test()
{
    QPolygonF poly(QRectF(-5, -180, 10, 180));
    for (int i = 0; i < N_ITEMS; ++i)
    {
        ProfGraphicsPolygonItem* item = new ProfGraphicsPolygonItem(true);
        int h = 50 + rand() % 131;
        item->setPolygon(QRectF(-5, -h, 10, h));
        item->setPos(LEFT + i * 10, 0);
        item->setBrush(QBrush(QColor(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225)));
        addItem(item);
    }
}

void ProfGraphicsScene::clearSilent()
{
    const QSignalBlocker b(this); // block all scene signals (otherwise clear() would be extremely slow!)
    clear(); // clear would be VERY SLOW if signals would not be blocked!
}

void ProfGraphicsScene::setTree(const thread_blocks_tree_t& _blocksTree)
{
    // delete all items created for previous session
    clearSilent();

    // fill scene with new items
    m_start = -1;
    setTreeInternal(_blocksTree);
}

void ProfGraphicsScene::setTreeInternal(const thread_blocks_tree_t& _blocksTree)
{
    // calculate scene size
    profiler::timestamp_t finish = 0;
    for (const auto& threadTree : _blocksTree)
    {
        const auto timestart = threadTree.second.children.front().node->block()->getBegin();
        const auto timefinish = threadTree.second.children.back().node->block()->getEnd();
        if (m_start > timestart) m_start = timestart;
        if (finish < timefinish) finish = timefinish;
    }

    const qreal endX = time2position(finish + 1000000);
    setSceneRect(QRectF(0, 0, endX, 110 * _blocksTree.size()));

    // fill scene with items
    qreal y = 0;
    for (const auto& threadTree : _blocksTree)
    {
        setTreeInternal(threadTree.second.children, y);
        y += 110; // each thread is shifted to 110 points down
    }
}

void ProfGraphicsScene::setTreeInternal(const BlocksTree::children_t& _children, qreal _y, int _level)
{
    for (const auto& child : _children)
    {
        const qreal xbegin = time2position(child.node->block()->getBegin());
        const qreal height = 100 - _level * 5;
        qreal duration = time2position(child.node->block()->getEnd()) - xbegin;

        const bool drawBorders = duration > 1;
        if (!drawBorders)
        {
            duration = 1;
        }

        ProfGraphicsPolygonItem* item = new ProfGraphicsPolygonItem(drawBorders);
        item->setPolygon(QRectF(0, _level * 5, duration, height));
        item->setPos(xbegin, _y);
        item->setZValue(_level);

        const auto color = child.node->block()->getColor();
        item->setBrush(QBrush(QColor(profiler::colors::get_red(color), profiler::colors::get_green(color), profiler::colors::get_blue(color))));

        addItem(item);

        ProfGraphicsTextItem* text = new ProfGraphicsTextItem(child.node->getBlockName(), item);
        text->setPos(0, _level * 5);

        setTreeInternal(child.children, _y, _level + 1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ProfGraphicsView::ProfGraphicsView(bool _test) : QGraphicsView(), m_scale(1), m_scaleCoeff(1.25), m_mouseButtons(Qt::NoButton)
{
    initMode();
    setScene(new ProfGraphicsScene(this, _test));
    centerOn(0, 0);
}

ProfGraphicsView::ProfGraphicsView(const thread_blocks_tree_t& _blocksTree) : QGraphicsView(), m_scale(1), m_scaleCoeff(1.25), m_mouseButtons(Qt::NoButton)
{
    initMode();
    setScene(new ProfGraphicsScene(_blocksTree, this));
    centerOn(0, 0);
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

    scale(scaleCoeff, 1);// scaleCoeff);
    m_scale *= scaleCoeff;

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
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
    }

    //_event->accept();
    QGraphicsView::mouseMoveEvent(_event); // This is to be able to use setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
}

void ProfGraphicsView::initMode()
{
    setCacheMode(QGraphicsView::CacheBackground);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    //setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
}

void ProfGraphicsView::setTree(const thread_blocks_tree_t& _blocksTree)
{
    // block all signals for faster scene recreation
    const QSignalBlocker b(this);

    // scale back to initial 100% scale
    scale(1. / m_scale, 1);
    m_scale = 1;

    // set new blocks tree
    static_cast<ProfGraphicsScene*>(scene())->setTree(_blocksTree);

    // center view on the beginning of the scene
    centerOn(0, 0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
