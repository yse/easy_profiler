/************************************************************************
* file name         : arbitrary_value_inspector.cpp
* ----------------- :
* creation time     : 2017/11/30
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of .
* ----------------- :
* change log        : * 2017/11/30 Victor Zarubkin: initial commit.
*                   :
*                   : *
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2017  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
*                   :
*                   : The MIT License
*                   :
*                   : Permission is hereby granted, free of charge, to any person obtaining a copy
*                   : of this software and associated documentation files (the "Software"), to deal
*                   : in the Software without restriction, including without limitation the rights
*                   : to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*                   : of the Software, and to permit persons to whom the Software is furnished
*                   : to do so, subject to the following conditions:
*                   :
*                   : The above copyright notice and this permission notice shall be included in all
*                   : copies or substantial portions of the Software.
*                   :
*                   : THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
*                   : INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
*                   : PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
*                   : LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*                   : TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
*                   : USE OR OTHER DEALINGS IN THE SOFTWARE.
*                   :
*                   : The Apache License, Version 2.0 (the "License")
*                   :
*                   : You may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
************************************************************************/

#include <QPainter>
#include <QGraphicsScene>
#include <QColor>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QResizeEvent>
#include <QVariant>
#include <list>
#include "arbitrary_value_inspector.h"
#include "treeview_first_column_delegate.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////

void getChartPoints(const ArbitraryValuesCollection& _collection, Points& _points, qreal& _minValue, qreal& _maxValue)
{
    _minValue =  1e300;
    _maxValue = -1e300;

    const auto size = _collection.size();
    _points.clear();
    _points.reserve(size);

    if (size == 0)
        return;

    const auto& valuesByThread = _collection.valuesMap();

    if (valuesByThread.size() == 1)
    {
        const auto& values = valuesByThread.begin()->second;
        for (auto value : values)
        {
            const qreal x = sceneX(value->begin());
            const qreal y = profiler_gui::value2real(*value);
            _points.emplace_back(x, y);

            if (y > _maxValue)
                _maxValue = y;
            if (y < _minValue)
                _minValue = y;
        }
    }
    else
    {
        std::list<profiler::thread_id_t> threadIds;
        for (const auto& it : valuesByThread)
            threadIds.push_back(it.first);

        size_t i = 0;
        while (!threadIds.empty())
        {
            for (auto it = threadIds.begin(); it != threadIds.end();)
            {
                const auto& values = valuesByThread.at(*it);
                if (i >= values.size())
                {
                    it = threadIds.erase(it);
                    continue;
                }

                const qreal x = sceneX(values[i]->begin());
                const qreal y = profiler_gui::value2real(*values[i]);
                _points.emplace_back(x, y);

                if (y > _maxValue)
                    _maxValue = y;
                if (y < _minValue)
                    _minValue = y;

                ++it;
            }
        }

        std::sort(_points.begin(), _points.end(), [](const QPointF& lhs, const QPointF& rhs) -> bool {
            return lhs.x() < rhs.x();
        });
    }
}

//////////////////////////////////////////////////////////////////////////

ArbitraryValuesCollection::ArbitraryValuesCollection()
    : m_beginTime(0)
    , m_minValue(1e300)
    , m_maxValue(-1e300)
    , m_jobType(0)
{
    m_status = ATOMIC_VAR_INIT(Idle);
    m_bInterrupt = ATOMIC_VAR_INIT(false);
}

ArbitraryValuesCollection::~ArbitraryValuesCollection()
{
    interrupt();
}

const ArbitraryValuesMap& ArbitraryValuesCollection::valuesMap() const
{
    return m_values;
}

const Points& ArbitraryValuesCollection::points() const
{
    return m_points;
}

ArbitraryValuesCollection::JobStatus ArbitraryValuesCollection::status() const
{
    return static_cast<JobStatus>(m_status.load(std::memory_order_acquire));
}

size_t ArbitraryValuesCollection::size() const
{
    size_t totalSize = 0;
    for (const auto& it : m_values)
        totalSize += it.second.size();
    return totalSize;
}

