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
#include <list>
#include <unordered_set>
#include "arbitrary_value_inspector.h"
#include "treeview_first_column_delegate.h"
#include "globals.h"

//////////////////////////////////////////////////////////////////////////

ArbitraryValuesCollection::ArbitraryValuesCollection()
{
    m_bReady = ATOMIC_VAR_INIT(false);
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

bool ArbitraryValuesCollection::ready() const
{
    return m_bReady.load(std::memory_order_acquire);
}

size_t ArbitraryValuesCollection::size() const
{
    size_t totalSize = 0;
    for (const auto& it : m_values)
        totalSize += it.second.size();
    return totalSize;
}

void ArbitraryValuesCollection::collectValues(profiler::thread_id_t _threadId, profiler::vin_t _valueId, const char* _valueName)
{
    using This = ArbitraryValuesCollection;

    interrupt();
    setReady(false);
    m_values.clear();

    if (_valueId == 0)
        m_collectorThread = std::thread(&This::collectByName, this, _threadId, _valueName);
    else
        m_collectorThread = std::thread(&This::collectById, this, _threadId, _valueId);
}

void ArbitraryValuesCollection::interrupt()
{
    if (!m_collectorThread.joinable())
        return;

    m_bInterrupt.store(true, std::memory_order_release);
    m_collectorThread.join();
    m_bInterrupt.store(false, std::memory_order_release);

    setReady(true);
    m_values.clear();
}

void ArbitraryValuesCollection::setReady(bool _ready)
{
    m_bReady.store(_ready, std::memory_order_release);
}

void ArbitraryValuesCollection::collectById(profiler::thread_id_t _threadId, profiler::vin_t _valueId)
{
    if (_threadId == 0)
    {
        for (const auto& it : EASY_GLOBALS.profiler_blocks)
        {
            if (!collectByIdForThread(it.second, _valueId))
                return;
        }
    }
    else
    {
        const auto t = EASY_GLOBALS.profiler_blocks.find(_threadId);
        if (t != EASY_GLOBALS.profiler_blocks.end() && !collectByIdForThread(t->second, _valueId))
            return;
    }

    setReady(true);
}

bool ArbitraryValuesCollection::collectByIdForThread(const profiler::BlocksTreeRoot& _threadRoot, profiler::vin_t _valueId)
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
    }

    return true;
}

void ArbitraryValuesCollection::collectByName(profiler::thread_id_t _threadId, const std::string _valueName)
{
    if (_threadId == 0)
    {
        for (const auto& it : EASY_GLOBALS.profiler_blocks)
        {
            if (!collectByNameForThread(it.second, _valueName))
                return;
        }
    }
    else
    {
        const auto t = EASY_GLOBALS.profiler_blocks.find(_threadId);
        if (t != EASY_GLOBALS.profiler_blocks.end() && !collectByNameForThread(t->second, _valueName))
            return;
    }

    setReady(true);
}

bool ArbitraryValuesCollection::collectByNameForThread(const profiler::BlocksTreeRoot& _threadRoot, const std::string& _valueName)
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
    }

    return true;
}

//////////////////////////////////////////////////////////////////////////

EasyArbitraryValueInspector::EasyArbitraryValueInspector(QWidget* _parent)
    : Parent(_parent)
{

}

EasyArbitraryValueInspector::~EasyArbitraryValueInspector()
{

}

//////////////////////////////////////////////////////////////////////////

EasyArbitraryValueItem::EasyArbitraryValueItem(const ::profiler::ArbitraryValue& _value, profiler::thread_id_t _threadId)
    : Parent(nullptr)
    , m_scale(1)
    , m_color(0)
{
    QObject::connect(&m_timer, &QTimer::timeout, [this] { onTimeout(); });
    m_collection.collectValues(_threadId, _value.value_id(), easyDescriptor(_value.id()).name());
    m_timer.start(40);
}

EasyArbitraryValueItem::~EasyArbitraryValueItem()
{

}

