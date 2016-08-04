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
#include <QSettings>
#include <QTextCodec>
#include "blocks_tree_widget.h"

//////////////////////////////////////////////////////////////////////////

enum ColumnsIndexes
{
    COL_UNKNOWN = -1,

    COL_NAME = 0,
    COL_BEGIN,
    COL_DURATION,
    COL_SELF_DURATION,
    COL_SELF_DURATION_PERCENT,
    COL_PERCENT_OF_PARENT,
    COL_PERCENT_OF_FRAME,
    COL_END,
    COL_MIN,
    COL_MAX,
    COL_AVERAGE,
    COL_NCALLS,
    COL_MIN_TOTAL,
    COL_MAX_TOTAL,
    COL_AVERAGE_TOTAL,
    COL_NCALLS_TOTAL,

    COL_COLUMNS_NUMBER
};

//////////////////////////////////////////////////////////////////////////

ProfTreeWidgetItem::ProfTreeWidgetItem(const ::profiler::BlocksTree* _treeBlock, Parent* _parent)
    : Parent(_parent)
    , m_block(_treeBlock)
    , m_customBGColor(0)
    , m_customTextColor(0)
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
        //case COL_UNKNOWN:
        case COL_NAME:
        {
            if (parent() == nullptr)
                return false; // Do not sort topLevelItems by name
            return Parent::operator < (_other);
        }

        case COL_NCALLS_TOTAL:
        case COL_NCALLS:
        {
            return data(col, Qt::UserRole).toUInt() < _other.data(col, Qt::UserRole).toUInt();
        }

        case COL_SELF_DURATION_PERCENT:
        case COL_PERCENT_OF_PARENT:
        case COL_PERCENT_OF_FRAME:
        {
            return data(col, Qt::UserRole).toInt() < _other.data(col, Qt::UserRole).toInt();
        }

        default:
        {
            // durations min, max, average
            return data(col, Qt::UserRole).toULongLong() < _other.data(col, Qt::UserRole).toULongLong();
        }
    }

    return false;
}

const ::profiler::BlocksTree* ProfTreeWidgetItem::block() const
{
    return m_block;
}

