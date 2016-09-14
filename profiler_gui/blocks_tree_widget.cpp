/************************************************************************
* file name         : blocks_tree_widget.cpp
* ----------------- :
* creation time     : 2016/06/26
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of EasyTreeWidget and it's auxiliary classes
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
*                   : Copyright(C) 2016  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : This program is free software : you can redistribute it and / or modify
*                   : it under the terms of the GNU General Public License as published by
*                   : the Free Software Foundation, either version 3 of the License, or
*                   : (at your option) any later version.
*                   :
*                   : This program is distributed in the hope that it will be useful,
*                   : but WITHOUT ANY WARRANTY; without even the implied warranty of
*                   : MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
*                   : GNU General Public License for more details.
*                   :
*                   : You should have received a copy of the GNU General Public License
*                   : along with this program.If not, see <http://www.gnu.org/licenses/>.
************************************************************************/

#include <QMenu>
#include <QHeaderView>
#include <QContextMenuEvent>
#include <QSignalBlocker>
#include <QSettings>
#include <QProgressDialog>
#include <QResizeEvent>
#include <QMoveEvent>
#include <QDebug>
#include "blocks_tree_widget.h"
#include "tree_widget_item.h"
#include "globals.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//////////////////////////////////////////////////////////////////////////

const int HIERARCHY_BUILDER_TIMER_INTERVAL = 40;

//////////////////////////////////////////////////////////////////////////

