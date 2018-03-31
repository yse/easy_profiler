/************************************************************************
* file name         : blocks_tree_widget.cpp
* ----------------- :
* creation time     : 2016/06/26
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of BlocksTreeWidget and it's auxiliary classes
*                   : for displyaing easy_profiler blocks tree.
* ----------------- :
* change log        : * 2016/06/26 Victor Zarubkin: Moved sources from tree_view.h
*                   :       and renamed classes from My* to Prof*.
*                   :
*                   : * 2016/06/27 Victor Zarubkin: Added possibility to colorize rows
*                   :       with profiler blocks' colors.
*                   :       Also added displaying frame statistics for blocks.
*                   :       Disabled sorting by name to save order of threads displayed on graphics view.
*                   :
*                   : * 2016/06/29 Victor Zarubkin: Added clearSilent() method.
*                   :
*                   : * 2016/08/18 Victor Zarubkin: Moved sources of TreeWidgetItem into tree_widget_item.h/.cpp
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2018  Sergey Yagovtsev, Victor Zarubkin
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

#include <QMenu>
#include <QAction>
#include <QActionGroup>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QSignalBlocker>
#include <QSettings>
#include <QProgressDialog>
#include <QResizeEvent>
#include <QMoveEvent>
#include <QLineEdit>
#include <QLabel>
#include <QToolBar>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QByteArray>
#include <QDebug>
#include <QApplication>
#include "blocks_tree_widget.h"
#include "arbitrary_value_tooltip.h"
#include "globals.h"
#include "thread_pool.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//////////////////////////////////////////////////////////////////////////

const int HIERARCHY_BUILDER_TIMER_INTERVAL = 40;

const bool SIMPLIFIED_REGIME_COLUMNS[COL_COLUMNS_NUMBER] = {
    true, //COL_NAME,
    true, //COL_BEGIN,
    true, //COL_DURATION,
    true, //COL_SELF_DURATION,
    false, //COL_DURATION_SUM_PER_PARENT,
    false, //COL_DURATION_SUM_PER_FRAME,
    true, //COL_DURATION_SUM_PER_THREAD,
    true, //COL_SELF_DURATION_PERCENT,
    false, //COL_PERCENT_PER_PARENT,
    true, //COL_PERCENT_PER_FRAME,
    false, //COL_PERCENT_SUM_PER_PARENT,
    false, //COL_PERCENT_SUM_PER_FRAME,
    true, //COL_PERCENT_SUM_PER_THREAD,
    true, //COL_END,
    true, //COL_MIN_PER_FRAME,
    true, //COL_MAX_PER_FRAME,
    true, //COL_AVERAGE_PER_FRAME,
    true, //COL_NCALLS_PER_FRAME,
    true, //COL_MIN_PER_THREAD,
    true, //COL_MAX_PER_THREAD,
    true, //COL_AVERAGE_PER_THREAD,
    true, //COL_NCALLS_PER_THREAD,
    false, //COL_MIN_PER_PARENT,
    false, //COL_MAX_PER_PARENT,
    false, //COL_AVERAGE_PER_PARENT,
    false, //COL_NCALLS_PER_PARENT,
    true, //COL_ACTIVE_TIME,
    true //COL_ACTIVE_PERCENT,
};

//////////////////////////////////////////////////////////////////////////

BlocksTreeWidget::BlocksTreeWidget(QWidget* _parent)
    : Parent(_parent)
    , m_beginTime(::std::numeric_limits<decltype(m_beginTime)>::max())
    , m_lastFound(nullptr)
    , m_progress(nullptr)
    , m_hintLabel(nullptr)
    , m_valueTooltip(nullptr)
    , m_mode(TreeMode::Plain)
    , m_bLocked(false)
    , m_bSilentExpandCollapse(false)
{
    installEventFilter(this);
    memset(m_columnsHiddenStatus, 0, sizeof(m_columnsHiddenStatus));

    setAutoFillBackground(false);
    setAlternatingRowColors(true);
    setItemsExpandable(true);
    setAnimated(true);
    setSortingEnabled(false);
    setColumnCount(COL_COLUMNS_NUMBER);
    setSelectionBehavior(QAbstractItemView::SelectRows);

    auto header_item = new QTreeWidgetItem();
    auto f = header()->font();
    f.setBold(true);
    header()->setFont(f);// profiler_gui::EFont("Helvetica", 9, QFont::Bold));

    header_item->setText(COL_NAME, "Name");

    header_item->setText(COL_BEGIN, "Begin, ms");

    header_item->setText(COL_DURATION, "Duration");
    header_item->setText(COL_SELF_DURATION, "Self dur.");
    //header_item->setToolTip(COL_SELF_DURATION, "");
    header_item->setText(COL_DURATION_SUM_PER_PARENT, "Total / Parent");
    header_item->setText(COL_DURATION_SUM_PER_FRAME,  "Total / Frame");
    header_item->setText(COL_DURATION_SUM_PER_THREAD, "Total / Thread");

    header_item->setText(COL_SELF_DURATION_PERCENT, "Self %");
    header_item->setText(COL_PERCENT_PER_PARENT, "% / Parent");
    header_item->setText(COL_PERCENT_PER_FRAME, "% / Frame");
    header_item->setText(COL_PERCENT_SUM_PER_FRAME,  "Sum % / Frame");
    header_item->setText(COL_PERCENT_SUM_PER_PARENT, "Sum % / Parent");
    header_item->setText(COL_PERCENT_SUM_PER_THREAD, "Sum % / Thread");

    header_item->setText(COL_END, "End, ms");

    header_item->setText(COL_MIN_PER_FRAME, "Min / Frame");
    header_item->setText(COL_MAX_PER_FRAME, "Max / Frame");
    header_item->setText(COL_AVERAGE_PER_FRAME, "Avg / Frame");
    header_item->setText(COL_NCALLS_PER_FRAME, "N Calls / Frame");

    header_item->setText(COL_MIN_PER_PARENT, "Min / Parent");
    header_item->setText(COL_MAX_PER_PARENT, "Max / Parent");
    header_item->setText(COL_AVERAGE_PER_PARENT, "Avg / Parent");
    header_item->setText(COL_NCALLS_PER_PARENT, "N Calls / Parent");

    header_item->setText(COL_MIN_PER_THREAD, "Min / Thread");
    header_item->setText(COL_MAX_PER_THREAD, "Max / Thread");
    header_item->setText(COL_AVERAGE_PER_THREAD, "Avg / Thread");
    header_item->setText(COL_NCALLS_PER_THREAD, "N Calls / Thread");

    header_item->setText(COL_ACTIVE_TIME, "Active time");
    header_item->setText(COL_ACTIVE_PERCENT, "Active %");

    auto color = QColor::fromRgb(profiler::colors::DeepOrange900);
    header_item->setForeground(COL_MIN_PER_THREAD, color);
    header_item->setForeground(COL_MAX_PER_THREAD, color);
    header_item->setForeground(COL_AVERAGE_PER_THREAD, color);
    header_item->setForeground(COL_NCALLS_PER_THREAD, color);
    header_item->setForeground(COL_PERCENT_SUM_PER_THREAD, color);
    header_item->setForeground(COL_DURATION_SUM_PER_THREAD, color);

    color = QColor::fromRgb(profiler::colors::Blue900);
    header_item->setForeground(COL_MIN_PER_FRAME, color);
    header_item->setForeground(COL_MAX_PER_FRAME, color);
    header_item->setForeground(COL_AVERAGE_PER_FRAME, color);
    header_item->setForeground(COL_NCALLS_PER_FRAME, color);
    header_item->setForeground(COL_PERCENT_SUM_PER_FRAME, color);
    header_item->setForeground(COL_DURATION_SUM_PER_FRAME, color);
    header_item->setForeground(COL_PERCENT_PER_FRAME, color);

    color = QColor::fromRgb(profiler::colors::Teal900);
    header_item->setForeground(COL_MIN_PER_PARENT, color);
    header_item->setForeground(COL_MAX_PER_PARENT, color);
    header_item->setForeground(COL_AVERAGE_PER_PARENT, color);
    header_item->setForeground(COL_NCALLS_PER_PARENT, color);
    header_item->setForeground(COL_PERCENT_SUM_PER_PARENT, color);
    header_item->setForeground(COL_DURATION_SUM_PER_PARENT, color);
    header_item->setForeground(COL_PERCENT_PER_PARENT, color);

    setHeaderItem(header_item);

    connect(&EASY_GLOBALS.events, &profiler_gui::GlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChange, Qt::QueuedConnection);
    connect(&EASY_GLOBALS.events, &profiler_gui::GlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange, Qt::QueuedConnection);
    connect(&m_fillTimer, &QTimer::timeout, this, &This::onFillTimerTimeout);
    connect(&m_idleTimer, &QTimer::timeout, this, &This::onIdleTimeout);
    m_idleTimer.setInterval(500);

    loadSettings();

    m_columnsHiddenStatus[0] = 0;
    setColumnHidden(0, false);

    if (m_mode == TreeMode::Full)
    {
        for (int i = 1; i < COL_COLUMNS_NUMBER; ++i)
            m_columnsHiddenStatus[i] = isColumnHidden(i) ? 1 : 0;
    }
    else
    {
        for (int i = 1; i < COL_COLUMNS_NUMBER; ++i)
        {
            if (SIMPLIFIED_REGIME_COLUMNS[i])
            {
                if (isColumnHidden(i))
                    m_columnsHiddenStatus[i] = 1;
            }
            else if (!isColumnHidden(i))
            {
                setColumnHidden(i, true);
            }
        }
    }

    m_hintLabel = new QLabel("Use Right Mouse Button on the Diagram to build a hierarchy...\nPress and hold, move, release", this);
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setStyleSheet("QLabel { color: gray; font: 12pt; }");

    QTimer::singleShot(1500, this, &This::alignProgressBar);

    setItemDelegateForColumn(0, new TreeWidgetItemDelegate(this));
}

BlocksTreeWidget::~BlocksTreeWidget()
{
    saveSettings();
    delete m_valueTooltip;
}

//////////////////////////////////////////////////////////////////////////

bool BlocksTreeWidget::eventFilter(QObject* _object, QEvent* _event)
{
    if (_object == this)
    {
        if (_event->type() == QEvent::MouseMove || _event->type() == QEvent::HoverMove)
        {
            if (m_idleTimer.isActive())
                m_idleTimer.stop();

            if (m_valueTooltip != nullptr)
            {
                const int size = std::min(m_valueTooltip->width(), m_valueTooltip->height()) >> 1;
                const auto rect = m_valueTooltip->rect().adjusted(-size, -size, size, size);
                const auto pos = m_valueTooltip->mapFromGlobal(QCursor::pos());

                if (rect.contains(pos))
                {
                    if (!m_valueTooltip->rect().contains(pos))
                        m_idleTimer.start();
                    return false;
                }

                delete m_valueTooltip;
                m_valueTooltip = nullptr;
            }

            m_idleTimer.start();
        }
    }

    return false;
}

void BlocksTreeWidget::mousePressEvent(QMouseEvent* _event)
{
    delete m_valueTooltip;
    m_valueTooltip = nullptr;

    if (m_idleTimer.isActive())
        m_idleTimer.stop();
    m_idleTimer.start();

    Parent::mousePressEvent(_event);
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::onFillTimerTimeout()
{
    if (m_hierarchyBuilder.done())
    {
        m_fillTimer.stop();

        ThreadedItems toplevelitems;
        m_hierarchyBuilder.takeItems(m_items);
        m_hierarchyBuilder.takeTopLevelItems(toplevelitems);
        m_hierarchyBuilder.interrupt();
        {
            const QSignalBlocker b(this);
            for (auto& item : toplevelitems)
            {
                addTopLevelItem(item.second);
                m_roots[item.first] = item.second;
            }
        }

        destroyProgressDialog();

        m_bLocked = false;
        m_inputBlocks.clear();

        setSortingEnabled(true);

        sortByColumn(COL_BEGIN, Qt::AscendingOrder); // sort by begin time
        if (m_mode == TreeMode::Plain) // and after that, sort by frame %
            sortByColumn(COL_PERCENT_PER_FRAME, Qt::DescendingOrder);

        //resizeColumnToContents(COL_NAME);
        resizeColumnsToContents();

        connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
        connect(this, &Parent::itemCollapsed, this, &This::onItemCollapse);
        connect(this, &Parent::currentItemChanged, this, &This::onCurrentItemChange);
        onSelectedThreadChange(EASY_GLOBALS.selected_thread);
        onSelectedBlockChange(EASY_GLOBALS.selected_block);
    }
    else if (m_progress != nullptr)
    {
        m_progress->setValue(m_hierarchyBuilder.progress());
    }
}

void BlocksTreeWidget::onIdleTimeout()
{
    if (m_idleTimer.isActive())
        m_idleTimer.stop();

    // Close old tooltip
    delete m_valueTooltip;
    m_valueTooltip = nullptr;

    const auto pos = viewport()->mapFromGlobal(QCursor::pos());
    auto itemUnderCursor = itemAt(pos);
    if (itemUnderCursor == nullptr)
        return;

    auto item = static_cast<TreeWidgetItem*>(itemUnderCursor);
    if (profiler_gui::is_max(item->block_index()))
        return;

    const auto& block = item->block();
    const auto& desc = easyDescriptor(block.node->id());
    if (desc.type() != profiler::BlockType::Value)
        return;

    const int column = columnAt(pos.x());
    if (item->hasToolTip(column))
        return;

    m_valueTooltip = new ArbitraryValueToolTip(itemUnderCursor->text(COL_NAME), block, this);
    m_valueTooltip->move(QCursor::pos());
    m_valueTooltip->show();

    // Actual size becomes valid only after show()
    m_valueTooltip->setFixedSize(m_valueTooltip->size());
}

void BlocksTreeWidget::setTree(const unsigned int _blocksNumber, const profiler::thread_blocks_tree_t& _blocksTree)
{
    clearSilent();

    if (!_blocksTree.empty())
    {
        m_bLocked = true;
        m_hintLabel->hide();
        createProgressDialog();
        m_hierarchyBuilder.fillTree(m_beginTime, _blocksNumber, _blocksTree, m_mode);
        m_fillTimer.start(HIERARCHY_BUILDER_TIMER_INTERVAL);
    }

    //StubLocker l;
    //ThreadedItems toplevelitems;
    //FillTreeClass<StubLocker>::setTreeInternal1(l, m_items, toplevelitems, m_beginTime, _blocksNumber, _blocksTree, m_bColorRows);
    //{
    //    const QSignalBlocker b(this);
    //    for (auto& item : toplevelitems)
    //    {
    //        addTopLevelItem(item.second);
    //        m_roots[item.first] = item.second;
    //        if (item.first == EASY_GLOBALS.selected_thread)
    //            item.second->setMain(true);
    //    }
    //}
}

void BlocksTreeWidget::setTreeBlocks(const profiler_gui::TreeBlocks& _blocks, profiler::timestamp_t _session_begin_time, profiler::timestamp_t _left, profiler::timestamp_t _right, bool _strict)
{
    clearSilent();

    m_beginTime = _session_begin_time;
    _left += m_beginTime;// - ::std::min(m_beginTime, 1000ULL);
    _right += m_beginTime;// + 1000;

    m_inputBlocks = _blocks;
    if (!m_inputBlocks.empty())
    {
        m_bLocked = true;
        m_hintLabel->hide();
        createProgressDialog();
        m_hierarchyBuilder.fillTreeBlocks(m_inputBlocks, _session_begin_time, _left, _right, _strict, m_mode);
        m_fillTimer.start(HIERARCHY_BUILDER_TIMER_INTERVAL);
    }

    //StubLocker l;
    //ThreadedItems toplevelitems;
    //FillTreeClass<StubLocker>::setTreeInternal2(l, m_items, toplevelitems, m_beginTime, _blocks, _left, _right, _strict, m_bColorRows);
    //{
    //    const QSignalBlocker b(this);
    //    for (auto& item : toplevelitems)
    //    {
    //        addTopLevelItem(item.second);
    //        m_roots[item.first] = item.second;
    //        if (item.first == EASY_GLOBALS.selected_thread)
    //            item.second->setMain(true);
    //    }
    //}

    //setSortingEnabled(true);
    //sortByColumn(COL_BEGIN, Qt::AscendingOrder);
    //resizeColumnToContents(COL_NAME);

    //connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
    //connect(this, &Parent::itemCollapsed, this, &This::onItemCollapse);
    //onSelectedBlockChange(EASY_GLOBALS.selected_block);
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::clearSilent(bool _global)
{
    const QSignalBlocker b(this);

    m_hierarchyBuilder.interrupt();
    destroyProgressDialog();
    m_hintLabel->show();

    m_bLocked = false;
    m_beginTime = ::std::numeric_limits<decltype(m_beginTime)>::max();

    setSortingEnabled(false);
    disconnect(this, &Parent::itemExpanded, this, &This::onItemExpand);
    disconnect(this, &Parent::itemCollapsed, this, &This::onItemCollapse);
    disconnect(this, &Parent::currentItemChanged, this, &This::onCurrentItemChange);
    m_lastFound = nullptr;
    m_lastSearch.clear();

    if (!_global)
    {
        if (EASY_GLOBALS.collapse_items_on_tree_close)
#ifdef EASY_TREE_WIDGET__USE_VECTOR
            for (auto item : m_items)
#else
            for (auto& item : m_items)
#endif
        {
#ifdef EASY_TREE_WIDGET__USE_VECTOR
            auto& gui_block = item->guiBlock();
            gui_block.expanded = false;
            profiler_gui::set_max(gui_block.tree_item);
#else
            item.second->guiBlock().expanded = false;
#endif
        }
#ifdef EASY_TREE_WIDGET__USE_VECTOR
        else for (auto item : m_items)
        {
            profiler_gui::set_max(item->guiBlock().tree_item);
        }
#endif
    }

    m_items.clear();
    m_roots.clear();

    if (topLevelItemCount() != 0)
    {
        std::vector<QTreeWidgetItem*> topLevelItems;
        topLevelItems.reserve(static_cast<size_t>(topLevelItemCount()));
        for (int i = topLevelItemCount() - 1; i >= 0; --i)
            topLevelItems.push_back(takeTopLevelItem(i));

        ThreadPool::instance().backgroundJob([=] {
            for (auto item : topLevelItems)
                delete item;
        });
    }

    //clear();

    if (!_global)
        emit EASY_GLOBALS.events.itemsExpandStateChanged();
}

//////////////////////////////////////////////////////////////////////////

int BlocksTreeWidget::findNext(const QString& _str, Qt::MatchFlags _flags)
{
    if (m_bLocked || _str.isEmpty())
        return 0;

    const bool isNewSearch = (m_lastSearch != _str);
    auto itemsList = findItems(_str, Qt::MatchContains | Qt::MatchRecursive | _flags, COL_NAME);

    if (!isNewSearch)
    {
        if (!itemsList.empty())
        {
            bool stop = false;
            decltype(m_lastFound) next = nullptr;
            for (auto item : itemsList)
            {
                if (item->parent() == nullptr)
                    continue;

                if (stop)
                {
                    next = item;
                    break;
                }

                stop = item == m_lastFound;
            }

            m_lastFound = next == nullptr ? itemsList.front() : next;
        }
        else
        {
            m_lastFound = nullptr;
        }
    }
    else
    {
        m_lastSearch = _str;
        m_lastFound = !itemsList.empty() ? itemsList.front() : nullptr;
    }

    if (m_lastFound != nullptr)
    {
        scrollToItem(m_lastFound, QAbstractItemView::PositionAtCenter);
        setCurrentItem(m_lastFound);
    }

    return itemsList.size();
}

int BlocksTreeWidget::findPrev(const QString& _str, Qt::MatchFlags _flags)
{
    if (m_bLocked || _str.isEmpty())
        return 0;

    const bool isNewSearch = (m_lastSearch != _str);
    auto itemsList = findItems(_str, Qt::MatchContains | Qt::MatchRecursive | _flags, COL_NAME);

    if (!isNewSearch)
    {
        if (!itemsList.empty())
        {
            decltype(m_lastFound) prev = nullptr;
            for (auto item : itemsList)
            {
                if (item->parent() == nullptr)
                    continue;

                if (item == m_lastFound)
                    break;

                prev = item;
            }

            m_lastFound = prev == nullptr ? itemsList.back() : prev;
        }
        else
        {
            m_lastFound = nullptr;
        }
    }
    else
    {
        m_lastSearch = _str;
        m_lastFound = !itemsList.empty() ? itemsList.front() : nullptr;
    }

    if (m_lastFound != nullptr)
    {
        scrollToItem(m_lastFound, QAbstractItemView::PositionAtCenter);
        setCurrentItem(m_lastFound);
    }

    return itemsList.size();
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::contextMenuEvent(QContextMenuEvent* _event)
{
    if (m_bLocked)
    {
        _event->accept();
        return;
    }

    const auto col = currentColumn();
    auto item = static_cast<TreeWidgetItem*>(currentItem());
    QMenu menu;
    menu.setToolTipsVisible(true);
    QAction* action = nullptr;

    if (!m_items.empty())
    {
        action = menu.addAction("Expand all");
        connect(action, &QAction::triggered, this, &This::onExpandAllClicked);
        action->setIcon(QIcon(imagePath("expand")));

        action = menu.addAction("Collapse all");
        connect(action, &QAction::triggered, this, &This::onCollapseAllClicked);
        action->setIcon(QIcon(imagePath("collapse")));

        if (item != nullptr && col >= 0)
        {
            menu.addSeparator();

            action = menu.addAction("Expand all children");
            connect(action, &QAction::triggered, this, &This::onExpandAllChildrenClicked);
            action->setIcon(QIcon(imagePath("expand")));

            action = menu.addAction("Collapse all children");
            connect(action, &QAction::triggered, this, &This::onCollapseAllChildrenClicked);
            action->setIcon(QIcon(imagePath("collapse")));
        }

        menu.addSeparator();
    }

    auto actionGroup = new QActionGroup(&menu);
    actionGroup->setExclusive(true);

    auto actionHierarchy = new QAction("Hierarchy mode", actionGroup);
    actionHierarchy->setCheckable(true);
    actionHierarchy->setChecked(m_mode == TreeMode::Full);
    actionHierarchy->setToolTip("Display full blocks hierarchy");
    actionHierarchy->setData((quint32)TreeMode::Full);
    menu.addAction(actionHierarchy);

    auto actionPlain = new QAction("Plain mode", actionGroup);
    actionPlain->setCheckable(true);
    actionPlain->setChecked(m_mode == TreeMode::Plain);
    actionPlain->setToolTip("Display plain list of blocks per frame.\nSome columns are disabled with this mode.");
    actionPlain->setData((quint32)TreeMode::Plain);
    menu.addAction(actionPlain);

    connect(actionHierarchy, &QAction::triggered, this, &This::onModeChange);
    connect(actionPlain, &QAction::triggered, this, &This::onModeChange);

    menu.addSeparator();

    if (item != nullptr && item->parent() != nullptr)
    {
        if (col >= 0)
        {
            switch (col)
            {
                case COL_MIN_PER_THREAD:
                case COL_MIN_PER_PARENT:
                case COL_MIN_PER_FRAME:
                case COL_MAX_PER_THREAD:
                case COL_MAX_PER_PARENT:
                case COL_MAX_PER_FRAME:
                {
                    auto& block = item->block();
                    auto i = profiler_gui::numeric_max<uint32_t>();
                    switch (col)
                    {
                        case COL_MIN_PER_THREAD: i = block.per_thread_stats->min_duration_block; break;
                        case COL_MIN_PER_PARENT: i = block.per_parent_stats->min_duration_block; break;
                        case COL_MIN_PER_FRAME: i = block.per_frame_stats->min_duration_block; break;
                        case COL_MAX_PER_THREAD: i = block.per_thread_stats->max_duration_block; break;
                        case COL_MAX_PER_PARENT: i = block.per_parent_stats->max_duration_block; break;
                        case COL_MAX_PER_FRAME: i = block.per_frame_stats->max_duration_block; break;
                    }

                    if (i != profiler_gui::numeric_max(i))
                    {
                        menu.addSeparator();
                        auto itemAction = new QAction("Jump to such item", nullptr);
                        itemAction->setData(i);
                        itemAction->setToolTip("Jump to item with min/max duration (depending on clicked column)");
                        connect(itemAction, &QAction::triggered, this, &This::onJumpToItemClicked);
                        menu.addAction(itemAction);
                    }

                    break;
                }

                default:
                    break;
            }
        }

        const auto& desc = easyDescriptor(item->block().node->id());
        auto submenu = menu.addMenu("Block status");
        submenu->setToolTipsVisible(true);

#define ADD_STATUS_ACTION(NameValue, StatusValue, ToolTipValue)\
        action = submenu->addAction(NameValue);\
        action->setCheckable(true);\
        action->setChecked(desc.status() == StatusValue);\
        action->setData(static_cast<quint32>(StatusValue));\
        action->setToolTip(ToolTipValue);\
        connect(action, &QAction::triggered, this, &This::onBlockStatusChangeClicked)

        ADD_STATUS_ACTION("Off", profiler::OFF, "Do not profile this block.");
        ADD_STATUS_ACTION("On", profiler::ON, "Profile this block\nif parent enabled children.");
        ADD_STATUS_ACTION("Force-On", profiler::FORCE_ON, "Always profile this block even\nif it's parent disabled children.");
        ADD_STATUS_ACTION("Off-recursive", profiler::OFF_RECURSIVE, "Do not profile neither this block\nnor it's children.");
        ADD_STATUS_ACTION("On-without-children", profiler::ON_WITHOUT_CHILDREN, "Profile this block, but\ndo not profile it's children.");
        ADD_STATUS_ACTION("Force-On-without-children", profiler::FORCE_ON_WITHOUT_CHILDREN, "Always profile this block, but\ndo not profile it's children.");
#undef ADD_STATUS_ACTION

        submenu->setEnabled(EASY_GLOBALS.connected);
        if (!EASY_GLOBALS.connected)
            submenu->setTitle(QString("%1 (connection needed)").arg(submenu->title()));
    }

    auto hidemenu = menu.addMenu("Select columns");
    auto hdr = headerItem();

    for (int i = 1; i < COL_COLUMNS_NUMBER; ++i)
    {
        auto columnAction = new QAction(hdr->text(i), nullptr);
        columnAction->setData(i);
        columnAction->setCheckable(true);
        columnAction->setChecked(m_columnsHiddenStatus[i] == 0);
        if ((m_mode == TreeMode::Full || SIMPLIFIED_REGIME_COLUMNS[i]))
            connect(columnAction, &QAction::triggered, this, &This::onHideShowColumn);
        else
            columnAction->setEnabled(false);
        hidemenu->addAction(columnAction);
    }

    menu.exec(QCursor::pos());

    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::resizeEvent(QResizeEvent* _event)
{
    Parent::resizeEvent(_event);
    alignProgressBar();
}

void BlocksTreeWidget::moveEvent(QMoveEvent* _event)
{
    Parent::moveEvent(_event);
    alignProgressBar();
}

void BlocksTreeWidget::alignProgressBar()
{
    auto center = rect().center();
    auto pos = mapToGlobal(center);

    if (m_progress != nullptr)
        m_progress->move(pos.x() - (m_progress->width() >> 1), pos.y() - (m_progress->height() >> 1));

    m_hintLabel->move(center.x() - (m_hintLabel->width() >> 1), std::max(center.y() - (m_hintLabel->height() >> 1), header()->height()));
}

void BlocksTreeWidget::destroyProgressDialog()
{
    if (m_progress != nullptr)
    {
        m_progress->setValue(100);
        m_progress->deleteLater();
        m_progress = nullptr;
    }
}

void BlocksTreeWidget::createProgressDialog()
{
    destroyProgressDialog();

    m_progress = new QProgressDialog("Building blocks hierarchy...", "", 0, 100, this, Qt::FramelessWindowHint);
    m_progress->setAttribute(Qt::WA_TranslucentBackground);
    m_progress->setCancelButton(nullptr);
    m_progress->setValue(0);
    m_progress->show();

    alignProgressBar();
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::onJumpToItemClicked(bool)
{
    auto action = qobject_cast<QAction*>(sender());
    if (action == nullptr)
        return;

    auto block_index = action->data().toUInt();
    EASY_GLOBALS.selected_block = block_index;
    if (block_index < EASY_GLOBALS.gui_blocks.size())
        EASY_GLOBALS.selected_block_id = easyBlock(block_index).tree.node->id();
    else
        profiler_gui::set_max(EASY_GLOBALS.selected_block_id);
    emit EASY_GLOBALS.events.selectedBlockChanged(block_index);
}

void BlocksTreeWidget::onCollapseAllClicked(bool)
{
    const QSignalBlocker b(this);

    m_bSilentExpandCollapse = true;
    collapseAll();
    m_bSilentExpandCollapse = false;

    if (EASY_GLOBALS.bind_scene_and_tree_expand_status)
    {
#ifdef EASY_TREE_WIDGET__USE_VECTOR
        for (auto item : m_items)
            item->guiBlock().expanded = false;
#else
        for (auto& item : m_items)
            item.second->guiBlock().expanded = false;
#endif
        emit EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void BlocksTreeWidget::onExpandAllClicked(bool)
{
    const QSignalBlocker blocker(this);

    m_bSilentExpandCollapse = true;
    expandAll();
    resizeColumnsToContents();
    m_bSilentExpandCollapse = false;

    if (EASY_GLOBALS.bind_scene_and_tree_expand_status)
    {
#ifdef EASY_TREE_WIDGET__USE_VECTOR
        for (auto item : m_items){
            auto& b = item->guiBlock();
#else
        for (auto& item : m_items){
            auto& b = item.second->guiBlock();
#endif
            b.expanded = !b.tree.children.empty();
        }

        emit EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void BlocksTreeWidget::onCollapseAllChildrenClicked(bool)
{
    auto current = static_cast<TreeWidgetItem*>(currentItem());
    if (current != nullptr)
    {
        const QSignalBlocker b(this);

        m_bSilentExpandCollapse = true;
        current->collapseAll();
        m_bSilentExpandCollapse = false;

        emit EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void BlocksTreeWidget::onExpandAllChildrenClicked(bool)
{
    auto current = static_cast<TreeWidgetItem*>(currentItem());
    if (current != nullptr)
    {
        const QSignalBlocker b(this);

        m_bSilentExpandCollapse = true;
        current->expandAll();
        resizeColumnsToContents();
        m_bSilentExpandCollapse = false;

        emit EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::onBlockStatusChangeClicked(bool _checked)
{
    if (!_checked)
        return;

    auto item = static_cast<TreeWidgetItem*>(currentItem());
    if (item == nullptr)
        return;

    auto action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        auto& desc = easyDescriptor(item->block().node->id());
        desc.setStatus(static_cast<profiler::EasyBlockStatus>(action->data().toUInt()));
        emit EASY_GLOBALS.events.blockStatusChanged(desc.id(), desc.status());
    }
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::onItemExpand(QTreeWidgetItem* _item)
{
    if (!EASY_GLOBALS.bind_scene_and_tree_expand_status || _item->parent() == nullptr)
    {
        resizeColumnsToContents();
        return;
    }

    static_cast<TreeWidgetItem*>(_item)->guiBlock().expanded = true;

    if (!m_bSilentExpandCollapse)
    {
        resizeColumnsToContents();
        emit EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void BlocksTreeWidget::onItemCollapse(QTreeWidgetItem* _item)
{
    if (!EASY_GLOBALS.bind_scene_and_tree_expand_status || _item->parent() == nullptr)
        return;

    static_cast<TreeWidgetItem*>(_item)->guiBlock().expanded = false;

    if (!m_bSilentExpandCollapse)
        emit EASY_GLOBALS.events.itemsExpandStateChanged();
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::onCurrentItemChange(QTreeWidgetItem* _item, QTreeWidgetItem* _previous)
{
    if (_previous != nullptr)
        static_cast<TreeWidgetItem*>(_previous)->setBold(false);

    if (_item == nullptr)
    {
        profiler_gui::set_max(EASY_GLOBALS.selected_block);
        profiler_gui::set_max(EASY_GLOBALS.selected_block_id);
    }
    else
    {
        auto item = static_cast<TreeWidgetItem*>(_item);
        item->setBold(true);

        EASY_GLOBALS.selected_block = item->block_index();
        if (EASY_GLOBALS.selected_block < EASY_GLOBALS.gui_blocks.size())
            EASY_GLOBALS.selected_block_id = easyBlock(EASY_GLOBALS.selected_block).tree.node->id();
        else
            profiler_gui::set_max(EASY_GLOBALS.selected_block_id);
    }

    disconnect(&EASY_GLOBALS.events, &profiler_gui::GlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange);
    emit EASY_GLOBALS.events.selectedBlockChanged(EASY_GLOBALS.selected_block);
    connect(&EASY_GLOBALS.events, &profiler_gui::GlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange);
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::onSelectedThreadChange(profiler::thread_id_t _id)
{
    for (auto& it : m_roots)
    {
        auto item = it.second;
        item->setMain(it.first == _id);
    }

    // Calling update() or repaint() (or both!) does not work even if setUpdatesEnabled(true) have been set in constructor.
    // Have to set focus to this widget to force update/repaint. :(
    // TODO: Find valid solution instead of this workaround.
    auto f = qApp->focusWidget();
    setFocus();
    if (f != nullptr)
        f->setFocus();
}

void BlocksTreeWidget::onSelectedBlockChange(uint32_t _block_index)
{
    disconnect(this, &Parent::currentItemChanged, this, &This::onCurrentItemChange);

    TreeWidgetItem* item = nullptr;

    if (_block_index < EASY_GLOBALS.gui_blocks.size())
    {
#ifdef EASY_TREE_WIDGET__USE_VECTOR
        const auto i = easyBlock(_block_index).tree_item;
        if (i < m_items.size())
            item = m_items[i];
#else
        auto it = m_items.find(_block_index);
        if (it != m_items.end())
            item = it->second;
#endif
    }

    auto previous = static_cast<TreeWidgetItem*>(currentItem());
    if (previous != nullptr)
        previous->setBold(false);

    if (item != nullptr)
    {
        //const QSignalBlocker b(this);

        if (EASY_GLOBALS.bind_scene_and_tree_expand_status)
        {
            m_bSilentExpandCollapse = true;
            setCurrentItem(item);
            scrollToItem(item, QAbstractItemView::PositionAtCenter);
            if (item->guiBlock().expanded)
                expandItem(item);
            else
                collapseItem(item);
            resizeColumnsToContents();
            m_bSilentExpandCollapse = false;

            emit EASY_GLOBALS.events.itemsExpandStateChanged();
        }
        else
        {
            disconnect(this, &Parent::itemExpanded, this, &This::onItemExpand);
            setCurrentItem(item);
            scrollToItem(item, QAbstractItemView::PositionAtCenter);
            resizeColumnsToContents();
            connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
        }

        item->setBold(true);
    }
    else
    {
        setCurrentItem(item);
    }

    connect(this, &Parent::currentItemChanged, this, &This::onCurrentItemChange);
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::resizeColumnsToContents()
{
    for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
    {
        resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::onHideShowColumn(bool)
{
    auto action = qobject_cast<QAction*>(sender());
    if (action == nullptr)
        return;

    const auto col = action->data().toInt();
    const bool hideCol = m_columnsHiddenStatus[col] == 0;
    setColumnHidden(col, hideCol);
    m_columnsHiddenStatus[col] = hideCol ? 1 : 0;
}

void BlocksTreeWidget::onModeChange(bool)
{
    auto action = qobject_cast<QAction*>(sender());
    if (action == nullptr)
        return;

    const auto prev = m_mode;
    m_mode = static_cast<TreeMode>(action->data().toUInt());

    if (m_mode == prev)
        return;

    if (m_mode == TreeMode::Full)
    {
        for (int i = 1; i < COL_COLUMNS_NUMBER; ++i)
            setColumnHidden(i, m_columnsHiddenStatus[i] != 0);
    }
    else
    {
        for (int i = 1; i < COL_COLUMNS_NUMBER; ++i)
            setColumnHidden(i, m_columnsHiddenStatus[i] != 0 || !SIMPLIFIED_REGIME_COLUMNS[i]);
    }

    emit EASY_GLOBALS.events.blocksTreeModeChanged();
}

//////////////////////////////////////////////////////////////////////////

void BlocksTreeWidget::loadSettings()
{
    QSettings settings(profiler_gui::ORGANAZATION_NAME, profiler_gui::APPLICATION_NAME);
    settings.beginGroup("tree_widget");

    auto val = settings.value("regime");
    if (!val.isNull())
        m_mode = static_cast<TreeMode>(val.toUInt());

    val = settings.value("columns");
    if (!val.isNull())
    {
        auto byteArray = val.toByteArray();
        memcpy(m_columnsHiddenStatus, byteArray.constData(), ::std::min(sizeof(m_columnsHiddenStatus), (size_t)byteArray.size()));
    }

    auto state = settings.value("headerState").toByteArray();
    if (!state.isEmpty())
        header()->restoreState(state);

    settings.endGroup();
}

void BlocksTreeWidget::saveSettings()
{
    QSettings settings(profiler_gui::ORGANAZATION_NAME, profiler_gui::APPLICATION_NAME);
    settings.beginGroup("tree_widget");
    settings.setValue("regime", static_cast<uint8_t>(m_mode));
    settings.setValue("columns", QByteArray(m_columnsHiddenStatus, COL_COLUMNS_NUMBER));
    settings.setValue("headerState", header()->saveState());
    settings.endGroup();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

HierarchyWidget::HierarchyWidget(QWidget* _parent) : Parent(_parent)
    , m_tree(new BlocksTreeWidget(this))
    , m_searchBox(new QLineEdit(this))
    , m_foundNumber(new QLabel("0 matches", this))
    , m_searchButton(nullptr)
    , m_bCaseSensitiveSearch(false)
{
    loadSettings();

    m_searchBox->setFixedWidth(300);
    m_searchBox->setContentsMargins(5, 0, 0, 0);
    m_searchBox->setClearButtonEnabled(true);
    m_searchBox->setPlaceholderText("Search by name");

    auto menu = new QMenu(this);
    m_searchButton = menu->menuAction();
    m_searchButton->setText("Find next");
    m_searchButton->setIcon(QIcon(imagePath("find-next")));
    m_searchButton->setData(true);
    connect(m_searchButton, &QAction::triggered, this, &This::findNext);

    auto actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    auto a = new QAction(tr("Find next"), actionGroup);
    a->setCheckable(true);
    a->setChecked(true);
    connect(a, &QAction::triggered, this, &This::findNextFromMenu);
    menu->addAction(a);

    a = new QAction(tr("Find previous"), actionGroup);
    a->setCheckable(true);
    connect(a, &QAction::triggered, this, &This::findPrevFromMenu);
    menu->addAction(a);

    a = menu->addAction("Case sensitive");
    a->setCheckable(true);
    a->setChecked(m_bCaseSensitiveSearch);
    connect(a, &QAction::triggered, [this](bool _checked){ m_bCaseSensitiveSearch = _checked; });
    menu->addAction(a);

    auto tb = new QToolBar(this);
    tb->setIconSize(applicationIconsSize());
    tb->setContentsMargins(0, 0, 0, 0);
    tb->addAction(m_searchButton);
    tb->addWidget(m_searchBox);

    auto searchbox = new QHBoxLayout();
    searchbox->setContentsMargins(0, 0, 5, 0);
    searchbox->addWidget(tb);
    searchbox->addSpacing(5);
    searchbox->addWidget(m_foundNumber);
    searchbox->addStretch(100);

    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(1, 1, 1, 1);
    lay->addLayout(searchbox);
    lay->addWidget(m_tree);

    connect(m_searchBox, &QLineEdit::returnPressed, this, &This::onSeachBoxReturnPressed);
    connect(m_searchBox, &QLineEdit::textChanged, this, &This::onSearchBoxTextChanged);

    connect(&EASY_GLOBALS.events, &profiler_gui::GlobalSignals::allDataGoingToBeDeleted, [this] {
        clear(true);
    });

    m_foundNumber->hide();
}

HierarchyWidget::~HierarchyWidget()
{
    saveSettings();
}

void HierarchyWidget::loadSettings()
{
    QSettings settings(profiler_gui::ORGANAZATION_NAME, profiler_gui::APPLICATION_NAME);
    settings.beginGroup("HierarchyWidget");

    auto val = settings.value("case_sensitive");
    if (!val.isNull())
        m_bCaseSensitiveSearch = val.toBool();

    settings.endGroup();
}

void HierarchyWidget::saveSettings()
{
    QSettings settings(profiler_gui::ORGANAZATION_NAME, profiler_gui::APPLICATION_NAME);
    settings.beginGroup("HierarchyWidget");
    settings.setValue("case_sensitive", m_bCaseSensitiveSearch);
    settings.endGroup();
}

void HierarchyWidget::keyPressEvent(QKeyEvent* _event)
{
    if (_event->key() == Qt::Key_F3)
    {
        if (_event->modifiers() & Qt::ShiftModifier)
            findPrev(true);
        else
            findNext(true);
    }

    _event->accept();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void HierarchyWidget::contextMenuEvent(QContextMenuEvent* _event)
{
    m_tree->contextMenuEvent(_event);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

BlocksTreeWidget* HierarchyWidget::tree()
{
    return m_tree;
}

void HierarchyWidget::clear(bool _global)
{
    m_tree->clearSilent(_global);
    m_foundNumber->setText(QString("0 matches"));
    m_foundNumber->hide();
}

void HierarchyWidget::onSeachBoxReturnPressed()
{
    if (m_searchButton->data().toBool() == true)
        findNext(true);
    else
        findPrev(true);
}

void HierarchyWidget::onSearchBoxTextChanged(const QString& _text)
{
    if (_text.isEmpty())
        m_foundNumber->hide();
}

void HierarchyWidget::findNext(bool)
{
    auto text = m_searchBox->text();
    if (text.isEmpty())
    {
        if (m_foundNumber->isVisible())
            m_foundNumber->hide();
        return;
    }

    auto matches = m_tree->findNext(text, m_bCaseSensitiveSearch ? Qt::MatchCaseSensitive : Qt::MatchFlags());

    if (matches == 1)
        m_foundNumber->setText(QString("1 match"));
    else
        m_foundNumber->setText(QString("%1 matches").arg(matches));

    if (!m_foundNumber->isVisible())
        m_foundNumber->show();
}

void HierarchyWidget::findPrev(bool)
{
    auto text = m_searchBox->text();
    if (text.isEmpty())
    {
        if (m_foundNumber->isVisible())
            m_foundNumber->hide();
        return;
    }

    auto matches = m_tree->findPrev(text, m_bCaseSensitiveSearch ? Qt::MatchCaseSensitive : Qt::MatchFlags());

    if (matches == 1)
        m_foundNumber->setText(QString("1 match"));
    else
        m_foundNumber->setText(QString("%1 matches").arg(matches));

    if (!m_foundNumber->isVisible())
        m_foundNumber->show();
}

void HierarchyWidget::findNextFromMenu(bool _checked)
{
    if (!_checked)
        return;

    if (m_searchButton->data().toBool() == false)
    {
        m_searchButton->setData(true);
        m_searchButton->setText(tr("Find next"));
        m_searchButton->setIcon(QIcon(imagePath("find-next")));
        disconnect(m_searchButton, &QAction::triggered, this, &This::findPrev);
        connect(m_searchButton, &QAction::triggered, this, &This::findNext);
    }

    findNext(true);
}

void HierarchyWidget::findPrevFromMenu(bool _checked)
{
    if (!_checked)
        return;

    if (m_searchButton->data().toBool() == true)
    {
        m_searchButton->setData(false);
        m_searchButton->setText(tr("Find prev"));
        m_searchButton->setIcon(QIcon(imagePath("find-prev")));
        disconnect(m_searchButton, &QAction::triggered, this, &This::findNext);
        connect(m_searchButton, &QAction::triggered, this, &This::findPrev);
    }

    findPrev(true);
}

//////////////////////////////////////////////////////////////////////////
