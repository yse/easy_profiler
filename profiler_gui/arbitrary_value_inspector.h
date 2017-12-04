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
#include <QTimer>
#include <QPointF>
#include <unordered_map>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <easy/serialized_block.h>
#include <easy/reader.h>

//////////////////////////////////////////////////////////////////////////

using ArbitraryValues = std::vector<const profiler::ArbitraryValue*>;
using ArbitraryValuesMap = std::unordered_map<profiler::thread_id_t, ArbitraryValues, profiler::passthrough_hash<profiler::thread_id_t> >;

class ArbitraryValuesCollection EASY_FINAL
{
    ArbitraryValuesMap          m_values;
    std::thread        m_collectorThread;
    std::atomic_bool            m_bReady;
    std::atomic_bool        m_bInterrupt;

public:

    explicit ArbitraryValuesCollection();
    ~ArbitraryValuesCollection();

    const ArbitraryValuesMap& valuesMap() const;
    bool ready() const;
    size_t size() const;

    void collectValues(profiler::thread_id_t _threadId, profiler::vin_t _valueId, const char* _valueName);
    void interrupt();

private:

    void setReady(bool _ready);
    void collectById(profiler::thread_id_t _threadId, profiler::vin_t _valueId);
    void collectByName(profiler::thread_id_t _threadId, const std::string _valueName);
    bool collectByIdForThread(profiler::thread_id_t _threadId, profiler::vin_t _valueId);
    bool collectByNameForThread(profiler::thread_id_t _threadId, const std::string& _valueName);

}; // end of class ArbitraryValuesCollection.

//////////////////////////////////////////////////////////////////////////

class EasyArbitraryValueInspector : public QWidget
{
    Q_OBJECT

    using Parent = QWidget;
    using This = EasyArbitraryValueInspector;

public:

    explicit EasyArbitraryValueInspector(QWidget* _parent = nullptr);
    ~EasyArbitraryValueInspector() override;

}; // end of class EasyArbitraryValueInspector.

//////////////////////////////////////////////////////////////////////////

class EasyArbitraryValueItem : public QGraphicsItem
{
    using Parent = QGraphicsItem;
    using This = EasyArbitraryValueItem;
    using Points = std::vector<QPointF>;

    ArbitraryValuesCollection m_collection;
    Points                        m_points;
    QTimer                         m_timer;
    qreal                          m_scale;
    QRgb                           m_color;

public:

    explicit EasyArbitraryValueItem(const profiler::ArbitraryValue& _value, profiler::thread_id_t _threadId);
    ~EasyArbitraryValueItem() override;

    void paint(QPainter* _painter, const QStyleOptionGraphicsItem* _option, QWidget* _widget = nullptr) override;

    void onScaleChanged(qreal _scale);

private:

    void onTimeout();

}; // end of class EasyArbitraryValueItem.

//////////////////////////////////////////////////////////////////////////

class EasyArbitraryValuesTreeItem : public QTreeWidgetItem
{
    using Parent = QTreeWidgetItem;
    using This = EasyArbitraryValuesTreeItem;

public:

    explicit EasyArbitraryValuesTreeItem(Parent* _parent = nullptr);
    ~EasyArbitraryValuesTreeItem() override;



}; // end of class EasyArbitraryValuesTreeItem.

class EasyArbitraryValuesWidget : public QWidget
{
    Q_OBJECT

    using Parent = QWidget;
    using This = EasyArbitraryValuesWidget;

    QTreeWidget* m_treeWidget;

public:

    explicit EasyArbitraryValuesWidget(QWidget* _parent = nullptr);
    ~EasyArbitraryValuesWidget() override;

private slots:

    void onSelectedThreadChanged(profiler::thread_id_t _id);
    void onSelectedBlockChanged(uint32_t _block_index);
    void onSelectedBlockIdChanged(profiler::block_id_t _id);

private:

    void buildTree(profiler::thread_id_t _threadId, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId);
    QTreeWidgetItem* buildTreeForThread(const profiler::BlocksTreeRoot& _threadTree, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId);

}; // end of class EasyArbitraryValuesWidget.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_GUI_ARBITRARY_VALUE_INSPECTOR_H