qreal ArbitraryValuesCollection::minValue() const
{
    return m_minValue;
}

qreal ArbitraryValuesCollection::maxValue() const
{
    return m_maxValue;
}

void ArbitraryValuesCollection::collectValues(profiler::thread_id_t _threadId, profiler::vin_t _valueId, const char* _valueName)
{
    interrupt();

    setStatus(InProgress);
    m_points.clear();
    m_jobType = ValuesJob;

    if (_valueId == 0)
        m_collectorThread = std::thread(&This::collectByName, this, _threadId, _valueName);
    else
        m_collectorThread = std::thread(&This::collectById, this, _threadId, _valueId);
}

void ArbitraryValuesCollection::collectValues(profiler::thread_id_t _threadId, profiler::vin_t _valueId, const char* _valueName, profiler::timestamp_t _beginTime)
{
    interrupt();

    setStatus(InProgress);
    m_points.clear();
    m_beginTime = _beginTime;
    m_minValue =  1e300;
    m_maxValue = -1e300;
    m_jobType = ValuesJob | PointsJob;

    if (_valueId == 0)
        m_collectorThread = std::thread(&This::collectByName, this, _threadId, _valueName);
    else
        m_collectorThread = std::thread(&This::collectById, this, _threadId, _valueId);
}

bool ArbitraryValuesCollection::calculatePoints(profiler::timestamp_t _beginTime)
{
    if (status() != Ready || m_values.empty())
        return false;

    if (m_collectorThread.joinable())
        m_collectorThread.join();

    setStatus(InProgress);
    m_points.clear();
    m_beginTime = _beginTime;
    m_minValue =  1e300;
    m_maxValue = -1e300;
    m_jobType = PointsJob;

    m_collectorThread = std::thread([this]
    {
        getChartPoints(*this, m_points, m_minValue, m_maxValue);
        setStatus(Ready);
    });

    return true;
}

void ArbitraryValuesCollection::interrupt()
{
    if (!m_collectorThread.joinable())
        return;

    m_bInterrupt.store(true, std::memory_order_release);
    m_collectorThread.join();
    m_bInterrupt.store(false, std::memory_order_release);

    setStatus(Idle);
    m_jobType = None;
    m_values.clear();
}

void ArbitraryValuesCollection::setStatus(JobStatus _status)
{
    m_status.store(static_cast<uint8_t>(_status), std::memory_order_release);
}

void ArbitraryValuesCollection::collectById(profiler::thread_id_t _threadId, profiler::vin_t _valueId)
{
    if (_threadId == 0)
    {
        const auto threadsCount = EASY_GLOBALS.profiler_blocks.size();
        const bool calculatePointsInner = (m_jobType & PointsJob) != 0 && threadsCount == 1;

        for (const auto& it : EASY_GLOBALS.profiler_blocks)
        {
            if (!collectByIdForThread(it.second, _valueId, calculatePointsInner))
                return;
        }

        if ((m_jobType & PointsJob) != 0 && threadsCount > 1)
            getChartPoints(*this, m_points, m_minValue, m_maxValue);
    }
    else
    {
        const auto t = EASY_GLOBALS.profiler_blocks.find(_threadId);
        if (t != EASY_GLOBALS.profiler_blocks.end() && !collectByIdForThread(t->second, _valueId, (m_jobType & PointsJob) != 0))
            return;
    }

    setStatus(Ready);
}

bool ArbitraryValuesCollection::collectByIdForThread(const profiler::BlocksTreeRoot& _threadRoot,
                                                     profiler::vin_t _valueId, bool _calculatePoints)
{
    auto& valuesList = m_values[_threadRoot.thread_id];

    for (auto i : _threadRoot.events)
    {
        if (m_bInterrupt.load(std::memory_order_acquire))
            return false;

        const auto& block = easyBlock(i).tree;
        const auto& desc = easyDescriptor(block.node->id());
        if (desc.type() != profiler::BlockType::Value)
            continue;

        const auto value = block.value;
        if (value->value_id() != _valueId)
            continue;

        valuesList.push_back(value);

        if (_calculatePoints)
        {
            const auto p = point(*block.value);

            if (p.y() > m_maxValue)
                m_maxValue = p.y();
            if (p.y() < m_minValue)
                m_minValue = p.y();

            m_points.push_back(p);
        }
    }

    return true;
}

