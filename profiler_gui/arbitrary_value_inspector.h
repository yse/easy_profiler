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
#include "graphics_slider_area.h"

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

struct CollectionPaintData EASY_FINAL
{
    const ArbitraryValuesCollection* ptr;
    QRgb                           color;
    ChartType                  chartType;
    bool                        selected;
};

using Collections = std::vector<CollectionPaintData>;

//////////////////////////////////////////////////////////////////////////

class ArbitraryValuesChartItem : public GraphicsImageItem
{
    using Parent = GraphicsImageItem;
    using This = ArbitraryValuesChartItem;

    Collections m_collections;
    qreal    m_workerMaxValue;
    qreal    m_workerMinValue;

public:

    explicit ArbitraryValuesChartItem();
    ~ArbitraryValuesChartItem() override;

    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget) override;

    bool updateImage() override;

protected:

    void onImageUpdated() override;

public:

    void clear();
    void update(Collections _collections);
    void update(const ArbitraryValuesCollection* _selected);

private:

    void paintMouseIndicator(QPainter* _painter, qreal _top, qreal _bottom, qreal _width, qreal _height, int _font_h,
                             qreal _visibleRegionLeft, qreal _visibleRegionWidth);

    void updateImageAsync(QRectF _boundingRect, qreal _current_scale, qreal _minimum, qreal _maximum, qreal _range,
        qreal _value, qreal _width, bool _bindMode, profiler::timestamp_t _begin_time, bool _autoAdjust);

}; // end of class ArbitraryValuesChartItem.

class GraphicsChart : public GraphicsSliderArea
{
    Q_OBJECT

private:

    using Parent = GraphicsSliderArea;
    using This = GraphicsChart;

    ArbitraryValuesChartItem* m_chartItem;
    qreal               m_left;
    qreal              m_right;
    qreal             m_offset;
    qreal             m_xscale;
    qreal m_visibleRegionWidth;
    bool           m_bBindMode;

public:

    explicit GraphicsChart(QWidget* _parent = nullptr);
    ~GraphicsChart() override;

    void clear() override;

public:

    void update(Collections _collections);
    void update(const ArbitraryValuesCollection* _selected);

}; // end of class GraphicsChart.

//////////////////////////////////////////////////////////////////////////

class ArbitraryTreeWidgetItem : public QTreeWidgetItem
{
    using Parent = QTreeWidgetItem;
    using This = ArbitraryTreeWidgetItem;
    using CollectionPtr = std::unique_ptr<ArbitraryValuesCollection>;

    QFont                 m_font;
    CollectionPtr   m_collection;
    profiler::vin_t        m_vin;
    profiler::color_t    m_color;
    int              m_widthHint;

public:

    explicit ArbitraryTreeWidgetItem(QTreeWidgetItem* _parent, profiler::color_t _color, profiler::vin_t _vin = 0);
    ~ArbitraryTreeWidgetItem() override;

    QVariant data(int _column, int _role) const override;

    void setWidthHint(int _width);
    void setBold(bool _isBold);

    const ArbitraryValuesCollection* collection() const;
    void collectValues(profiler::thread_id_t _threadId);
    void interrupt();

    profiler::color_t color() const;

}; // end of class ArbitraryTreeWidgetItem.

//////////////////////////////////////////////////////////////////////////

class ArbitraryValuesWidget : public QWidget
{
    Q_OBJECT

    using Parent = QWidget;
    using This = ArbitraryValuesWidget;

    QTimer                      m_collectionsTimer;
    QList<ArbitraryTreeWidgetItem*> m_checkedItems;
    class QSplitter*                    m_splitter;
    QTreeWidget*                      m_treeWidget;
    GraphicsChart*                         m_chart;
    ArbitraryTreeWidgetItem*            m_boldItem;

public:

    explicit ArbitraryValuesWidget(QWidget* _parent = nullptr);
    ~ArbitraryValuesWidget() override;

    void contextMenuEvent(QContextMenuEvent*) override {}

public slots:

    void clear();
    void rebuild();

private slots:

    void onSelectedBlockChanged(uint32_t _block_index);
    void onSelectedBlockIdChanged(profiler::block_id_t _id);
    void onItemDoubleClicked(QTreeWidgetItem* _item, int _column);
    void onItemChanged(QTreeWidgetItem* _item, int _column);
    void onCurrentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
    void onCollectionsTimeout();

private:

    void buildTree(profiler::thread_id_t _threadId, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId);
    QTreeWidgetItem* buildTreeForThread(const profiler::BlocksTreeRoot& _threadRoot, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId);

    void loadSettings();
    void saveSettings();

}; // end of class ArbitraryValuesWidget.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_GUI_ARBITRARY_VALUE_INSPECTOR_H
