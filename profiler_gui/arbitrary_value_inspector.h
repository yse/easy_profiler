/************************************************************************
* file name         : arbitrary_value_inspector.h
* ----------------- :
* creation time     : 2017/11/30
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of .
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

#ifndef EASY_PROFILER_GUI_ARBITRARY_VALUE_INSPECTOR_H
#define EASY_PROFILER_GUI_ARBITRARY_VALUE_INSPECTOR_H

#include <QWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QGraphicsItem>
#include <QGraphicsView>
#include <QTimer>
#include <QPointF>
#include <QList>
#include <easy/serialized_block.h>
#include <easy/reader.h>
#include <easy/utility.h>
#include <unordered_map>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <memory>

//////////////////////////////////////////////////////////////////////////

using Points = std::vector<QPointF>;
using ArbitraryValues = std::vector<const profiler::ArbitraryValue*>;
using ArbitraryValuesMap = std::unordered_map<profiler::thread_id_t, ArbitraryValues, estd::hash<profiler::thread_id_t> >;

class ArbitraryValuesCollection EASY_FINAL
{
public:

    enum JobStatus : uint8_t { Idle = 0, Ready, InProgress };
    enum JobType : uint8_t { None = 0, ValuesJob = 1 << 0, PointsJob = 1 << 1 };

private:

    using This = ArbitraryValuesCollection;

    ArbitraryValuesMap          m_values;
    Points                      m_points;
    std::thread        m_collectorThread;
    profiler::timestamp_t    m_beginTime;
    qreal                     m_minValue;
    qreal                     m_maxValue;
    std::atomic<uint8_t>        m_status;
    std::atomic_bool        m_bInterrupt;
    uint8_t                    m_jobType;

public:

    explicit ArbitraryValuesCollection();
    ~ArbitraryValuesCollection();

    const ArbitraryValuesMap& valuesMap() const;
    const Points& points() const;
    JobStatus status() const;
    size_t size() const;

    qreal minValue() const;
    qreal maxValue() const;

    void collectValues(profiler::thread_id_t _threadId, profiler::vin_t _valueId, const char* _valueName);
    void collectValues(profiler::thread_id_t _threadId, profiler::vin_t _valueId, const char* _valueName, profiler::timestamp_t _beginTime);
    bool calculatePoints(profiler::timestamp_t _beginTime);
    void interrupt();

private:

    void setStatus(JobStatus _status);
    void collectById(profiler::thread_id_t _threadId, profiler::vin_t _valueId);
    void collectByName(profiler::thread_id_t _threadId, const std::string _valueName);
    bool collectByIdForThread(const profiler::BlocksTreeRoot& _threadRoot, profiler::vin_t _valueId, bool _calculatePoints);
    bool collectByNameForThread(const profiler::BlocksTreeRoot& _threadRoot, const std::string& _valueName, bool _calculatePoints);

    QPointF point(const profiler::ArbitraryValue& _value) const;

}; // end of class ArbitraryValuesCollection.

enum class ChartType : uint8_t
{
    Line = 0,
    Points
};

struct EasyCollectionPaintData EASY_FINAL
{
    const ArbitraryValuesCollection* ptr;
    QRgb                           color;
    ChartType                  chartType;
    bool                        selected;
};

using Collections = std::vector<EasyCollectionPaintData>;

//////////////////////////////////////////////////////////////////////////

class EasyArbitraryValuesChartItem : public QGraphicsItem
{
    using Parent = QGraphicsItem;
    using This = EasyArbitraryValuesChartItem;

    Collections m_collections;
    QRectF     m_boundingRect;

public:

    explicit EasyArbitraryValuesChartItem();
    ~EasyArbitraryValuesChartItem() override;

    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

    QRectF boundingRect() const override;
    void setBoundingRect(const QRectF& _rect);
    void setBoundingRect(qreal _left, qreal _top, qreal _width, qreal _height);

    void update(Collections _collections);
    void update(const ArbitraryValuesCollection* _selected);

}; // end of class EasyArbitraryValuesChartItem.

class EasyGraphicsChart : public QGraphicsView
{
    Q_OBJECT

private:

    using Parent = QGraphicsView;
    using This = EasyGraphicsChart;

    EasyArbitraryValuesChartItem* m_chartItem;
    qreal               m_left;
    qreal              m_right;
    qreal             m_offset;
    qreal             m_xscale;
    qreal m_visibleRegionWidth;
    bool           m_bBindMode;

public:

    explicit EasyGraphicsChart(QWidget* _parent = nullptr);
    ~EasyGraphicsChart() override;

    void resizeEvent(QResizeEvent* _event) override;

    void clear();

    bool bindMode() const;
    qreal xscale() const;

    qreal left() const;
    qreal right() const;
    qreal range() const;
    qreal offset() const;
    qreal region() const;

    void setOffset(qreal _offset);
    void setRange(qreal _left, qreal _right);
    void setRegion(qreal _visibleRegionWidth);

    void update(Collections _collections);
    void update(const ArbitraryValuesCollection* _selected);

private slots:

    void onSceneSizeChanged();
    void onWindowSizeChanged(qreal _width, qreal _height);

}; // end of class EasyGraphicsChart.

//////////////////////////////////////////////////////////////////////////

class EasyArbitraryTreeWidgetItem : public QTreeWidgetItem
{
    using Parent = QTreeWidgetItem;
    using This = EasyArbitraryTreeWidgetItem;
    using CollectionPtr = std::unique_ptr<ArbitraryValuesCollection>;

    CollectionPtr   m_collection;
    profiler::vin_t        m_vin;
    profiler::color_t    m_color;
    int              m_widthHint;

public:

    explicit EasyArbitraryTreeWidgetItem(QTreeWidgetItem* _parent, profiler::color_t _color, profiler::vin_t _vin = 0);
    ~EasyArbitraryTreeWidgetItem() override;

    QVariant data(int _column, int _role) const override;

    void setWidthHint(int _width);

    const ArbitraryValuesCollection* collection() const;
    void collectValues(profiler::thread_id_t _threadId);
    void interrupt();

    profiler::color_t color() const;

}; // end of class EasyArbitraryTreeWidgetItem.

//////////////////////////////////////////////////////////////////////////

class EasyArbitraryValuesWidget : public QWidget
{
    Q_OBJECT

    using Parent = QWidget;
    using This = EasyArbitraryValuesWidget;

    QTimer                  m_timer;
    QTimer       m_collectionsTimer;
    QList<EasyArbitraryTreeWidgetItem*> m_checkedItems;
    QTreeWidget*       m_treeWidget;
    EasyGraphicsChart*      m_chart;

public:

    explicit EasyArbitraryValuesWidget(QWidget* _parent = nullptr);
    ~EasyArbitraryValuesWidget() override;

    void clear();

public slots:

    void rebuild();

private slots:

    void onSelectedThreadChanged(profiler::thread_id_t _id);
    void onSelectedBlockChanged(uint32_t _block_index);
    void onSelectedBlockIdChanged(profiler::block_id_t _id);
    void onItemDoubleClicked(QTreeWidgetItem* _item, int _column);
    void onItemChanged(QTreeWidgetItem* _item, int _column);
    void onCurrentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
    void onCollectionsTimeout();

private:

    void buildTree(profiler::thread_id_t _threadId, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId);
    QTreeWidgetItem* buildTreeForThread(const profiler::BlocksTreeRoot& _threadRoot, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId);

}; // end of class EasyArbitraryValuesWidget.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_GUI_ARBITRARY_VALUE_INSPECTOR_H