void EasyArbitraryValueItem::paint(QPainter* _painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    if (m_points.empty())
        return;

    _painter->save();

    auto pen = _painter->pen();
    pen.setColor(QColor::fromRgba(m_color));
    pen.setWidth(2);
    _painter->setPen(pen);

    if (m_points.size() == 1)
        _painter->drawPoint(m_points.front());
    else
        _painter->drawPolyline(m_points.data(), static_cast<int>(m_points.size()));

    _painter->restore();
}

void EasyArbitraryValueItem::onScaleChanged(qreal _scale)
{
    const auto k = _scale / m_scale;
    for (auto& p : m_points)
        p.setX(p.x() * k);
    m_scale = _scale;
}

void EasyArbitraryValueItem::onTimeout()
{
    if (!m_collection.ready())
        return;

    m_timer.stop();

    const auto size = m_collection.size();
    m_points.clear();
    m_points.reserve(size);

    if (size == 0)
        return;

    const auto& valuesByThread = m_collection.valuesMap();

    if (valuesByThread.size() == 1)
    {
        const auto& values = valuesByThread.begin()->second;

        m_color = 0;
        for (auto value : values)
        {
            if (m_color == 0)
                m_color = easyDescriptor(values.front()->id()).color();

            // TODO: calculate y
            const auto x = sceneX(value->begin());
            m_points.emplace_back(x, 0);
        }
    }
    else
    {
        std::list<profiler::thread_id_t> threadIds;
        for (const auto& it : valuesByThread)
            threadIds.push_back(it.first);

        m_color = 0;
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

                if (m_color == 0)
                    m_color = easyDescriptor(values.front()->id()).color();

                // TODO: calculate y
                const auto x = sceneX(values[i]->begin());
                m_points.emplace_back(x, 0);

                ++it;
            }
        }

        std::sort(m_points.begin(), m_points.end(), [](const QPointF& lhs, const QPointF& rhs) -> bool {
            return lhs.x() < rhs.x();
        });
    }

    scene()->update();
}

//////////////////////////////////////////////////////////////////////////

EasyArbitraryValuesTreeItem::EasyArbitraryValuesTreeItem(Parent* _parent)
    : Parent(_parent, QTreeWidgetItem::UserType)
{

}

EasyArbitraryValuesTreeItem::~EasyArbitraryValuesTreeItem()
{

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

inline EASY_CONSTEXPR_FCN int int_cast(ArbitraryColumns col) {
    return static_cast<int>(col);
}

EasyArbitraryValuesWidget::EasyArbitraryValuesWidget(QWidget* _parent)
    : Parent(_parent)
    , m_treeWidget(new QTreeWidget(this))
{
    m_treeWidget->setAutoFillBackground(false);
    m_treeWidget->setAlternatingRowColors(true);
    m_treeWidget->setItemsExpandable(true);
    m_treeWidget->setAnimated(true);
    //m_treeWidget->setSortingEnabled(false);
    m_treeWidget->setColumnCount(int_cast(ArbitraryColumns::Count));
    m_treeWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_treeWidget->setItemDelegateForColumn(0, new EasyTreeViewFirstColumnItemDelegate(this));

//    auto f = m_treeWidget->header()->font();
//    f.setBold(true);
//    m_treeWidget->header()->setFont(f);

    auto headerItem = new QTreeWidgetItem();
    headerItem->setText(int_cast(ArbitraryColumns::Name), "Name");
    headerItem->setText(int_cast(ArbitraryColumns::Type), "Type");
    headerItem->setText(int_cast(ArbitraryColumns::Value), "Value");
    headerItem->setText(int_cast(ArbitraryColumns::Vin), "ID");
    m_treeWidget->setHeaderItem(headerItem);

    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->addWidget(m_treeWidget);

    connect(&m_timer, &QTimer::timeout, this, &This::rebuild);
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChanged);
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChanged);
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedBlockIdChanged, this, &This::onSelectedBlockIdChanged);
}

EasyArbitraryValuesWidget::~EasyArbitraryValuesWidget()
{

}