::profiler::timestamp_t ProfTreeWidgetItem::duration() const
{
    if (m_block->node)
        return m_block->node->block()->duration();
    return data(COL_DURATION, Qt::UserRole).toULongLong();
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

void ProfTreeWidgetItem::setBackgroundColor(QRgb _color)
{
    m_customBGColor = _color;
}

void ProfTreeWidgetItem::setTextColor(QRgb _color)
{
    m_customTextColor = _color;
}

void ProfTreeWidgetItem::colorize(bool _colorize)
{
    if (_colorize)
    {
        for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
        {
            setBackground(i, QColor::fromRgb(m_customBGColor));
            setForeground(i, QColor::fromRgb(m_customTextColor));
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
    header->setText(COL_BEGIN, "Begin, ms");
    header->setText(COL_DURATION, "Duration");
    header->setText(COL_SELF_DURATION, "Self Dur.");
    header->setText(COL_SELF_DURATION_PERCENT, "Self %");
    header->setText(COL_PERCENT_OF_PARENT, "Parent %");
    header->setText(COL_PERCENT_OF_FRAME, "Frame %");
    header->setText(COL_END, "End, ms");
    header->setText(COL_MIN_TOTAL, "Min dur. total");
    header->setText(COL_MAX_TOTAL, "Max dur. total");
    header->setText(COL_AVERAGE_TOTAL, "Average dur. total");
    header->setText(COL_MIN, "Min dur.");
    header->setText(COL_MAX, "Max dur.");
    header->setText(COL_AVERAGE, "Average dur.");
    header->setText(COL_NCALLS, "N Calls");
    header->setText(COL_NCALLS_TOTAL, "N Calls total");
    setHeaderItem(header);

    connect(&::profiler_gui::EASY_GLOBALS.events, &::profiler_gui::ProfGlobalSignals::selectedThreadChanged, this, &This::onSelectedThreadChange);

    QSettings settings(profiler_gui::ORGANAZATION_NAME, profiler_gui::APPLICATION_NAME);
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

ProfTreeWidget::ProfTreeWidget(const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree, QWidget* _parent) : This(_parent)
{
    setTreeInternal(_blocksNumber, _blocksTree);

    setSortingEnabled(true);
    sortByColumn(COL_BEGIN, Qt::AscendingOrder);
    resizeColumnToContents(COL_NAME);

    connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
}

ProfTreeWidget::~ProfTreeWidget()
{
	saveSettings();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProfTreeWidget::setTree(const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree)
{
    clearSilent();

    setTreeInternal(_blocksNumber, _blocksTree);

    setSortingEnabled(true);
    sortByColumn(COL_BEGIN, Qt::AscendingOrder);
    resizeColumnToContents(COL_NAME);

    connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
}

void ProfTreeWidget::setTreeBlocks(const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _session_begin_time, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict)
{
    clearSilent();

    m_beginTime = _session_begin_time;
    _left += m_beginTime;// - ::std::min(m_beginTime, 1000ULL);
    _right += m_beginTime;// + 1000;
    setTreeInternal(_blocks, _left, _right, _strict);

    setSortingEnabled(true);
    sortByColumn(COL_BEGIN, Qt::AscendingOrder);
    resizeColumnToContents(COL_NAME);

    connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProfTreeWidget::clearSilent(bool _global)
{
    m_beginTime = -1;

    setSortingEnabled(false);
    disconnect(this, &Parent::itemExpanded, this, &This::onItemExpand);

    if (!_global)
    {
        for (auto item : m_items)
        {
            ::profiler_gui::EASY_GLOBALS.gui_blocks[item->block()->block_index].tree_item = nullptr;
        }
    }

    m_items.clear();

    const QSignalBlocker b(this);
    clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

size_t ProfTreeWidget::setTreeInternal(const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree)
{
    m_items.reserve(_blocksNumber + _blocksTree.size()); // _blocksNumber does not include Thread root blocks

    decltype(m_beginTime) finishtime = 0;
    for (const auto& threadTree : _blocksTree)
    {
        const auto node_block = threadTree.second.tree.children.front().node->block();
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
        auto& block = threadTree.second.tree;
        auto item = new ProfTreeWidgetItem(&block);

        if (threadTree.second.thread_name && threadTree.second.thread_name[0] != 0)
        {
            item->setText(COL_NAME, QString("%1 Thread %2").arg(threadTree.second.thread_name).arg(threadTree.first));
        }
        else
        {
            item->setText(COL_NAME, QString("Thread %1").arg(threadTree.first));
        }

        ::profiler::timestamp_t duration = 0;
        if (!block.children.empty())
        {
            duration = block.children.back().node->block()->getEnd() - block.children.front().node->block()->getBegin();
        }

        item->setTimeSmart(COL_DURATION, duration);
        item->setBackgroundColor(::profiler_gui::SELECTED_THREAD_BACKGROUND);
        item->setTextColor(::profiler_gui::SELECTED_THREAD_FOREGROUND);
        m_items.push_back(item);

        ::profiler::timestamp_t children_duration = 0;
        const auto children_items_number = setTreeInternal(block.children, item, nullptr, m_beginTime, finishtime + 1000000000ULL, false, children_duration);

        if (children_items_number > 0)
        {
            total_items += children_items_number + 1;
            addTopLevelItem(item);

            if (threadTree.first == ::profiler_gui::EASY_GLOBALS.selected_thread)
            {
                item->colorize(true);
            }

            m_roots[threadTree.first] = item;

            if (children_duration > 0)
            {
                item->setTimeSmart(COL_SELF_DURATION, children_duration);
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

size_t ProfTreeWidget::setTreeInternal(const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict)
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

    typedef ::std::unordered_map<::profiler::thread_id_t, ProfTreeWidgetItem*, ::profiler_gui::do_no_hash<::profiler::thread_id_t>::hasher_t> ThreadsMap;
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

        ::profiler::timestamp_t duration = 0;
        ProfTreeWidgetItem* thread_item = nullptr;
        auto thread_item_it = threadsMap.find(block.root->thread_id);
        if (thread_item_it != threadsMap.end())
        {
            thread_item = thread_item_it->second;
        }
        else
        {
            thread_item = new ProfTreeWidgetItem(&block.root->tree);

            if (block.root->thread_name && block.root->thread_name[0] != 0)
            {
                thread_item->setText(COL_NAME, QString("%1 Thread %2").arg(block.root->thread_name).arg(block.root->thread_id));
            }
            else
            {
                thread_item->setText(COL_NAME, QString("Thread %1").arg(block.root->thread_id));
            }

            if (!block.root->tree.children.empty())
            {
                duration = block.root->tree.children.back().node->block()->getEnd() - block.root->tree.children.front().node->block()->getBegin();
            }

            thread_item->setTimeSmart(COL_DURATION, duration);
            thread_item->setBackgroundColor(::profiler_gui::SELECTED_THREAD_BACKGROUND);
            thread_item->setTextColor(::profiler_gui::SELECTED_THREAD_FOREGROUND);
            threadsMap.insert(::std::make_pair(block.root->thread_id, thread_item));
        }

        auto item = new ProfTreeWidgetItem(block.tree, thread_item);
        duration = endTime - startTime;

		/*QByteArray msg(block.tree->node->getBlockName());
		QTextCodec *codec = QTextCodec::codecForName("Windows-1251");
		QString strf = codec->toUnicode(msg);
		*/
		item->setText(COL_NAME, block.tree->node->getBlockName());
        item->setTimeSmart(COL_DURATION, duration);
        item->setTimeMs(COL_BEGIN, startTime - m_beginTime);
        item->setTimeMs(COL_END, endTime - m_beginTime);

        item->setData(COL_PERCENT_OF_PARENT, Qt::UserRole, 0);
        item->setText(COL_PERCENT_OF_PARENT, "");

        item->setData(COL_PERCENT_OF_FRAME, Qt::UserRole, 0);
        item->setText(COL_PERCENT_OF_FRAME, "");

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
        const auto bgColor = ::profiler_gui::toRgb(::profiler::colors::get_red(color), ::profiler::colors::get_green(color), ::profiler::colors::get_blue(color));
        const auto fgColor = 0x00ffffff - bgColor;
        item->setBackgroundColor(bgColor);
        item->setTextColor(fgColor);

        m_items.push_back(item);

        size_t children_items_number = 0;
        ::profiler::timestamp_t children_duration = 0;
        if (!block.tree->children.empty())
        {
            children_items_number = setTreeInternal(block.tree->children, item, item, _left, _right, _strict, children_duration);
        }

        int percentage = 100;
        auto self_duration = duration - children_duration;
        if (children_duration > 0 && duration > 0)
        {
            percentage = static_cast<int>(0.5 + 100. * static_cast<double>(self_duration) / static_cast<double>(duration));
        }

        item->setTimeSmart(COL_SELF_DURATION, self_duration);
        item->setData(COL_SELF_DURATION_PERCENT, Qt::UserRole, percentage);
        item->setText(COL_SELF_DURATION_PERCENT, QString::number(percentage));

        if (children_items_number > 0 || !_strict || (startTime >= _left && endTime <= _right))
        {
            total_items += children_items_number + 1;
            ::profiler_gui::EASY_GLOBALS.gui_blocks[block.tree->block_index].tree_item = item;

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
        auto item = it.second;

        if (item->childCount() > 0)
        {
            addTopLevelItem(item);

            if (it.first == ::profiler_gui::EASY_GLOBALS.selected_thread)
            {
                item->colorize(true);
            }

            m_roots[it.first] = item;
            m_items.push_back(item);
            ++total_items;

            // Calculate clean duration (sum of all children durations)
            ::profiler::timestamp_t children_duration = 0;
            auto itemBlock = item->block();
            for (const auto& child : itemBlock->children)
            {
                children_duration += child.node->block()->duration();
            }

            item->setTimeSmart(COL_SELF_DURATION, children_duration);
        }
        else
        {
            delete item;
        }
    }

    return total_items;
}

size_t ProfTreeWidget::setTreeInternal(const ::profiler::BlocksTree::children_t& _children, ProfTreeWidgetItem* _parent, ProfTreeWidgetItem* _frame, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict, ::profiler::timestamp_t& _duration)
{
    size_t total_items = 0;
    for (const auto& child : _children)
    {
        const auto startTime = child.node->block()->getBegin();
        const auto endTime = child.node->block()->getEnd();
        const auto duration = endTime - startTime;
        _duration += duration;

        if (startTime > _right || endTime < _left)
        {
            continue;
        }

		/*QByteArray msg(child.node->getBlockName());
		QTextCodec *codec = QTextCodec::codecForName("Windows-1251");
		QString strf = codec->toUnicode(msg);*/

        auto item = new ProfTreeWidgetItem(&child, _parent);
		item->setText(COL_NAME, child.node->getBlockName());
        item->setTimeSmart(COL_DURATION, duration);
        item->setTimeMs(COL_BEGIN, startTime - m_beginTime);
        item->setTimeMs(COL_END, endTime - m_beginTime);

        if (_frame != nullptr)
        {
            auto percentage = duration == 0 ? 0 : static_cast<int>(0.5 + 100. * static_cast<double>(duration) / static_cast<double>(_parent->duration()));
            item->setData(COL_PERCENT_OF_PARENT, Qt::UserRole, percentage);
            item->setText(COL_PERCENT_OF_PARENT, QString::number(percentage));

            if (_parent != _frame)
                percentage = duration == 0 ? 0 : static_cast<int>(0.5 + 100. * static_cast<double>(duration) / static_cast<double>(_frame->duration()));
            item->setData(COL_PERCENT_OF_FRAME, Qt::UserRole, percentage);
            item->setText(COL_PERCENT_OF_FRAME, QString::number(percentage));
        }
        else
        {
            item->setData(COL_PERCENT_OF_PARENT, Qt::UserRole, 0);
            item->setText(COL_PERCENT_OF_PARENT, "");

            item->setData(COL_PERCENT_OF_FRAME, Qt::UserRole, 0);
            item->setText(COL_PERCENT_OF_FRAME, "");
        }

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
        const auto bgColor = ::profiler_gui::toRgb(::profiler::colors::get_red(color), ::profiler::colors::get_green(color), ::profiler::colors::get_blue(color));
        const auto fgColor = 0x00ffffff - bgColor;
        item->setBackgroundColor(bgColor);
        item->setTextColor(fgColor);

        m_items.push_back(item);

        size_t children_items_number = 0;
        ::profiler::timestamp_t children_duration = 0;
        if (!child.children.empty())
        {
            children_items_number = setTreeInternal(child.children, item, _frame ? _frame : item, _left, _right, _strict, children_duration);
        }

        int percentage = 100;
        auto self_duration = duration - children_duration;
        if (children_duration > 0 && duration > 0)
        {
            percentage = static_cast<int>(0.5 + 100. * static_cast<double>(self_duration) / static_cast<double>(duration));
        }

        item->setTimeSmart(COL_SELF_DURATION, self_duration);
        item->setData(COL_SELF_DURATION_PERCENT, Qt::UserRole, percentage);
        item->setText(COL_SELF_DURATION_PERCENT, QString::number(percentage));

        if (children_items_number > 0 || !_strict || (startTime >= _left && endTime <= _right))
        {
            total_items += children_items_number + 1;
            ::profiler_gui::EASY_GLOBALS.gui_blocks[child.block_index].tree_item = item;

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

    menu.addSeparator();

    auto hidemenu = menu.addMenu("Select columns");
    auto hdr = headerItem();

	for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
    {
        auto columnAction = new ProfHideShowColumnAction(hdr->text(i), i);
        columnAction->setCheckable(true);
		columnAction->setChecked(!isColumnHidden(i));
        connect(columnAction, &ProfHideShowColumnAction::clicked, this, &This::onHideShowColumn);
        hidemenu->addAction(columnAction);
    }

    menu.exec(QCursor::pos());

    _event->accept();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ProfTreeWidget::onJumpToMinItemClicked(ProfTreeWidgetItem* _item)
{
    auto item = ::profiler_gui::EASY_GLOBALS.gui_blocks[_item->block()->total_statistics->min_duration_block].tree_item;
    if (item != nullptr)
    {
        scrollToItem(item, QAbstractItemView::PositionAtCenter);
        setCurrentItem(item);
    }
}

void ProfTreeWidget::onJumpToMaxItemClicked(ProfTreeWidgetItem* _item)
{
    auto item = ::profiler_gui::EASY_GLOBALS.gui_blocks[_item->block()->total_statistics->max_duration_block].tree_item;
    if (item != nullptr)
    {
        scrollToItem(item, QAbstractItemView::PositionAtCenter);
        setCurrentItem(item);
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
    //resizeColumnToContents(COL_NAME);
    resizeColumnsToContents();
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
        //resizeColumnToContents(COL_NAME);
        resizeColumnsToContents();
        connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
    }
}

void ProfTreeWidget::onItemExpand(QTreeWidgetItem*)
{
    //resizeColumnToContents(COL_NAME);
    resizeColumnsToContents();
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

void ProfTreeWidget::onSelectedThreadChange(::profiler::thread_id_t _id)
{
    for (auto& it : m_roots)
    {
        it.second->colorize(it.first == _id);
    }
}

//////////////////////////////////////////////////////////////////////////

void ProfTreeWidget::resizeColumnsToContents()
{
    for (int i = 0; i < COL_COLUMNS_NUMBER; ++i)
    {
        resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

void ProfTreeWidget::onHideShowColumn(int _column)
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

void ProfTreeWidget::saveSettings()
{
	QSettings settings(profiler_gui::ORGANAZATION_NAME, profiler_gui::APPLICATION_NAME);
	settings.beginGroup("tree_widget");

	settings.setValue("color_rows", m_bColorRows);

	for (int i = 0; i < columnCount(); i++)
	{
		settings.setValue(QString("Column") + QString::number(i) , isColumnHidden(i));
	}

	settings.endGroup();
}