EasyTreeWidget::EasyTreeWidget(QWidget* _parent)
    : Parent(_parent)
    , m_beginTime(::std::numeric_limits<decltype(m_beginTime)>::max())
    , m_progress(nullptr)
    , m_bColorRows(true)
    , m_bLocked(false)
    , m_bSilentExpandCollapse(false)
{
    setAutoFillBackground(false);
    setAlternatingRowColors(true);
    setItemsExpandable(true);
    setAnimated(true);
    setSortingEnabled(false);
    setColumnCount(COL_COLUMNS_NUMBER);

    auto header_item = new QTreeWidgetItem();
    auto f = header()->font();
    f.setBold(true);
    header()->setFont(f);// ::profiler_gui::EFont("Helvetica", 9, QFont::Bold));

    header_item->setText(COL_NAME, "Name");

    header_item->setText(COL_BEGIN, "Begin, ms");

    header_item->setText(COL_DURATION, "Duration");
    header_item->setText(COL_SELF_DURATION, "Self Dur.");
    //header_item->setToolTip(COL_SELF_DURATION, "");
    header_item->setText(COL_DURATION_SUM_PER_PARENT, "Tot. Dur./Parent");
    header_item->setText(COL_DURATION_SUM_PER_FRAME, "Tot. Dur./Frame");
    header_item->setText(COL_DURATION_SUM_PER_THREAD, "Tot. Dur./Thread");

    header_item->setText(COL_SELF_DURATION_PERCENT, "Self %");
    header_item->setText(COL_PERCENT_PER_PARENT, "% / Parent");
    header_item->setText(COL_PERCENT_PER_FRAME, "% / Frame");
    header_item->setText(COL_PERCENT_SUM_PER_FRAME, "Tot. % / Frame");
    header_item->setText(COL_PERCENT_SUM_PER_PARENT, "Tot. % / Parent");
    header_item->setText(COL_PERCENT_SUM_PER_THREAD, "Tot. % / Thread");

    header_item->setText(COL_END, "End, ms");

    header_item->setText(COL_MIN_PER_FRAME, "Min dur./Frame");
    header_item->setText(COL_MAX_PER_FRAME, "Max dur./Frame");
    header_item->setText(COL_AVERAGE_PER_FRAME, "Average dur./Frame");
    header_item->setText(COL_NCALLS_PER_FRAME, "N Calls/Frame");

    header_item->setText(COL_MIN_PER_PARENT, "Min dur./Parent");
    header_item->setText(COL_MAX_PER_PARENT, "Max dur./Parent");
    header_item->setText(COL_AVERAGE_PER_PARENT, "Average dur./Parent");
    header_item->setText(COL_NCALLS_PER_PARENT, "N Calls/Parent");

    header_item->setText(COL_MIN_PER_THREAD, "Min dur./Thread");
    header_item->setText(COL_MAX_PER_THREAD, "Max dur./Thread");
    header_item->setText(COL_AVERAGE_PER_THREAD, "Average dur./Thread");
    header_item->setText(COL_NCALLS_PER_THREAD, "N Calls/Thread");

    auto color = QColor::fromRgb(::profiler::colors::DeepOrange900);
    header_item->setForeground(COL_MIN_PER_THREAD, color);
    header_item->setForeground(COL_MAX_PER_THREAD, color);
    header_item->setForeground(COL_AVERAGE_PER_THREAD, color);
    header_item->setForeground(COL_NCALLS_PER_THREAD, color);
    header_item->setForeground(COL_PERCENT_SUM_PER_THREAD, color);
    header_item->setForeground(COL_DURATION_SUM_PER_THREAD, color);

    color = QColor::fromRgb(::profiler::colors::Blue900);
    header_item->setForeground(COL_MIN_PER_FRAME, color);
    header_item->setForeground(COL_MAX_PER_FRAME, color);
    header_item->setForeground(COL_AVERAGE_PER_FRAME, color);
    header_item->setForeground(COL_NCALLS_PER_FRAME, color);
    header_item->setForeground(COL_PERCENT_SUM_PER_FRAME, color);
    header_item->setForeground(COL_DURATION_SUM_PER_FRAME, color);
    header_item->setForeground(COL_PERCENT_PER_FRAME, color);

    color = QColor::fromRgb(::profiler::colors::Teal900);
    header_item->setForeground(COL_MIN_PER_PARENT, color);
    header_item->setForeground(COL_MAX_PER_PARENT, color);
    header_item->setForeground(COL_AVERAGE_PER_PARENT, color);
    header_item->setForeground(COL_NCALLS_PER_PARENT, color);
    header_item->setForeground(COL_PERCENT_SUM_PER_PARENT, color);
    header_item->setForeground(COL_DURATION_SUM_PER_PARENT, color);
    header_item->setForeground(COL_PERCENT_PER_PARENT, color);

    setHeaderItem(header_item);

    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChange);
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange);
    connect(&m_fillTimer, &QTimer::timeout, this, &This::onFillTimerTimeout);

    loadSettings();

    m_progress = new QProgressDialog("Building blocks hierarchy...", "", 0, 100, this, Qt::FramelessWindowHint);
    m_progress->setAttribute(Qt::WA_TranslucentBackground);
    m_progress->setCancelButton(nullptr);
    m_progress->setValue(100);
    //m_progress->hide();

    QTimer::singleShot(1500, this, &This::alignProgressBar);
}

EasyTreeWidget::~EasyTreeWidget()
{
    saveSettings();
    delete m_progress;
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::onFillTimerTimeout()
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

        if (m_progress)
        {
            m_progress->setValue(100);
            //m_progress->hide();
        }

        m_bLocked = false;
        m_inputBlocks.clear();

        setSortingEnabled(true);
        sortByColumn(COL_BEGIN, Qt::AscendingOrder);
        //resizeColumnToContents(COL_NAME);
        resizeColumnsToContents();

        connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
        connect(this, &Parent::itemCollapsed, this, &This::onItemCollapse);
        connect(this, &Parent::currentItemChanged, this, &This::onCurrentItemChange);
        onSelectedThreadChange(EASY_GLOBALS.selected_thread);
        onSelectedBlockChange(EASY_GLOBALS.selected_block);
    }
    else
    {
        m_progress->setValue(m_hierarchyBuilder.progress());
    }
}