void ArbitraryValuesCollection::collectByName(profiler::thread_id_t _threadId, const std::string _valueName)
{
    if (_threadId == 0)
    {
        const auto threadsCount = EASY_GLOBALS.profiler_blocks.size();
        const bool calculatePointsInner = (m_jobType & PointsJob) != 0 && threadsCount == 1;

        for (const auto& it : EASY_GLOBALS.profiler_blocks)
        {
            if (!collectByNameForThread(it.second, _valueName, calculatePointsInner))
                return;
        }

        if ((m_jobType & PointsJob) != 0 && threadsCount > 1)
            getChartPoints(*this, m_points, m_minValue, m_maxValue);
    }
    else
    {
        const auto t = EASY_GLOBALS.profiler_blocks.find(_threadId);
        if (t != EASY_GLOBALS.profiler_blocks.end() && !collectByNameForThread(t->second, _valueName, (m_jobType & PointsJob) != 0))
            return;
    }

    setStatus(Ready);
}

bool ArbitraryValuesCollection::collectByNameForThread(const profiler::BlocksTreeRoot& _threadRoot,
                                                       const std::string& _valueName, bool _calculatePoints)
{
    auto& valuesList = m_values[_threadRoot.thread_id];

    for (auto i : _threadRoot.events)
    {
        if (m_bInterrupt.load(std::memory_order_acquire))
            return false;

        const auto& block = easyBlock(i).tree;
        const auto& desc = easyDescriptor(block.node->id());
        if (desc.type() != profiler::BlockType::Value || _valueName != desc.name())
            continue;

        valuesList.push_back(block.value);

        if (_calculatePoints)
        {
            const auto p = point(*block.value);

            if (p.y() > m_maxValue)
                m_maxValue = p.y();
            if (p.y() < m_minValue)
                m_minValue = p.y();

            m_points.push_back(p);
        }
    }

    return true;
}

QPointF ArbitraryValuesCollection::point(const profiler::ArbitraryValue& _value) const
{
    const qreal x = PROF_MICROSECONDS(qreal(_value.begin() - m_beginTime));
    const qreal y = profiler_gui::value2real(_value);
    return {x, y};
}

//////////////////////////////////////////////////////////////////////////

EasyArbitraryValuesChartItem::EasyArbitraryValuesChartItem()
    : Parent(nullptr)
{
}

EasyArbitraryValuesChartItem::~EasyArbitraryValuesChartItem()
{

}

void EasyArbitraryValuesChartItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (m_collections.empty())
        return;

    const auto& chart = *reinterpret_cast<const EasyGraphicsChart*>(scene()->parent());
    const auto scale = chart.xscale();

    qreal minValue = 1e300, maxValue = -1e300;
    for (const auto& c : m_collections)
    {
        const auto& collection = *c.ptr;
        if (minValue > collection.minValue())
            minValue = collection.minValue();
        if (maxValue < collection.maxValue())
            maxValue = collection.maxValue();
    }

    const qreal height = std::max(maxValue - minValue, 1.);

    auto r = scene()->sceneRect();


    _painter->save();

    for (const auto& c : m_collections)
    {
        const auto& points = c.ptr->points();
        if (points.empty())
            continue;

        if (c.selected)
        {
            auto pen = _painter->pen();
            pen.setColor(QColor::fromRgba(c.color));
            pen.setWidth(3);
            _painter->setPen(pen);
        }
        else
        {
            _painter->setPen(QColor::fromRgba(c.color));
        }

        if (points.size() == 1)
            _painter->drawPoint(points.front());
        else
        {
            auto gety = [&r, &minValue, &maxValue, &height] (qreal y)
            {
                y = maxValue - y;
                y /= height;
                y *= r.height() - 10;
                y += r.top() + 5;
                return y;
            };

            if (c.chartType == ChartType::Points)
            {
                for (const auto& p : points)
                    _painter->drawPoint(QPointF {p.x() * scale, gety(p.y())});
            }
            else
            {
                QPointF p1 = points.front();
                for (int i = 1; i < points.size(); ++i)
                {
                    const auto& p2 = points[i];
                    _painter->drawLine(QPointF {p1.x() * scale, gety(p1.y())}, QPointF {p2.x() * scale, gety(p2.y())});
                    p1 = p2;
                }
            }
            //_painter->drawPolyline(points.data(), static_cast<int>(points.size()));
        }
    }

    _painter->restore();
}