void EasyArbitraryValuesWidget::clear()
{
    if (m_timer.isActive())
        m_timer.stop();
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

void EasyArbitraryValuesWidget::rebuild()
{
    clear();

    buildTree(EASY_GLOBALS.selected_thread, EASY_GLOBALS.selected_block, EASY_GLOBALS.selected_block_id);

    m_treeWidget->expandAll();
    for (int i = 0, columns = m_treeWidget->columnCount(); i < columns; ++i)
        m_treeWidget->resizeColumnToContents(i);
}

void EasyArbitraryValuesWidget::buildTree(profiler::thread_id_t _threadId, profiler::block_index_t _blockIndex, profiler::block_id_t _blockId)
{
    m_treeWidget->clear();

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
    //std::unordered_set<profiler::vin_t, profiler::passthrough_hash<profiler::vin_t> > vins;
    //std::unordered_set<std::string> names;

    auto rootItem = new QTreeWidgetItem(QTreeWidgetItem::UserType);
    rootItem->setText(0, profiler_gui::decoratedThreadName(EASY_GLOBALS.use_decorated_thread_name, _threadRoot, EASY_GLOBALS.hex_thread_id));

    if (_blockIndex != ::profiler_gui::numeric_max<decltype(_blockIndex)>())
    {
        const auto& block = easyBlocksTree(_blockIndex);

        auto blockItem = new QTreeWidgetItem(rootItem, QTreeWidgetItem::UserType);
        blockItem->setText(int_cast(ArbitraryColumns::Name), easyBlockName(block));

        for (auto childIndex : block.children)
        {
            const auto& child = easyBlocksTree(childIndex);
            const auto& desc = easyDescriptor(child.node->id());
            if (desc.type() == profiler::BlockType::Value)
            {
                auto vin = child.value->value_id();

//                if (vin == 0)
//                {
//                    auto result = names.insert(desc.name()).second;
//                    if (!result)
//                        continue; // already in set
//                }
//                else
//                {
//                    auto result = vins.insert(vin).second;
//                    if (!result)
//                        continue; // already in set
//                }

                auto valueItem = new QTreeWidgetItem(blockItem, QTreeWidgetItem::UserType);
                valueItem->setText(int_cast(ArbitraryColumns::Name), desc.name());
                valueItem->setText(int_cast(ArbitraryColumns::Type), profiler_gui::valueTypeString(*child.value));
                valueItem->setText(int_cast(ArbitraryColumns::Vin), QString("0x%1").arg(vin, 0, 16));
                valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*child.value));
            }
        }

        return rootItem;
    }

    if (_blockId == profiler_gui::numeric_max<decltype(_blockId)>())
        return rootItem;

    auto blockItem = new QTreeWidgetItem(rootItem, QTreeWidgetItem::UserType);
    blockItem->setText(int_cast(ArbitraryColumns::Name), easyDescriptor(_blockId).name());

    std::vector<profiler::block_index_t> stack;
    for (auto childIndex : _threadRoot.children)
    {
        stack.push_back(childIndex);
        while (!stack.empty())
        {
            const auto i = stack.back();
            stack.pop_back();

            const auto& block = easyBlocksTree(i);
            if (block.node->id() == _blockId || easyDescriptor(block.node->id()).id() == _blockId)
            {
                for (auto c : block.children)
                {
                    const auto& child = easyBlocksTree(c);
                    const auto& desc = easyDescriptor(child.node->id());
                    if (desc.type() == profiler::BlockType::Value)
                    {
                        auto vin = child.value->value_id();

//                        if (vin == 0)
//                        {
//                            auto result = names.insert(desc.name()).second;
//                            if (!result)
//                                continue; // already in set
//                        }
//                        else
//                        {
//                            auto result = vins.insert(vin).second;
//                            if (!result)
//                                continue; // already in set
//                        }

                        auto valueItem = new QTreeWidgetItem(blockItem, QTreeWidgetItem::UserType);
                        valueItem->setText(int_cast(ArbitraryColumns::Name), desc.name());
                        valueItem->setText(int_cast(ArbitraryColumns::Type), profiler_gui::valueTypeString(*child.value));
                        valueItem->setText(int_cast(ArbitraryColumns::Vin), QString("0x%1").arg(vin, 0, 16));
                        valueItem->setText(int_cast(ArbitraryColumns::Value), profiler_gui::valueString(*child.value));
                    }
                }
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