void EasyTreeWidget::setTree(const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree)
{
    clearSilent();

    if (!_blocksTree.empty())
    {
        m_bLocked = true;
        m_progress->setValue(0);
        m_progress->show();
        m_hierarchyBuilder.fillTree(m_beginTime, _blocksNumber, _blocksTree, m_bColorRows);
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
    //            item.second->colorize(true);
    //    }
    //}
}

void EasyTreeWidget::setTreeBlocks(const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _session_begin_time, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict)
{
    clearSilent();

    m_beginTime = _session_begin_time;
    _left += m_beginTime;// - ::std::min(m_beginTime, 1000ULL);
    _right += m_beginTime;// + 1000;

    m_inputBlocks = _blocks;
    if (!m_inputBlocks.empty())
    {
        m_bLocked = true;
        m_progress->setValue(0);
        m_progress->show();
        m_hierarchyBuilder.fillTreeBlocks(m_inputBlocks, _session_begin_time, _left, _right, _strict, m_bColorRows);
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
    //            item.second->colorize(true);
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

void EasyTreeWidget::clearSilent(bool _global)
{
    const QSignalBlocker b(this);

    m_hierarchyBuilder.interrupt();

    if (m_progress)
    {
        m_progress->setValue(100);
        //m_progress->hide();
    }

    m_bLocked = false;
    m_beginTime = ::std::numeric_limits<decltype(m_beginTime)>::max();

    setSortingEnabled(false);
    disconnect(this, &Parent::itemExpanded, this, &This::onItemExpand);
    disconnect(this, &Parent::itemCollapsed, this, &This::onItemCollapse);
    disconnect(this, &Parent::currentItemChanged, this, &This::onCurrentItemChange);

    if (!_global)
    {
        if (EASY_GLOBALS.collapse_items_on_tree_close) for (auto item : m_items)
        {
            auto& gui_block = item->guiBlock();
            ::profiler_gui::set_max(gui_block.tree_item);
            gui_block.expanded = false;
        }
        else for (auto item : m_items)
        {
            ::profiler_gui::set_max(item->guiBlock().tree_item);
        }
    }

    m_items.clear();
    m_roots.clear();

    ::std::vector<QTreeWidgetItem*> topLevelItems;
    topLevelItems.reserve(topLevelItemCount());
    for (int i = topLevelItemCount() - 1; i >= 0; --i)
        topLevelItems.push_back(takeTopLevelItem(i));

    auto deleter_thread = ::std::thread([](decltype(topLevelItems) _items) {
        for (auto item : _items)
            delete item;
    }, ::std::move(topLevelItems));
    deleter_thread.detach();

    //clear();

    if (!_global)
        emit EASY_GLOBALS.events.itemsExpandStateChanged();
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::contextMenuEvent(QContextMenuEvent* _event)
{
    if (m_bLocked)
    {
        _event->accept();
        return;
    }

    const auto col = currentColumn();
    auto item = static_cast<EasyTreeWidgetItem*>(currentItem());
    QMenu menu;
    QAction* action = nullptr;

    if (!m_items.empty())
    {
        action = menu.addAction("Expand all");
        connect(action, &QAction::triggered, this, &This::onExpandAllClicked);
        { QIcon icon(":/Expand"); if (!icon.isNull()) action->setIcon(icon); }

        action = menu.addAction("Collapse all");
        connect(action, &QAction::triggered, this, &This::onCollapseAllClicked);
        { QIcon icon(":/Collapse"); if (!icon.isNull()) action->setIcon(icon); }

        if (item != nullptr && col >= 0)
        {
            menu.addSeparator();

            action = menu.addAction("Expand all children");
            connect(action, &QAction::triggered, this, &This::onExpandAllChildrenClicked);
            { QIcon icon(":/Expand"); if (!icon.isNull()) action->setIcon(icon); }

            action = menu.addAction("Collapse all children");
            connect(action, &QAction::triggered, this, &This::onCollapseAllChildrenClicked);
            { QIcon icon(":/Collapse"); if (!icon.isNull()) action->setIcon(icon); }
        }

        menu.addSeparator();
    }

    action = menu.addAction("Color rows");
    action->setCheckable(true);
    action->setChecked(m_bColorRows);
    connect(action, &QAction::triggered, this, &This::onColorizeRowsTriggered);
    if (m_bColorRows) {
        auto f = action->font();
        f.setBold(true);
        action->setFont(f);
        { QIcon icon(":/Color"); if (!icon.isNull()) action->setIcon(icon); }
    }
    else { QIcon icon(":/NoColor"); if (!icon.isNull()) action->setIcon(icon); }

    if (item != nullptr && item->parent() != nullptr)
    {
        //auto itemAction = new EasyItemAction("Show this item on scene", item->block()->block_index);
        //itemAction->setToolTip("Scroll graphics scene to current item in the tree");
        //connect(itemAction, &EasyItemAction::clicked, this, &This::onJumpToItemClicked);
        //menu.addAction(itemAction);

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
                    auto i = ::profiler_gui::numeric_max<uint32_t>();
                    switch (col)
                    {
                        case COL_MIN_PER_THREAD: i = block.per_thread_stats->min_duration_block; break;
                        case COL_MIN_PER_PARENT: i = block.per_parent_stats->min_duration_block; break;
                        case COL_MIN_PER_FRAME: i = block.per_frame_stats->min_duration_block; break;
                        case COL_MAX_PER_THREAD: i = block.per_thread_stats->max_duration_block; break;
                        case COL_MAX_PER_PARENT: i = block.per_parent_stats->max_duration_block; break;
                        case COL_MAX_PER_FRAME: i = block.per_frame_stats->max_duration_block; break;
                    }

                    if (i != ::profiler_gui::numeric_max(i))
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
            }
        }
    }

    menu.addSeparator();

    auto hidemenu = menu.addMenu("Select columns");
    auto hdr = headerItem();

    for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
    {
        auto columnAction = new QAction(hdr->text(i), nullptr);
        columnAction->setData(i);
        columnAction->setCheckable(true);
        columnAction->setChecked(!isColumnHidden(i));
        connect(columnAction, &QAction::triggered, this, &This::onHideShowColumn);
        hidemenu->addAction(columnAction);
    }

    menu.exec(QCursor::pos());

    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::resizeEvent(QResizeEvent* _event)
{
    Parent::resizeEvent(_event);
    alignProgressBar();
}

void EasyTreeWidget::moveEvent(QMoveEvent* _event)
{
    Parent::moveEvent(_event);
    alignProgressBar();
}

void EasyTreeWidget::alignProgressBar()
{
    auto pos = mapToGlobal(rect().center());
    m_progress->move(pos.x() - (m_progress->width() >> 1), pos.y() - (m_progress->height() >> 1));
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::onJumpToItemClicked(bool)
{
    auto action = qobject_cast<QAction*>(sender());
    if (action == nullptr)
        return;

    auto block_index = action->data().toUInt();
    EASY_GLOBALS.selected_block = block_index;
    emit EASY_GLOBALS.events.selectedBlockChanged(block_index);
}

void EasyTreeWidget::onCollapseAllClicked(bool)
{
    const QSignalBlocker b(this);

    m_bSilentExpandCollapse = true;
    collapseAll();
    m_bSilentExpandCollapse = false;

    if (EASY_GLOBALS.bind_scene_and_tree_expand_status)
    {
        for (auto item : m_items)
            item->guiBlock().expanded = false;
        emit EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void EasyTreeWidget::onExpandAllClicked(bool)
{
    const QSignalBlocker b(this);

    m_bSilentExpandCollapse = true;
    expandAll();
    resizeColumnsToContents();
    m_bSilentExpandCollapse = false;

    if (EASY_GLOBALS.bind_scene_and_tree_expand_status)
    {
        for (auto item : m_items)
        {
            auto& b = item->guiBlock();
            b.expanded = !b.tree.children.empty();
        }

        emit EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void EasyTreeWidget::onCollapseAllChildrenClicked(bool)
{
    auto current = static_cast<EasyTreeWidgetItem*>(currentItem());
    if (current != nullptr)
    {
        const QSignalBlocker b(this);

        m_bSilentExpandCollapse = true;
        current->collapseAll();
        m_bSilentExpandCollapse = false;

        emit EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void EasyTreeWidget::onExpandAllChildrenClicked(bool)
{
    auto current = static_cast<EasyTreeWidgetItem*>(currentItem());
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

void EasyTreeWidget::onItemExpand(QTreeWidgetItem* _item)
{
    if (!EASY_GLOBALS.bind_scene_and_tree_expand_status)
    {
        resizeColumnsToContents();
        return;
    }

    static_cast<EasyTreeWidgetItem*>(_item)->guiBlock().expanded = true;

    if (!m_bSilentExpandCollapse)
    {
        resizeColumnsToContents();
        emit EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void EasyTreeWidget::onItemCollapse(QTreeWidgetItem* _item)
{
    if (!EASY_GLOBALS.bind_scene_and_tree_expand_status)
        return;

    static_cast<EasyTreeWidgetItem*>(_item)->guiBlock().expanded = false;

    if (!m_bSilentExpandCollapse)
        emit EASY_GLOBALS.events.itemsExpandStateChanged();
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::onCurrentItemChange(QTreeWidgetItem* _item, QTreeWidgetItem*)
{
    if (_item == nullptr)
        ::profiler_gui::set_max(EASY_GLOBALS.selected_block);
    else
        EASY_GLOBALS.selected_block = static_cast<EasyTreeWidgetItem*>(_item)->block_index();

    disconnect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange);
    emit EASY_GLOBALS.events.selectedBlockChanged(EASY_GLOBALS.selected_block);
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange);
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::onColorizeRowsTriggered(bool _colorize)
{
    const QSignalBlocker b(this);

    m_bColorRows = _colorize;

    auto current = currentItem();
    collapseAll(); // Without collapseAll() changing items process is VERY VERY SLOW.
    // TODO: Find the reason of such behavior. QSignalBlocker(this) does not help. QSignalBlocker(item) does not work, because items are not inherited from QObject.

    for (auto item : m_items)
    {
        if (item->parent() != nullptr)
            item->colorize(m_bColorRows);
    }

    // Scroll back to previously selected item
    if (current)
    {
        scrollToItem(current, QAbstractItemView::PositionAtCenter);
        setCurrentItem(current);
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::onSelectedThreadChange(::profiler::thread_id_t _id)
{
    for (auto& it : m_roots)
    {
        it.second->colorize(it.first == _id);
    }
}

void EasyTreeWidget::onSelectedBlockChange(unsigned int _block_index)
{
    disconnect(this, &Parent::currentItemChanged, this, &This::onCurrentItemChange);

    EasyTreeWidgetItem* item = nullptr;

    if (_block_index < EASY_GLOBALS.gui_blocks.size())
    {
        const auto i = easyBlock(_block_index).tree_item;
        if (i < m_items.size())
            item = m_items[i];
    }

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
    }
    else
    {
        setCurrentItem(item);
    }

    connect(this, &Parent::currentItemChanged, this, &This::onCurrentItemChange);
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::resizeColumnsToContents()
{
    for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
    {
        resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::onHideShowColumn(bool)
{
    auto action = qobject_cast<QAction*>(sender());
    if (action == nullptr)
        return;

    auto col = action->data().toInt();
    if (isColumnHidden(col))
        showColumn(col);
    else
        hideColumn(col);
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::loadSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("tree_widget");

    auto color_rows_set = settings.value("color_rows");
    if (!color_rows_set.isNull())
        m_bColorRows = color_rows_set.toBool();

    for (int i = 0; i < columnCount(); i++)
    {
        if (settings.value(QString("Column") + QString::number(i)).toBool())
            hideColumn(i);
    }

    settings.endGroup();
}

void EasyTreeWidget::saveSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("tree_widget");

    settings.setValue("color_rows", m_bColorRows);

    for (int i = 0; i < columnCount(); i++)
    {
        settings.setValue(QString("Column") + QString::number(i) , isColumnHidden(i));
    }

    settings.endGroup();
}

//////////////////////////////////////////////////////////////////////////