QRectF EasyArbitraryValuesChartItem::boundingRect() const
{
    return m_boundingRect;
}

void EasyArbitraryValuesChartItem::setBoundingRect(const QRectF& _rect)
{
    m_boundingRect = _rect;
}

void EasyArbitraryValuesChartItem::setBoundingRect(qreal _left, qreal _top, qreal _width, qreal _height)
{
    m_boundingRect.setRect(_left, _top, _width, _height);
}

void EasyArbitraryValuesChartItem::update(Collections _collections)
{
    m_collections = std::move(_collections);
}

void EasyArbitraryValuesChartItem::update(const ArbitraryValuesCollection* _selected)
{
    for (auto& collection : m_collections)
        collection.selected = collection.ptr == _selected;
}

//////////////////////////////////////////////////////////////////////////

EasyGraphicsChart::EasyGraphicsChart(QWidget* _parent)
    : Parent(_parent)
    , m_chartItem(new EasyArbitraryValuesChartItem())
    , m_left(0)
    , m_right(100)
    , m_offset(0)
    , m_xscale(1)
    , m_visibleRegionWidth(100)
    , m_bBindMode(false)
{
    setCacheMode(QGraphicsView::CacheNone);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    //setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setOptimizationFlag(QGraphicsView::DontSavePainterState, true);

    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setContentsMargins(0, 0, 0, 0);

    setScene(new QGraphicsScene(this));
    scene()->setSceneRect(0, -250, 500, 500);
    scene()->addItem(m_chartItem);

    connect(&EASY_GLOBALS.events, &profiler_gui::EasyGlobalSignals::sceneSizeChanged,
            this, &This::onSceneSizeChanged, Qt::QueuedConnection);

    onSceneSizeChanged();
}

EasyGraphicsChart::~EasyGraphicsChart()
{

}

void EasyGraphicsChart::onSceneSizeChanged()
{
    setRange(EASY_GLOBALS.scene_left, EASY_GLOBALS.scene_right);
}

void EasyGraphicsChart::resizeEvent(QResizeEvent* _event)
{
    auto size = _event->size();
    onWindowSizeChanged(size.width(), size.height());
    scene()->update();
}

void EasyGraphicsChart::clear()
{
    m_chartItem->update(Collections {});
}

bool EasyGraphicsChart::bindMode() const
{
    return m_bBindMode;
}

qreal EasyGraphicsChart::xscale() const
{
    return m_xscale;
}

qreal EasyGraphicsChart::left() const
{
    return m_left;
}

qreal EasyGraphicsChart::right() const
{
    return m_right;
}

qreal EasyGraphicsChart::range() const
{
    return m_right - m_left;
}

qreal EasyGraphicsChart::offset() const
{
    return m_bBindMode ? m_offset : 0;
}

qreal EasyGraphicsChart::region() const
{
    return m_bBindMode ? m_visibleRegionWidth : range();
}

void EasyGraphicsChart::setOffset(qreal _offset)
{
    m_offset = std::min(std::max(m_left, m_offset), m_right - m_visibleRegionWidth);
}

