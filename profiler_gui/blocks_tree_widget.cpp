/************************************************************************
* file name         : blocks_tree_widget.cpp
* ----------------- :
* creation time     : 2016/06/26
* copyright         : (c) 2016 Victor Zarubkin
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
* license           : TODO: add license text
************************************************************************/

#include <QMenu>
#include <QContextMenuEvent>
#include <QSignalBlocker>
#include <QSettings>
#include <QProgressDialog>
#include <QResizeEvent>
#include <QMoveEvent>
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

    auto header = new QTreeWidgetItem();

    header->setText(COL_NAME, "Name");

    header->setText(COL_BEGIN, "Begin, ms");

    header->setText(COL_DURATION, "Duration");
    header->setText(COL_SELF_DURATION, "Self Dur.");
    //header->setToolTip(COL_SELF_DURATION, "");
    header->setText(COL_DURATION_SUM_PER_PARENT, "Tot. Dur./Parent");
    header->setText(COL_DURATION_SUM_PER_FRAME, "Tot. Dur./Frame");
    header->setText(COL_DURATION_SUM_PER_THREAD, "Tot. Dur./Thread");

    header->setText(COL_SELF_DURATION_PERCENT, "Self %");
    header->setText(COL_PERCENT_PER_PARENT, "% / Parent");
    header->setText(COL_PERCENT_PER_FRAME, "% / Frame");
    header->setText(COL_PERCENT_SUM_PER_FRAME, "Tot. % / Frame");
    header->setText(COL_PERCENT_SUM_PER_PARENT, "Tot. % / Parent");
    header->setText(COL_PERCENT_SUM_PER_THREAD, "Tot. % / Thread");

    header->setText(COL_END, "End, ms");

    header->setText(COL_MIN_PER_FRAME, "Min dur./Frame");
    header->setText(COL_MAX_PER_FRAME, "Max dur./Frame");
    header->setText(COL_AVERAGE_PER_FRAME, "Average dur./Frame");
    header->setText(COL_NCALLS_PER_FRAME, "N Calls/Frame");

    header->setText(COL_MIN_PER_PARENT, "Min dur./Parent");
    header->setText(COL_MAX_PER_PARENT, "Max dur./Parent");
    header->setText(COL_AVERAGE_PER_PARENT, "Average dur./Parent");
    header->setText(COL_NCALLS_PER_PARENT, "N Calls/Parent");

    header->setText(COL_MIN_PER_THREAD, "Min dur./Thread");
    header->setText(COL_MAX_PER_THREAD, "Max dur./Thread");
    header->setText(COL_AVERAGE_PER_THREAD, "Average dur./Thread");
    header->setText(COL_NCALLS_PER_THREAD, "N Calls/Thread");

    setHeaderItem(header);

    connect(&::profiler_gui::EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChange);
    connect(&::profiler_gui::EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange);
    connect(&m_fillTimer, &QTimer::timeout, this, &This::onFillTimerTimeout);

    loadSettings();

    m_progress = new QProgressDialog("Building blocks hierarchy...", "", 0, 100, this, Qt::FramelessWindowHint);
    m_progress->setAttribute(Qt::WA_TranslucentBackground);
    m_progress->setCancelButton(nullptr);
    m_progress->setValue(100);
    //m_progress->hide();

    QTimer::singleShot(40, this, &This::alignProgressBar);
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
        onSelectedThreadChange(::profiler_gui::EASY_GLOBALS.selected_thread);
        onSelectedBlockChange(::profiler_gui::EASY_GLOBALS.selected_block);
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
        //m_progress->show();
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
    //        if (item.first == ::profiler_gui::EASY_GLOBALS.selected_thread)
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
        //m_progress->show();
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
    //        if (item.first == ::profiler_gui::EASY_GLOBALS.selected_thread)
    //            item.second->colorize(true);
    //    }
    //}

    //setSortingEnabled(true);
    //sortByColumn(COL_BEGIN, Qt::AscendingOrder);
    //resizeColumnToContents(COL_NAME);

    //connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
    //connect(this, &Parent::itemCollapsed, this, &This::onItemCollapse);
    //onSelectedBlockChange(::profiler_gui::EASY_GLOBALS.selected_block);
}

//////////////////////////////////////////////////////////////////////////

