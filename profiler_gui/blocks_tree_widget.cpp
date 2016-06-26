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
* change log        : * 2016/06/26 Victor Zarubkin: moved sources from tree_view.h
*                   :       and renamed classes from My* to Prof*.
*                   : *
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#include <QMenu>
#include <QContextMenuEvent>
#include "blocks_tree_widget.h"

//////////////////////////////////////////////////////////////////////////

ProfTreeWidgetItem::ProfTreeWidgetItem(const BlocksTree* _block, QTreeWidgetItem* _parent) : QTreeWidgetItem(_parent), m_block(_block)
{
}

ProfTreeWidgetItem::~ProfTreeWidgetItem()
{
}

const BlocksTree* ProfTreeWidgetItem::block() const
{
    return m_block;
}

void ProfTreeWidgetItem::setTimeSmart(int _column, const ::profiler::timestamp_t& _time)
{
    setData(_column, Qt::UserRole, (quint64)_time);

    setToolTip(_column, QString("%1 ns").arg(_time));

    if (_time < 1e3)
    {
        setText(_column, QString("%1 ns").arg(_time));
    }
    else if (_time < 1e6)
    {
        setText(_column, QString("%1 us").arg(double(_time) * 1e-3, 0, 'f', 3));
    }
    else if (_time < 1e9)
    {
        setText(_column, QString("%1 ms").arg(double(_time) * 1e-6, 0, 'f', 3));
    }
    else
    {
        setText(_column, QString("%1 s").arg(double(_time) * 1e-9, 0, 'f', 3));
    }
}

void ProfTreeWidgetItem::setTimeMs(int _column, const ::profiler::timestamp_t& _time)
{
    setData(_column, Qt::UserRole, (quint64)_time);
    setToolTip(_column, QString("%1 ns").arg(_time));
    setText(_column, QString::number(double(_time) * 1e-6, 'g', 9));
}

//////////////////////////////////////////////////////////////////////////

ProfTreeWidget::ProfTreeWidget(QWidget* _parent) : QTreeWidget(_parent), m_beginTime(-1)
{
    setAlternatingRowColors(true);
    setItemsExpandable(true);
    setAnimated(true);
    setSortingEnabled(false);
    setColumnCount(8);

    auto header = new QTreeWidgetItem();
    header->setText(0, "Name");
    header->setText(1, "Duration");
    header->setText(2, "Begin, ms");
    header->setText(3, "End, ms");
    header->setText(4, "Min dur.");
    header->setText(5, "Max dur.");
    header->setText(6, "Average dur.");
    header->setText(7, "N Calls");
    setHeaderItem(header);
}

ProfTreeWidget::ProfTreeWidget(const thread_blocks_tree_t& _blocksTree, QWidget* _parent) : ProfTreeWidget(_parent)
{
    setTreeInternal(_blocksTree);

    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);
    sortByColumn(2, Qt::AscendingOrder);

    connect(this, &QTreeWidget::itemExpanded, this, &ProfTreeWidget::onItemExpand);
}

ProfTreeWidget::~ProfTreeWidget()
{
}

void ProfTreeWidget::setTree(const thread_blocks_tree_t& _blocksTree)
{
    m_itemblocks.clear();

    clear();

    disconnect(this, &QTreeWidget::itemExpanded, this, &ProfTreeWidget::onItemExpand);
    setSortingEnabled(false);

    m_beginTime = -1;
    setTreeInternal(_blocksTree);

    setSortingEnabled(true);
    sortByColumn(0, Qt::AscendingOrder);
    sortByColumn(2, Qt::AscendingOrder);

    connect(this, &QTreeWidget::itemExpanded, this, &ProfTreeWidget::onItemExpand);
}

void ProfTreeWidget::setTreeInternal(const thread_blocks_tree_t& _blocksTree)
{
    for (const auto& threadTree : _blocksTree)
    {
        const auto beginTime = threadTree.second.children.front().node->block()->getBegin();
        if (m_beginTime > beginTime)
        {
            m_beginTime = beginTime;
        }
    }

    for (const auto& threadTree : _blocksTree)
    {
        auto item = new ProfTreeWidgetItem(&threadTree.second);
        item->setText(0, QString("Thread %1").arg(threadTree.first));
        setTreeInternal(threadTree.second.children, item);
        addTopLevelItem(item);
    }
}

void ProfTreeWidget::setTreeInternal(const BlocksTree::children_t& _children, ProfTreeWidgetItem* _parent)
{
    for (const auto& child : _children)
    {
        auto item = new ProfTreeWidgetItem(&child, _parent);
        item->setText(0, child.node->getBlockName());
        item->setTimeSmart(1, child.node->block()->duration());
        item->setTimeMs(2, child.node->block()->getBegin() - m_beginTime);
        item->setTimeMs(3, child.node->block()->getEnd() - m_beginTime);

        if (child.total_statistics)
        {
            item->setTimeSmart(4, child.total_statistics->min_duration);
            item->setTimeSmart(5, child.total_statistics->max_duration);
            item->setTimeSmart(6, child.total_statistics->average_duration());
            item->setText(7, QString::number(child.total_statistics->calls_number));
        }

        m_itemblocks[child.node] = item;

        if (!child.children.empty())
        {
            setTreeInternal(child.children, item);
        }
    }
}

void ProfTreeWidget::contextMenuEvent(QContextMenuEvent* _event)
{
    const auto col = currentColumn();
    auto item = static_cast<ProfTreeWidgetItem*>(currentItem());

    if (item == nullptr || col < 0)
    {
        return;
    }

    QMenu menu;

    auto action = new QAction("Expand all", nullptr);
    connect(action, &QAction::triggered, this, &ProfTreeWidget::onExpandAllClicked);
    menu.addAction(action);

    action = new QAction("Collapse all", nullptr);
    connect(action, &QAction::triggered, this, &ProfTreeWidget::onCollapseAllClicked);
    menu.addAction(action);

    switch (col)
    {
        case 4:
        {
            menu.addSeparator();
            auto itemAction = new ProfItemAction("Jump to such item", item);
            connect(itemAction, &ProfItemAction::clicked, this, &ProfTreeWidget::onJumpToMinItemClicked);
            menu.addAction(itemAction);
            break;
        }

        case 5:
        {
            menu.addSeparator();
            auto itemAction = new ProfItemAction("Jump to such item", item);
            connect(itemAction, &ProfItemAction::clicked, this, &ProfTreeWidget::onJumpToMaxItemClicked);
            menu.addAction(itemAction);
            break;
        }
    }

    menu.exec(QCursor::pos());

    _event->accept();
}

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
    disconnect(this, &QTreeWidget::itemExpanded, this, &ProfTreeWidget::onItemExpand);
    expandAll();
    resizeColumnToContents(0);
    connect(this, &QTreeWidget::itemExpanded, this, &ProfTreeWidget::onItemExpand);
}

void ProfTreeWidget::onItemExpand(QTreeWidgetItem*)
{
    resizeColumnToContents(0);
}

//////////////////////////////////////////////////////////////////////////