void EasyGraphicsChart::setRange(qreal _left, qreal _right)
{
    const auto oldRange = range();
    const auto oldOffsetPart = oldRange < 1e-3 ? 0.0 : m_offset / oldRange;

    m_left = _left;
    m_right = _right;

    if (m_left > m_right)
        std::swap(m_left, m_right);

    const auto sceneRange = range();
    //scene()->setSceneRect(m_left, -(height() >> 1), sceneRange, height());
    //m_chartItem->setBoundingRect(scene()->sceneRect());

    m_offset = m_left + oldOffsetPart * sceneRange;
    m_visibleRegionWidth = std::min(m_visibleRegionWidth, sceneRange);

    //const auto oldXScale = m_xscale;
    m_xscale = sceneRange < 1e-3 ? 1.0 : width() / sceneRange;
    //scale(m_xscale / oldXScale, 1);

    scene()->update();
}

void EasyGraphicsChart::setRegion(qreal _visibleRegionWidth)
{
    m_visibleRegionWidth = std::min(_visibleRegionWidth, range());
    setOffset(m_offset);
}

void EasyGraphicsChart::onWindowSizeChanged(qreal _width, qreal _height)
{
    //const auto oldXScale = m_xscale;
    const auto sceneRange = range();

    m_xscale = sceneRange < 1e-3 ? 1.0 : _width / sceneRange;

    scene()->setSceneRect(0, -_height * 0.5, _width, _height);
    //scene()->setSceneRect(m_left, -_height * 0.5, sceneRange, _height);
    m_chartItem->setBoundingRect(scene()->sceneRect());
    //scale(m_xscale / oldXScale, 1);
}

void EasyGraphicsChart::update(Collections _collections)
{
    m_chartItem->update(std::move(_collections));
    scene()->update();
}

void EasyGraphicsChart::update(const ArbitraryValuesCollection* _selected)
{
    m_chartItem->update(_selected);
    scene()->update();
}

//////////////////////////////////////////////////////////////////////////

enum class ArbitraryColumns : uint8_t
{
    Name = 0,
    Type,
    Value,
    Vin,

    Count
};

EASY_CONSTEXPR auto CheckColumn = int_cast(ArbitraryColumns::Name);
EASY_CONSTEXPR auto StdItemType = QTreeWidgetItem::UserType;
EASY_CONSTEXPR auto ValueItemType = QTreeWidgetItem::UserType + 1;

struct UsedValueTypes {
    EasyArbitraryTreeWidgetItem* items[int_cast(profiler::DataType::TypesCount)];
    UsedValueTypes(int = 0) { memset(items, 0, sizeof(items)); }
};

//////////////////////////////////////////////////////////////////////////