void EasyTreeWidget::clearSilent(bool _global)
{
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

    if (!_global)
    {
        for (auto item : m_items)
        {
            auto& gui_block = ::profiler_gui::EASY_GLOBALS.gui_blocks[item->block()->block_index];
            ::profiler_gui::set_max(gui_block.tree_item);
            gui_block.expanded = false;
        }
    }

    m_items.clear();
    m_roots.clear();

    const QSignalBlocker b(this);
    clear();

    if (!_global)
        emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();
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

    auto action = new QAction("Expand all", nullptr);
    connect(action, &QAction::triggered, this, &This::onExpandAllClicked);
    menu.addAction(action);

    action = new QAction("Collapse all", nullptr);
    connect(action, &QAction::triggered, this, &This::onCollapseAllClicked);
    menu.addAction(action);

    if (item != nullptr && col >= 0)
    {
        menu.addSeparator();

        action = new QAction("Expand all children", nullptr);
        connect(action, &QAction::triggered, this, &This::onExpandAllChildrenClicked);
        menu.addAction(action);

        action = new QAction("Collapse all children", nullptr);
        connect(action, &QAction::triggered, this, &This::onCollapseAllChildrenClicked);
        menu.addAction(action);
    }

    menu.addSeparator();

    action = new QAction("Color rows", nullptr);
    action->setCheckable(true);
    action->setChecked(m_bColorRows);
    connect(action, &QAction::triggered, this, &This::onColorizeRowsTriggered);
    menu.addAction(action);

    if (item != nullptr)
    {
        auto itemAction = new EasyItemAction("Show this item on scene", item->block()->block_index);
        itemAction->setToolTip("Scroll graphics scene to current item in the tree");
        connect(itemAction, &EasyItemAction::clicked, this, &This::onJumpToItemClicked);
        menu.addAction(itemAction);

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
                    auto block = item->block();
                    auto i = ::profiler_gui::numeric_max<unsigned int>();
                    switch (col)
                    {
                        case COL_MIN_PER_THREAD: i = block->per_thread_stats->min_duration_block; break;
                        case COL_MIN_PER_PARENT: i = block->per_parent_stats->min_duration_block; break;
                        case COL_MIN_PER_FRAME: i = block->per_frame_stats->min_duration_block; break;
                        case COL_MAX_PER_THREAD: i = block->per_thread_stats->max_duration_block; break;
                        case COL_MAX_PER_PARENT: i = block->per_parent_stats->max_duration_block; break;
                        case COL_MAX_PER_FRAME: i = block->per_frame_stats->max_duration_block; break;
                    }

                    if (i != ::profiler_gui::numeric_max(i))
                    {
                        menu.addSeparator();
                        itemAction = new EasyItemAction("Jump to such item", i);
                        itemAction->setToolTip("Jump to item with min/max duration (depending on clicked column)");
                        connect(itemAction, &EasyItemAction::clicked, this, &This::onJumpToItemClicked);
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
        auto columnAction = new EasyHideShowColumnAction(hdr->text(i), i);
        columnAction->setCheckable(true);
		columnAction->setChecked(!isColumnHidden(i));
        connect(columnAction, &EasyHideShowColumnAction::clicked, this, &This::onHideShowColumn);
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

void EasyTreeWidget::onJumpToItemClicked(unsigned int _block_index)
{
    ::profiler_gui::EASY_GLOBALS.selected_block = _block_index;
    emit ::profiler_gui::EASY_GLOBALS.events.selectedBlockChanged(_block_index);
}

void EasyTreeWidget::onCollapseAllClicked(bool)
{
    const QSignalBlocker b(this);

    m_bSilentExpandCollapse = true;
    collapseAll();
    m_bSilentExpandCollapse = false;

    for (auto item : m_items)
        ::profiler_gui::EASY_GLOBALS.gui_blocks[item->block()->block_index].expanded = false;
    emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();
}

void EasyTreeWidget::onExpandAllClicked(bool)
{
    const QSignalBlocker b(this);

    m_bSilentExpandCollapse = true;
    expandAll();
    resizeColumnsToContents();
    m_bSilentExpandCollapse = false;

    for (auto item : m_items)
        ::profiler_gui::EASY_GLOBALS.gui_blocks[item->block()->block_index].expanded = !item->block()->children.empty();
    emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();
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

        emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();
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

        emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void EasyTreeWidget::onItemExpand(QTreeWidgetItem* _item)
{
    ::profiler_gui::EASY_GLOBALS.gui_blocks[static_cast<EasyTreeWidgetItem*>(_item)->block()->block_index].expanded = true;

    if (!m_bSilentExpandCollapse)
    {
        resizeColumnsToContents();
        emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();
    }
}

void EasyTreeWidget::onItemCollapse(QTreeWidgetItem* _item)
{
    ::profiler_gui::EASY_GLOBALS.gui_blocks[static_cast<EasyTreeWidgetItem*>(_item)->block()->block_index].expanded = false;

    if (!m_bSilentExpandCollapse)
        emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();
}

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
    EasyTreeWidgetItem* item = nullptr;

    if (_block_index < ::profiler_gui::EASY_GLOBALS.gui_blocks.size())
    {
        const auto i = ::profiler_gui::EASY_GLOBALS.gui_blocks[_block_index].tree_item;
        if (i < m_items.size())
            item = m_items[i];
    }

    if (item != nullptr)
    {
        //const QSignalBlocker b(this);

        m_bSilentExpandCollapse = true;
        setCurrentItem(item);
        scrollToItem(item, QAbstractItemView::PositionAtCenter);
        if (::profiler_gui::EASY_GLOBALS.gui_blocks[item->block()->block_index].expanded)
            expandItem(item);
        else
            collapseItem(item);
        resizeColumnsToContents();
        m_bSilentExpandCollapse = false;

        //::profiler_gui::EASY_GLOBALS.gui_blocks[item->block()->block_index].expanded = item->isExpanded();
        emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();
    }
    else
    {
        setCurrentItem(item);
    }
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

void EasyTreeWidget::onHideShowColumn(int _column)
{
    if (isColumnHidden(_column))
    {
        showColumn(_column);
    }
    else
    {
        hideColumn(_column);
    }
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
