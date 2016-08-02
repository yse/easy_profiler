/************************************************************************
* file name         : blocks_tree_widget.cpp
* ----------------- :
* creation time     : 2016/06/26
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of TreeWidget and it's auxiliary classes
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
*                   : * 
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#include <algorithm>
#include <QMenu>
#include <QContextMenuEvent>
#include <QSignalBlocker>
#include "blocks_tree_widget.h"

//////////////////////////////////////////////////////////////////////////

enum ColumnsIndexes
{
    COL_UNKNOWN = -1,

    COL_NAME = 0,
    COL_BEGIN,
    COL_DURATION,
    COL_END,
    COL_MIN_TOTAL,
    COL_MAX_TOTAL,
    COL_AVERAGE_TOTAL,
    COL_NCALLS_TOTAL,
    COL_MIN,
    COL_MAX,
    COL_AVERAGE,
    COL_NCALLS,

    COL_COLUMNS_NUMBER
};

//////////////////////////////////////////////////////////////////////////

ProfTreeWidgetItem::ProfTreeWidgetItem(const BlocksTree* _treeBlock, Parent* _parent)
    : Parent(_parent)
    , m_block(_treeBlock)
{
}

ProfTreeWidgetItem::~ProfTreeWidgetItem()
{
}

bool ProfTreeWidgetItem::operator < (const Parent& _other) const
{
    const auto col = treeWidget()->sortColumn();

    switch (col)
    {
//         case COL_UNKNOWN:
        case COL_NAME:
        {
            // column 0 - Name
            if (parent() == nullptr)
                return false; // Do not sort topLevelItems
            return Parent::operator < (_other);
        }

        case COL_NCALLS_TOTAL:
        case COL_NCALLS:
        {
            // column 7 and 11 - N calls
            return data(col, Qt::UserRole).toUInt() < _other.data(col, Qt::UserRole).toUInt();
        }

        default:
        {
            // columns [1; 6] and [8; 10] - durations
            return data(col, Qt::UserRole).toULongLong() < _other.data(col, Qt::UserRole).toULongLong();
        }
    }

    return false;
}

const BlocksTree* ProfTreeWidgetItem::block() const
{
    return m_block;
}

void ProfTreeWidgetItem::setTimeSmart(int _column, const ::profiler::timestamp_t& _time)
{
    const ::profiler::timestamp_t nanosecondsTime = PROF_NANOSECONDS(_time);

    setData(_column, Qt::UserRole, (quint64)nanosecondsTime);

    setToolTip(_column, QString("%1 ns").arg(nanosecondsTime));

    if (_time < 1e3)
    {
        setText(_column, QString("%1 ns").arg(nanosecondsTime));
    }
    else if (_time < 1e6)
    {
        setText(_column, QString("%1 us").arg(double(nanosecondsTime) * 1e-3, 0, 'f', 3));
    }
    else if (_time < 1e9)
    {
        setText(_column, QString("%1 ms").arg(double(nanosecondsTime) * 1e-6, 0, 'f', 3));
    }
    else
    {
        setText(_column, QString("%1 s").arg(double(nanosecondsTime) * 1e-9, 0, 'f', 3));
    }
}

void ProfTreeWidgetItem::setTimeMs(int _column, const ::profiler::timestamp_t& _time)
{
    const ::profiler::timestamp_t nanosecondsTime = PROF_NANOSECONDS(_time);
    setData(_column, Qt::UserRole, (quint64)nanosecondsTime);
    setToolTip(_column, QString("%1 ns").arg(nanosecondsTime));
    setText(_column, QString::number(double(nanosecondsTime) * 1e-6, 'g', 9));
}

void ProfTreeWidgetItem::setBackgroundColor(const QColor& _color)
{
    m_customBGColor = _color;
}

void ProfTreeWidgetItem::setTextColor(const QColor& _color)
{
    m_customTextColor = _color;
}

void ProfTreeWidgetItem::colorize(bool _colorize)
{
    if (_colorize)
    {
        for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
        {
            setBackground(i, m_customBGColor);
            setForeground(i, m_customTextColor);
        }
    }
    else
    {
        const QBrush nobrush;
        for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
        {
            setBackground(i, nobrush);
            setForeground(i, nobrush);
        }
    }
}

void ProfTreeWidgetItem::collapseAll()
{
    for (int i = 0, childrenNumber = childCount(); i < childrenNumber; ++i)
    {
        static_cast<ProfTreeWidgetItem*>(child(i))->collapseAll();
    }

    setExpanded(false);
}

void ProfTreeWidgetItem::expandAll()
{
    for (int i = 0, childrenNumber = childCount(); i < childrenNumber; ++i)
    {
        static_cast<ProfTreeWidgetItem*>(child(i))->expandAll();
    }

    setExpanded(true);
}

//////////////////////////////////////////////////////////////////////////

ProfTreeWidget::ProfTreeWidget(QWidget* _parent) : Parent(_parent), m_beginTime(-1), m_bColorRows(true)
{
    setAutoFillBackground(false);
    setAlternatingRowColors(true);
    setItemsExpandable(true);
    setAnimated(true);
    setSortingEnabled(false);
    setColumnCount(COL_COLUMNS_NUMBER);

    auto header = new QTreeWidgetItem();
    header->setText(COL_NAME, "Name");
    header->setText(COL_DURATION, "Duration");
    header->setText(COL_BEGIN, "Begin, ms");
    header->setText(COL_END, "End, ms");
    header->setText(COL_MIN_TOTAL, "Min dur. total");
    header->setText(COL_MAX_TOTAL, "Max dur. total");
    header->setText(COL_AVERAGE_TOTAL, "Average dur. total");
    header->setText(COL_NCALLS_TOTAL, "N Calls total");
    header->setText(COL_MIN, "Min dur.");
    header->setText(COL_MAX, "Max dur.");
    header->setText(COL_AVERAGE, "Average dur.");
    header->setText(COL_NCALLS, "N Calls");
    setHeaderItem(header);
}

ProfTreeWidget::ProfTreeWidget(const unsigned int _blocksNumber, const thread_blocks_tree_t& _blocksTree, QWidget* _parent) : This(_parent)
{
    setTreeInternal(_blocksNumber, _blocksTree);

    setSortingEnabled(true);
    sortByColumn(COL_BEGIN, Qt::AscendingOrder);

    connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
}

ProfTreeWidget::~ProfTreeWidget()
{
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProfTreeWidget::setTree(const unsigned int _blocksNumber, const thread_blocks_tree_t& _blocksTree)
{
    clearSilent();

    setTreeInternal(_blocksNumber, _blocksTree);

    setSortingEnabled(true);
    sortByColumn(COL_BEGIN, Qt::AscendingOrder);

    connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
}

void ProfTreeWidget::setTreeBlocks(const TreeBlocks& _blocks, ::profiler::timestamp_t _session_begin_time, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict)
{
    clearSilent();

    m_beginTime = _session_begin_time;
    _left += m_beginTime;// - ::std::min(m_beginTime, 1000ULL);
    _right += m_beginTime;// + 1000;
    setTreeInternal(_blocks, _left, _right, _strict);

    setSortingEnabled(true);
    sortByColumn(COL_BEGIN, Qt::AscendingOrder);

    connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProfTreeWidget::clearSilent()
{
    m_beginTime = -1;

    setSortingEnabled(false);
    disconnect(this, &Parent::itemExpanded, this, &This::onItemExpand);

    m_items.clear();
    m_itemblocks.clear();

    const QSignalBlocker b(this);
    clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t ProfTreeWidget::setTreeInternal(const unsigned int _blocksNumber, const thread_blocks_tree_t& _blocksTree)
{
    m_items.reserve(_blocksNumber + _blocksTree.size()); // _blocksNumber does not include Thread root blocks

    decltype(m_beginTime) finishtime = 0;
    for (const auto& threadTree : _blocksTree)
    {
        const auto node_block = threadTree.second.children.front().node->block();
        const auto startTime = node_block->getBegin();
        const auto endTime = node_block->getEnd();

        if (m_beginTime > startTime)
        {
            m_beginTime = startTime;
        }

        if (finishtime < endTime)
        {
            finishtime = endTime;
        }
    }

    size_t total_items = 0;
    const QSignalBlocker b(this);
    for (const auto& threadTree : _blocksTree)
    {
        auto item = new ProfTreeWidgetItem(&threadTree.second);
        item->setText(COL_NAME, QString("Thread %1").arg(threadTree.first));

        m_items.push_back(item);

        const auto children_items_number = setTreeInternal(threadTree.second.children, item, m_beginTime, finishtime + 1000000000, false);

        if (children_items_number > 0)
        {
            total_items += children_items_number + 1;
            addTopLevelItem(item);

            if (m_bColorRows)
            {
                item->colorize(m_bColorRows);
            }
        }
        else
        {
            delete item;
        }
    }

    return total_items;
}

size_t ProfTreeWidget::setTreeInternal(const TreeBlocks& _blocks, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict)
{
    if (_blocks.empty())
    {
        return 0;
    }

    size_t blocksNumber = 0;
    for (const auto& block : _blocks)
    {
        blocksNumber += block.tree->total_children_number;
    }

    m_items.reserve(blocksNumber + _blocks.size()); // blocksNumber does not include root blocks

    typedef ::std::unordered_map<::profiler::thread_id_t, ProfTreeWidgetItem*, ::btw::do_no_hash<::profiler::thread_id_t>::hasher_t> ThreadsMap;
    ThreadsMap threadsMap;

    size_t total_items = 0;
    const QSignalBlocker b(this);
    for (const auto& block : _blocks)
    {
        const auto startTime = block.tree->node->block()->getBegin();
        const auto endTime = block.tree->node->block()->getEnd();
        if (startTime > _right || endTime < _left)
        {
            continue;
        }

        ProfTreeWidgetItem* thread_item = nullptr;
        auto thread_item_it = threadsMap.find(block.thread_id);
        if (thread_item_it != threadsMap.end())
        {
            thread_item = thread_item_it->second;
        }
        else
        {
            thread_item = new ProfTreeWidgetItem(block.thread_tree);
            thread_item->setText(COL_NAME, QString("Thread %1").arg(block.thread_id));
            threadsMap.insert(::std::make_pair(block.thread_id, thread_item));
        }

        auto item = new ProfTreeWidgetItem(block.tree, thread_item);
        item->setText(COL_NAME, block.tree->node->getBlockName());
        item->setTimeSmart(COL_DURATION, block.tree->node->block()->duration());
        item->setTimeMs(COL_BEGIN, startTime - m_beginTime);
        item->setTimeMs(COL_END, endTime - m_beginTime);

        if (block.tree->total_statistics)
        {
            item->setTimeSmart(COL_MIN_TOTAL, block.tree->total_statistics->min_duration);
            item->setTimeSmart(COL_MAX_TOTAL, block.tree->total_statistics->max_duration);
            item->setTimeSmart(COL_AVERAGE_TOTAL, block.tree->total_statistics->average_duration());

            item->setData(COL_NCALLS_TOTAL, Qt::UserRole, block.tree->total_statistics->calls_number);
            item->setText(COL_NCALLS_TOTAL, QString::number(block.tree->total_statistics->calls_number));
        }

        if (block.tree->frame_statistics)
        {
            item->setTimeSmart(COL_MIN, block.tree->frame_statistics->min_duration);
            item->setTimeSmart(COL_MAX, block.tree->frame_statistics->max_duration);
            item->setTimeSmart(COL_AVERAGE, block.tree->frame_statistics->average_duration());

            item->setData(COL_NCALLS, Qt::UserRole, block.tree->frame_statistics->calls_number);
            item->setText(COL_NCALLS, QString::number(block.tree->frame_statistics->calls_number));
        }

        const auto color = block.tree->node->block()->getColor();
        const auto bgColor = QColor(toRgb(profiler::colors::get_red(color), profiler::colors::get_green(color), profiler::colors::get_blue(color)));
        const auto fgColor = QColor(QRgb(0x00ffffff - bgColor.rgb()));
        item->setBackgroundColor(bgColor);
        item->setTextColor(fgColor);

        m_items.push_back(item);

        size_t children_items_number = 0;
        if (!block.tree->children.empty())
        {
            children_items_number = setTreeInternal(block.tree->children, item, _left, _right, _strict);
        }

        if (children_items_number > 0 || !_strict || (startTime >= _left && endTime <= _right))
        {
            total_items += children_items_number + 1;
            m_itemblocks[block.tree->node] = item;

            if (m_bColorRows)
            {
                item->colorize(m_bColorRows);
            }
        }
        else
        {
            m_items.pop_back();
            delete item;
        }
    }

    for (auto& it : threadsMap)
    {
        if (it.second->childCount() > 0)
        {
            addTopLevelItem(it.second);

            if (m_bColorRows)
            {
                it.second->colorize(m_bColorRows);
            }

            m_items.push_back(it.second);
            ++total_items;
        }
        else
        {
            delete it.second;
        }
    }

    return total_items;
}

size_t ProfTreeWidget::setTreeInternal(const BlocksTree::children_t& _children, ProfTreeWidgetItem* _parent, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict)
{
    size_t total_items = 0;
    for (const auto& child : _children)
    {
        const auto startTime = child.node->block()->getBegin();
        const auto endTime = child.node->block()->getEnd();
        if (startTime > _right || endTime < _left)
        {
            continue;
        }

        auto item = new ProfTreeWidgetItem(&child, _parent);
        item->setText(COL_NAME, child.node->getBlockName());
        item->setTimeSmart(COL_DURATION, child.node->block()->duration());
        item->setTimeMs(COL_BEGIN, startTime - m_beginTime);
        item->setTimeMs(COL_END, endTime - m_beginTime);

        if (child.total_statistics)
        {
            item->setTimeSmart(COL_MIN_TOTAL, child.total_statistics->min_duration);
            item->setTimeSmart(COL_MAX_TOTAL, child.total_statistics->max_duration);
            item->setTimeSmart(COL_AVERAGE_TOTAL, child.total_statistics->average_duration());

            item->setData(COL_NCALLS_TOTAL, Qt::UserRole, child.total_statistics->calls_number);
            item->setText(COL_NCALLS_TOTAL, QString::number(child.total_statistics->calls_number));
        }

        if (child.frame_statistics)
        {
            item->setTimeSmart(COL_MIN, child.frame_statistics->min_duration);
            item->setTimeSmart(COL_MAX, child.frame_statistics->max_duration);
            item->setTimeSmart(COL_AVERAGE, child.frame_statistics->average_duration());

            item->setData(COL_NCALLS, Qt::UserRole, child.frame_statistics->calls_number);
            item->setText(COL_NCALLS, QString::number(child.frame_statistics->calls_number));
        }

        const auto color = child.node->block()->getColor();
        const auto bgColor = QColor(toRgb(profiler::colors::get_red(color), profiler::colors::get_green(color), profiler::colors::get_blue(color)));
        const auto fgColor = QColor(QRgb(0x00ffffff - bgColor.rgb()));
        item->setBackgroundColor(bgColor);
        item->setTextColor(fgColor);

        m_items.push_back(item);

        size_t children_items_number = 0;
        if (!child.children.empty())
        {
            children_items_number = setTreeInternal(child.children, item, _left, _right, _strict);
        }

        if (children_items_number > 0 || !_strict || (startTime >= _left && endTime <= _right))
        {
            total_items += children_items_number + 1;
            m_itemblocks[child.node] = item;

            if (m_bColorRows)
            {
                item->colorize(m_bColorRows);
            }
        }
        else
        {
            m_items.pop_back();
            delete item;
        }
    }

    return total_items;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProfTreeWidget::contextMenuEvent(QContextMenuEvent* _event)
{
    const auto col = currentColumn();
    auto item = static_cast<ProfTreeWidgetItem*>(currentItem());
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

    if (item != nullptr && col >= 0)
    {
        switch (col)
        {
            case COL_MIN_TOTAL:
            case COL_MIN:
            {
                menu.addSeparator();
                auto itemAction = new ProfItemAction("Jump to such item", item);
                connect(itemAction, &ProfItemAction::clicked, this, &This::onJumpToMinItemClicked);
                menu.addAction(itemAction);
                break;
            }

            case COL_MAX_TOTAL:
            case COL_MAX:
            {
                menu.addSeparator();
                auto itemAction = new ProfItemAction("Jump to such item", item);
                connect(itemAction, &ProfItemAction::clicked, this, &This::onJumpToMaxItemClicked);
                menu.addAction(itemAction);
                break;
            }
        }
    }

    menu.exec(QCursor::pos());

    _event->accept();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProfTreeWidget::onJumpToMinItemClicked(ProfTreeWidgetItem* _item)
{
    auto it = m_itemblocks.find(_item->block()->total_statistics->min_duration_block);
    if (it != m_itemblocks.end())
    {
        scrollToItem(it->second, QAbstractItemView::PositionAtCenter);
        setCurrentItem(it->second);
    }
}

void ProfTreeWidget::onJumpToMaxItemClicked(ProfTreeWidgetItem* _item)
{
    auto it = m_itemblocks.find(_item->block()->total_statistics->max_duration_block);
    if (it != m_itemblocks.end())
    {
        scrollToItem(it->second, QAbstractItemView::PositionAtCenter);
        setCurrentItem(it->second);
    }
}

void ProfTreeWidget::onCollapseAllClicked(bool)
{
    collapseAll();
}

void ProfTreeWidget::onExpandAllClicked(bool)
{
    disconnect(this, &Parent::itemExpanded, this, &This::onItemExpand);
    expandAll();
    resizeColumnToContents(COL_NAME);
    connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
}

void ProfTreeWidget::onCollapseAllChildrenClicked(bool)
{
    auto current = static_cast<ProfTreeWidgetItem*>(currentItem());
    if (current != nullptr)
    {
        current->collapseAll();
    }
}

void ProfTreeWidget::onExpandAllChildrenClicked(bool)
{
    auto current = static_cast<ProfTreeWidgetItem*>(currentItem());
    if (current != nullptr)
    {
        disconnect(this, &Parent::itemExpanded, this, &This::onItemExpand);
        current->expandAll();
        resizeColumnToContents(COL_NAME);
        connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
    }
}

void ProfTreeWidget::onItemExpand(QTreeWidgetItem*)
{
    resizeColumnToContents(COL_NAME);
}

void ProfTreeWidget::onColorizeRowsTriggered(bool _colorize)
{
    const QSignalBlocker b(this);

    m_bColorRows = _colorize;

    auto current = currentItem();
    collapseAll(); // Without collapseAll() changing items process is VERY VERY SLOW.
    // TODO: Find the reason of such behavior. QSignalBlocker(this) does not help. QSignalBlocker(item) does not work, because items are not inherited from QObject.

    for (auto item : m_items)
    {
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