EasyArbitraryTreeWidgetItem::EasyArbitraryTreeWidgetItem(QTreeWidgetItem* _parent, profiler::color_t _color, profiler::vin_t _vin)
    : Parent(_parent, ValueItemType)
    , m_vin(_vin)
    , m_color(_color)
    , m_widthHint(0)
{
    setFlags(flags() | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
    setCheckState(CheckColumn, Qt::Unchecked);
}

EasyArbitraryTreeWidgetItem::~EasyArbitraryTreeWidgetItem()
{

}

QVariant EasyArbitraryTreeWidgetItem::data(int _column, int _role) const
{
    if (_column == CheckColumn && _role == Qt::SizeHintRole)
        return QSize(m_widthHint, 26);
    return Parent::data(_column, _role);
}

void EasyArbitraryTreeWidgetItem::setWidthHint(int _width)
{
    m_widthHint = _width;
}

const ArbitraryValuesCollection* EasyArbitraryTreeWidgetItem::collection() const
{
    return m_collection.get();
}

void EasyArbitraryTreeWidgetItem::collectValues(profiler::thread_id_t _threadId)
{
    if (!m_collection)
        m_collection = CollectionPtr(new ArbitraryValuesCollection);
    else
        m_collection->interrupt();

    m_collection->collectValues(_threadId, m_vin, text(int_cast(ArbitraryColumns::Name)).toStdString().c_str(), EASY_GLOBALS.begin_time);
}

void EasyArbitraryTreeWidgetItem::interrupt()
{
    if (!m_collection)
        return;

    m_collection->interrupt();
    m_collection.release();
}

profiler::color_t EasyArbitraryTreeWidgetItem::color() const
{
    return m_color;
}

//////////////////////////////////////////////////////////////////////////

EasyArbitraryValuesWidget::EasyArbitraryValuesWidget(QWidget* _parent)
    : Parent(_parent)
    , m_treeWidget(new QTreeWidget(this))
    , m_chart(new EasyGraphicsChart(this))
{
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_treeWidget);
    layout->addWidget(m_chart);
    layout->setStretch(0, 1);
    layout->setStretch(1, 1);

    m_treeWidget->setAutoFillBackground(false);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setItemsExpandable(true);
    m_treeWidget->setAnimated(true);
    //m_treeWidget->setSortingEnabled(false);
    m_treeWidget->setColumnCount(int_cast(ArbitraryColumns::Count));
    m_treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeWidget->setItemDelegateForColumn(0, new EasyTreeViewFirstColumnItemDelegate(this));

    auto headerItem = new QTreeWidgetItem();
    headerItem->setText(int_cast(ArbitraryColumns::Type), "Type");
    headerItem->setText(int_cast(ArbitraryColumns::Name), "Name");
    headerItem->setText(int_cast(ArbitraryColumns::Value), "Value");
    headerItem->setText(int_cast(ArbitraryColumns::Vin), "ID");
    m_treeWidget->setHeaderItem(headerItem);

//    auto mainLayout = new QVBoxLayout(this);
//    mainLayout->setContentsMargins(1, 1, 1, 1);
//    mainLayout->addWidget(m_treeWidget);

    connect(&m_timer, &QTimer::timeout, this, &This::rebuild);
    connect(&m_collectionsTimer, &QTimer::timeout, this, &This::onCollectionsTimeout);

    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked, this, &This::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::itemChanged, this, &This::onItemChanged);
    connect(m_treeWidget, &QTreeWidget::currentItemChanged, this, &This::onCurrentItemChanged);

    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChanged);
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChanged);
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedBlockIdChanged, this, &This::onSelectedBlockIdChanged);
}

EasyArbitraryValuesWidget::~EasyArbitraryValuesWidget()
{

}

void EasyArbitraryValuesWidget::clear()
{
    if (m_collectionsTimer.isActive())
        m_collectionsTimer.stop();
    if (m_timer.isActive())
        m_timer.stop();
    m_checkedItems.clear();
    m_treeWidget->clear();
}

void EasyArbitraryValuesWidget::onSelectedThreadChanged(::profiler::thread_id_t)
{
    if (!m_timer.isActive())
        m_timer.start(100);
}

void EasyArbitraryValuesWidget::onSelectedBlockChanged(uint32_t)
{
    if (!m_timer.isActive())
        m_timer.start(100);
}

void EasyArbitraryValuesWidget::onSelectedBlockIdChanged(::profiler::block_id_t)
{
    if (!m_timer.isActive())
        m_timer.start(100);
}

void EasyArbitraryValuesWidget::onItemDoubleClicked(QTreeWidgetItem* _item, int)
{
    if (_item == nullptr || _item->type() != ValueItemType)
        return;

    _item->setCheckState(CheckColumn, _item->checkState(CheckColumn) == Qt::Checked ? Qt::Unchecked : Qt::Checked);
}

void EasyArbitraryValuesWidget::onItemChanged(QTreeWidgetItem* _item, int _column)
{
    if (_item == nullptr || _item->type() != ValueItemType || _column != CheckColumn)
        return;

    auto item = static_cast<EasyArbitraryTreeWidgetItem*>(_item);

    if (item->checkState(CheckColumn) == Qt::Checked)
    {
        m_checkedItems.push_back(item);
        item->collectValues(EASY_GLOBALS.selected_thread);
        if (!m_collectionsTimer.isActive())
            m_collectionsTimer.start(100);
    }
    else
    {
        m_checkedItems.removeOne(item);
        item->interrupt();
        onCollectionsTimeout();
    }
}

