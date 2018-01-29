

#ifndef EASY_PROFILER_GRAPHICS_IMAGE_ITEM_H
#define EASY_PROFILER_GRAPHICS_IMAGE_ITEM_H

#include <QGraphicsItem>
#include <atomic>
#include "easy_qtimer.h"
#include "thread_pool_task.h"

class GraphicsImageItem : public QGraphicsItem
{
protected:

    using Parent = QGraphicsItem;
    using This = GraphicsImageItem;

    QRectF      m_boundingRect;
    QImage             m_image;
    Timer      m_boundaryTimer;
    ThreadPoolTask    m_worker;
    QPointF         m_mousePos;
    QImage*      m_workerImage;
    qreal        m_imageOrigin;
    qreal         m_imageScale;
    qreal  m_imageOriginUpdate;
    qreal   m_imageScaleUpdate;
    qreal  m_workerImageOrigin;
    qreal   m_workerImageScale;
    qreal           m_topValue;
    qreal        m_bottomValue;
    qreal           m_maxValue;
    qreal           m_minValue;
    std::atomic_bool  m_bReady;

private:

    Timer         m_timer;
    bool  m_bPermitImageUpdate; ///< Is false when m_workerThread is parsing input dataset (when setSource(_block_id) is called)

public:

    explicit GraphicsImageItem();
    ~GraphicsImageItem() override;

    QRectF boundingRect() const override;

    virtual bool pickTopValue();
    virtual bool increaseTopValue();
    virtual bool decreaseTopValue();

    virtual bool pickBottomValue();
    virtual bool increaseBottomValue();
    virtual bool decreaseBottomValue();

    virtual bool updateImage();
    virtual void onModeChanged();

protected:

    virtual void onImageUpdated();

public:

    void onValueChanged();
    void setMousePos(const QPointF& pos);
    void setMousePos(qreal x, qreal y);
    void setBoundingRect(const QRectF& _rect);
    void setBoundingRect(qreal x, qreal y, qreal w, qreal h);
    bool cancelImageUpdate();

protected:

    void paintImage(QPainter* _painter);
    void paintImage(QPainter* _painter, qreal _scale, qreal _sceneLeft, qreal _sceneRight,
                    qreal _visibleRegionLeft, qreal _visibleRegionWidth);

    void setImageUpdatePermitted(bool _permit);
    bool isImageUpdatePermitted() const;

    void cancelAnyJob();
    void resetTopBottomValues();

    bool isReady() const;
    void setReady(bool _ready);

    void startTimer();
    void stopTimer();

private:

    void onTimeout();

}; // end of class GraphicsImageItem.

#endif //EASY_PROFILER_GRAPHICS_IMAGE_ITEM_H
