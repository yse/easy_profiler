

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPolygonItem>
#include <QGraphicsSimpleTextItem>
#include <QWheelEvent>
#include "blocks_graphics_view.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const GlobalSignals GLOBALS;

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MyPolygon::MyPolygon(QGraphicsItem* _parent) : QGraphicsPolygonItem(_parent)
{
}

MyPolygon::~MyPolygon()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MyText::MyText(const char* _text, QGraphicsItem* _parent) : QGraphicsSimpleTextItem(_text, _parent), QObject()
{
    connect(&GLOBALS, &GlobalSignals::scaleIncreased, this, &MyText::onScaleIncrease);
    connect(&GLOBALS, &GlobalSignals::scaleDecreased, this, &MyText::onScaleDecrease);
}

MyText::~MyText()
{
}

void MyText::onScaleIncrease(qreal _scale)
{
    setScale(100.0 / _scale);

    if (!isVisible())
    {
        if (boundingRect().width() * 100.0 < parentItem()->boundingRect().width() * _scale)
        {
            show();
        }
    }
}

void MyText::onScaleDecrease(qreal _scale)
{
    setScale(100.0 / _scale);

    if (isVisible())
    {
        if (boundingRect().width() * 100.0 > parentItem()->boundingRect().width() * _scale)
        {
            hide();
        }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

const qreal LEFT = -2500000;
const qreal WIDTH = 5000000;
const int N_ITEMS = 200000;

MyGraphicsScene::MyGraphicsScene(QGraphicsView* _parent) : QGraphicsScene(_parent), m_start(0)
{
    setSceneRect(QRectF(LEFT, -200, WIDTH, 220));
    test();
}

MyGraphicsScene::MyGraphicsScene(const thread_blocks_tree_t& _blocksTree, QGraphicsView* _parent) : QGraphicsScene(_parent), m_start(0)
{
    setTree(_blocksTree);
}

MyGraphicsScene::~MyGraphicsScene()
{
}

void MyGraphicsScene::test()
{
    QPolygonF poly(QRectF(-5, -180, 10, 180));
    for (int i = 0; i < N_ITEMS; ++i)
    {
        MyPolygon* item = new MyPolygon();
        int h = 50 + rand() % 131;
        item->setPolygon(QRectF(-5, -h, 10, h));
        item->setPos(LEFT + i * 10, 0);
        item->setBrush(QBrush(QColor(30 + rand() % 225, 30 + rand() % 225, 30 + rand() % 225)));
        addItem(item);
    }
}

void MyGraphicsScene::setTree(const thread_blocks_tree_t& _blocksTree)
{
    m_start = -1;
    profiler::timestamp_t finish = 0;
    for (const auto& threadTree : _blocksTree)
    {
        ++items_couinter;
        const auto timestart = threadTree.second.children.front().node->block()->getBegin();
        const auto timefinish = threadTree.second.children.back().node->block()->getEnd();
        if (m_start > timestart) m_start = timestart;
        if (finish < timefinish) finish = timefinish;
    }

    const qreal endX = time2position(finish + 1000000);
    setSceneRect(QRectF(0, 0, endX, 110 * _blocksTree.size()));

    qreal y = 0;
    for (const auto& threadTree : _blocksTree)
    {
        setTree(threadTree.second.children, y);
        y += 110;
    }
}

void MyGraphicsScene::setTree(const BlocksTree::children_t& _children, qreal _y, int _level)
{
    for (const auto& child : _children)
    {
        ++items_couinter;
        MyPolygon* item = new MyPolygon();

        const qreal xbegin = time2position(child.node->block()->getBegin());
        const qreal height = 100 - _level * 5;
        qreal duration = time2position(child.node->block()->getEnd()) - xbegin;
        if (duration < 1)
            duration = 1;

        item->setPolygon(QRectF(0, _level * 5, duration, height));
        item->setPos(xbegin, _y);
        item->setZValue(_level);

        const auto color = child.node->block()->getColor();
        item->setBrush(QBrush(QColor(profiler::colors::get_red(color), profiler::colors::get_green(color), profiler::colors::get_blue(color))));
        item->setPen(QPen(Qt::NoPen)); // disable borders painting

        addItem(item);
        MyText* text = new MyText(child.node->getBlockName(), item);
        QRectF textRect = text->boundingRect();
        text->setPos(0, _level * 5);

        QRectF itemRect = item->boundingRect();
        if (textRect.width() > itemRect.width())
            text->hide();

        setTree(child.children, _y, _level + 1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

MyGraphicsView::MyGraphicsView() : QGraphicsView(), m_scale(100), m_scaleCoeff(1.25)
{
    initMode();
    setScene(new MyGraphicsScene(this));
    centerOn(0, 0);
}

MyGraphicsView::MyGraphicsView(const thread_blocks_tree_t& _blocksTree) : QGraphicsView(), m_scale(100), m_scaleCoeff(1.25)
{
    initMode();
    setScene(new MyGraphicsScene(_blocksTree, this));
    centerOn(0, 0);
}

MyGraphicsView::~MyGraphicsView()
{
}

void MyGraphicsView::initMode()
{
    setCacheMode(QGraphicsView::CacheBackground);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
}

void MyGraphicsView::wheelEvent(QWheelEvent* _event)
{
    //MyGraphicsScene* myscene = (MyGraphicsScene*)scene();
    if (_event->delta() > 0)
    {
        scale(m_scaleCoeff, m_scaleCoeff);
        m_scale *= m_scaleCoeff;
        emit GLOBALS.scaleIncreased(m_scale);
    }
    else
    {
        const qreal scaleCoeff = 1.0 / m_scaleCoeff;
        scale(scaleCoeff, scaleCoeff);
        m_scale *= scaleCoeff;
        emit GLOBALS.scaleDecreased(m_scale);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