void EasyArbitraryValuesWidget::onCurrentItemChanged(QTreeWidgetItem* _current, QTreeWidgetItem*)
{
    if (_current == nullptr || _current->type() != ValueItemType)
    {
        m_chart->update(nullptr);
        return;
    }

    auto item = static_cast<const EasyArbitraryTreeWidgetItem*>(_current);
    m_chart->update(item->collection());
}

void EasyArbitraryValuesWidget::rebuild()
{
    clear();

    buildTree(EASY_GLOBALS.selected_thread, EASY_GLOBALS.selected_block, EASY_GLOBALS.selected_block_id);

    m_treeWidget->expandAll();
    for (int i = 0, columns = m_treeWidget->columnCount(); i < columns; ++i)
        m_treeWidget->resizeColumnToContents(i);
}

void EasyArbitraryValuesWidget::onCollectionsTimeout()
{
    if (m_checkedItems.isEmpty())
    {
        if (m_collectionsTimer.isActive())
            m_collectionsTimer.stop();
        m_chart->update(Collections {});
        return;
    }

    Collections collections;
    collections.reserve(m_checkedItems.size());
    for (auto item : m_checkedItems)
    {
        if (item->collection()->status() != ArbitraryValuesCollection::InProgress)
        {
            collections.push_back(EasyCollectionPaintData {item->collection(), item->color(),
                ChartType::Line, item == m_treeWidget->currentItem()});
        }
    }

    if (collections.size() == m_checkedItems.size())
    {
        if (m_collectionsTimer.isActive())
            m_collectionsTimer.stop();
        m_chart->update(std::move(collections));
    }
}

void EasyArbitraryValuesWidget::buildTree(profiler::thread_id_t _threadId, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId)
{
    m_treeWidget->clear();
    m_treeWidget->setColumnHidden(int_cast(ArbitraryColumns::Value), profiler_gui::is_max(_blockIndex));

    if (_threadId != 0)
    {
        auto it = EASY_GLOBALS.profiler_blocks.find(_threadId);
        if (it != EASY_GLOBALS.profiler_blocks.end())
        {
            auto threadItem = buildTreeForThread(it->second, _blockIndex, _blockId);
            m_treeWidget->addTopLevelItem(threadItem);
        }
    }
    else
    {
        for (const auto& it : EASY_GLOBALS.profiler_blocks)
        {
            auto threadItem = buildTreeForThread(it.second, _blockIndex, _blockId);
            m_treeWidget->addTopLevelItem(threadItem);
        }
    }
}

