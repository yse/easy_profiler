

#ifndef MY____GRAPHICS___VIEW_H
#define MY____GRAPHICS___VIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPolygonItem>
#include <QGraphicsSimpleTextItem>
#include <QWheelEvent>
#include <stdlib.h>
#include "profiler/reader.h"

class GlobalSignals : public QObject
{
    Q_OBJECT

public:

    GlobalSignals() : QObject() {}

signals:

    void scaleIncreased(qreal _scale) const;
    void scaleDecreased(qreal _scale) const;

} const GLOBALS;

class MyPolygon : public QGraphicsPolygonItem
{
public:

    MyPolygon(QGraphicsItem* _parent = nullptr) : QGraphicsPolygonItem(_parent)
    {
    }

    virtual ~MyPolygon()
    {
    }

};

class MyText : public QObject, public QGraphicsSimpleTextItem
{
    Q_OBJECT

public:

    MyText(const char* _text, QGraphicsItem* _parent = nullptr) : QGraphicsSimpleTextItem(_text, _parent), QObject()
    {
        connect(&GLOBALS, &GlobalSignals::scaleIncreased, this, &MyText::onScaleIncrease);
        connect(&GLOBALS, &GlobalSignals::scaleDecreased, this, &MyText::onScaleDecrease);
    }

private slots:

    void onScaleIncrease(qreal _scale)
    {
        setScale(100.0 / _scale);

        if (!isVisible())
        {
            QRectF selfRect = boundingRect();
            QRectF parentRect = parentItem()->boundingRect();
            if (selfRect.width() * 100.0 < parentRect.width() * _scale)
                show();
        }
    }

    void onScaleDecrease(qreal _scale)
    {
        setScale(100.0 / _scale);

        if (isVisible())
        {
            QRectF selfRect = boundingRect();
            QRectF parentRect = parentItem()->boundingRect();
            if (selfRect.width() * 100.0 > parentRect.width() * _scale)
                hide();
        }
    }



};

const qreal LEFT = -2500000;
const qreal WIDTH = 5000000;
const int N_ITEMS = 200000;

class MyGraphicsScene : public QGraphicsScene
{
    Q_OBJECT

private:

    profiler::timestamp_t m_start;

public:

    int items_couinter = 0;

    MyGraphicsScene(QGraphicsView* _parent) : QGraphicsScene(_parent), m_start(0)
    {
        setSceneRect(QRectF(LEFT, -200, WIDTH, 220));
        test();
    }

    MyGraphicsScene(const thread_blocks_tree_t& _blocksTree, QGraphicsView* _parent) : QGraphicsScene(_parent), m_start(0)
    {
        setTree(_blocksTree);
    }

    virtual ~MyGraphicsScene()
    {
    }

    void test()
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

    void setTree(const thread_blocks_tree_t& _blocksTree)
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

private:

    inline qreal time2position(const profiler::timestamp_t& _time) const
    {
        return qreal(_time - m_start) * 1e-6;
    }

    void setTree(const BlocksTree::children_t& _children, qreal _y, int _level = 0)
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
};


class MyGraphicsView : public QGraphicsView
{
    Q_OBJECT

private:

    qreal m_scale;
    qreal m_scaleCoeff;

public:

    MyGraphicsView() : QGraphicsView(), m_scale(100), m_scaleCoeff(1.25)
    {
        initMode();
        setScene(new MyGraphicsScene(this));
        centerOn(0, 0);
    }

    MyGraphicsView(const thread_blocks_tree_t& _blocksTree) : QGraphicsView(), m_scale(100), m_scaleCoeff(1.25)
    {
        initMode();
        setScene(new MyGraphicsScene(_blocksTree, this));
        centerOn(0, 0);
    }

    void initMode()
    {
        setCacheMode(QGraphicsView::CacheBackground);
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    }

    virtual ~MyGraphicsView()
    {
    }

    void wheelEvent(QWheelEvent* event)
    {
        //MyGraphicsScene* myscene = (MyGraphicsScene*)scene();
        if (event->delta() > 0)
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
};

#endif // MY____GRAPHICS___VIEW_H