QTreeWidgetItem* EasyArbitraryValuesWidget::buildTreeForThread(const profiler::BlocksTreeRoot& _threadRoot, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId)
{
    auto fm = m_treeWidget->fontMetrics();

    auto rootItem = new QTreeWidgetItem(StdItemType);
    rootItem->setText(int_cast(ArbitraryColumns::Type), QStringLiteral("Thread"));
    rootItem->setText(int_cast(ArbitraryColumns::Name),
        profiler_gui::decoratedThreadName(EASY_GLOBALS.use_decorated_thread_name, _threadRoot, EASY_GLOBALS.hex_thread_id));

    const bool hasConcreteBlock = !profiler_gui::is_max(_blockIndex);
    if (hasConcreteBlock)
    {
        const auto& block = easyBlocksTree(_blockIndex);
        const auto& desc = easyDescriptor(block.node->id());
        if (desc.type() == profiler::BlockType::Value)
        {
            auto valueItem = new EasyArbitraryTreeWidgetItem(rootItem, desc.color(), block.value->value_id());
            valueItem->setText(int_cast(ArbitraryColumns::Type), profiler_gui::valueTypeString(*block.value));
            valueItem->setText(int_cast(ArbitraryColumns::Name), desc.name());
            valueItem->setText(int_cast(ArbitraryColumns::Vin), QString("0x%1").arg(block.value->value_id(), 0, 16));
            valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*block.value));

            const auto sizeHintWidth = valueItem->sizeHint(CheckColumn).width();
            valueItem->setWidthHint(std::max(sizeHintWidth, fm.width(valueItem->text(CheckColumn))) + 32);

            return rootItem;
        }

        _blockId = block.node->id();
    }

    const bool noId = profiler_gui::is_max(_blockId);
    QTreeWidgetItem* blockItem = nullptr;
    if (!noId)
    {
        blockItem = new QTreeWidgetItem(rootItem, StdItemType);
        blockItem->setText(int_cast(ArbitraryColumns::Type), QStringLiteral("Block"));
        if (hasConcreteBlock)
            blockItem->setText(int_cast(ArbitraryColumns::Name), easyBlockName(_blockIndex));
        else
            blockItem->setText(int_cast(ArbitraryColumns::Name), easyDescriptor(_blockId).name());
    }

    std::unordered_map<profiler::block_id_t, QTreeWidgetItem*, estd::hash<profiler::block_id_t> > blocks;
    std::unordered_map<profiler::vin_t, UsedValueTypes, estd::hash<profiler::vin_t> > vins;
    std::unordered_map<std::string, UsedValueTypes> names;

    std::vector<profiler::block_index_t> stack;
    for (auto childIndex : _threadRoot.children)
    {
        stack.push_back(childIndex);
        while (!stack.empty())
        {
            const auto i = stack.back();
            stack.pop_back();

            const auto& block = easyBlocksTree(i);
            if (noId || block.node->id() == _blockId || easyDescriptor(block.node->id()).id() == _blockId)
            {
                for (auto c : block.children)
                {
                    if (noId)
                        stack.push_back(c);

                    const auto& child = easyBlocksTree(c);
                    const auto& desc = easyDescriptor(child.node->id());
                    if (desc.type() != profiler::BlockType::Value)
                        continue;

                    if (blockItem == nullptr)
                    {
                        const auto id = block.node->id();
                        auto it = blocks.find(id);
                        if (it != blocks.end())
                        {
                            blockItem = it->second;
                        }
                        else
                        {
                            blockItem = new QTreeWidgetItem(rootItem, StdItemType);
                            blockItem->setText(int_cast(ArbitraryColumns::Type), QStringLiteral("Block"));
                            blockItem->setText(int_cast(ArbitraryColumns::Name), easyBlockName(block));
                            blocks.emplace(id, blockItem);
                        }
                    }

                    const auto typeIndex = int_cast(child.value->type());
                    auto vin = child.value->value_id();

                    EasyArbitraryTreeWidgetItem** usedItems = nullptr;
                    EasyArbitraryTreeWidgetItem* valueItem = nullptr;
                    if (vin == 0)
                    {
                        auto result = names.emplace(desc.name(), 0);
                        usedItems = result.first->second.items;
                        if (!result.second && (valueItem = *(usedItems + typeIndex)))
                        {
                            if (i == _blockIndex)
                                valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*child.value));
                            continue; // already in set
                        }
                    }
                    else
                    {
                        auto result = vins.emplace(vin, 0);
                        usedItems = result.first->second.items;
                        if (!result.second && (valueItem = *(usedItems + typeIndex)))
                        {
                            if (i == _blockIndex)
                                valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*child.value));
                            continue; // already in set
                        }
                    }

                    valueItem = new EasyArbitraryTreeWidgetItem(blockItem, desc.color(), vin);
                    valueItem->setText(int_cast(ArbitraryColumns::Type), profiler_gui::valueTypeString(*child.value));
                    valueItem->setText(int_cast(ArbitraryColumns::Name), desc.name());
                    valueItem->setText(int_cast(ArbitraryColumns::Vin), QString("0x%1").arg(vin, 0, 16));

                    if (i == _blockIndex)
                        valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*child.value));

                    const auto sizeHintWidth = valueItem->sizeHint(CheckColumn).width();
                    valueItem->setWidthHint(std::max(sizeHintWidth, fm.width(valueItem->text(CheckColumn))) + 32);

                    *(usedItems + typeIndex) = valueItem;
                }

                if (noId)
                    blockItem = nullptr;
            }
            else
            {
                for (auto c : block.children)
                    stack.push_back(c);
            }
        }
    }

    return rootItem;
}

