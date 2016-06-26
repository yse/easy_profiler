

#ifndef MY____TREE___VIEW_H
#define MY____TREE___VIEW_H

#include <QTreeWidget>
#include <QMenu>
#include <QAction>
#include <stdlib.h>
#include <unordered_map>
#include "profiler/reader.h"

//////////////////////////////////////////////////////////////////////////

template <const size_t SIZEOF_T>
struct no_hasher {
    template <class T> inline size_t operator () (const T& _data) const {
        return (size_t)_data;
    }
};

#ifdef _WIN64
template <> struct no_hasher<8> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};
#endif

template <> struct no_hasher<4> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};

template <> struct no_hasher<2> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};

template <> struct no_hasher<1> {
    template <class T> inline size_t operator () (T _data) const {
        return (size_t)_data;
    }
};

template <class T>
struct do_no_hash {
    typedef no_hasher<sizeof(T)> hasher_t;
};

//////////////////////////////////////////////////////////////////////////

class MyTreeItem : public QTreeWidgetItem
{
    //Q_OBJECT

    const BlocksTree* m_block;

public:

    MyTreeItem(const BlocksTree* _block, QTreeWidgetItem* _parent = nullptr) : QTreeWidgetItem(_parent), m_block(_block)
    {
    }

    const BlocksTree* block() const
    {
        return m_block;
    }

    inline bool operator < (const QTreeWidgetItem& _other) const
    {
        const auto col = treeWidget()->sortColumn();
        //switch (col)
        //{
        //    case 1:
        //    case 2:
        //    case 3:
            if (col > 0 && col < 7)
            {
#ifndef _DEBUG
                return data(col, Qt::UserRole).toULongLong() < _other.data(col, Qt::UserRole).toULongLong();
#else
                const auto selfdata = data(col, Qt::UserRole).toULongLong();
                const auto otherdata = _other.data(col, Qt::UserRole).toULongLong();
                return selfdata < otherdata;
#endif
            }
        //}

        return QTreeWidgetItem::operator < (_other);
    }

    void setTimeSmart(int _column, const ::profiler::timestamp_t& _time)
    {
        setData(_column, Qt::UserRole, _time);

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

    void setTimeMs(int _column, const ::profiler::timestamp_t& _time)
    {
        setData(_column, Qt::UserRole, _time);
        setToolTip(_column, QString("%1 ns").arg(_time));
        setText(_column, QString::number(double(_time) * 1e-6, 'g', 9));
    }

};

//////////////////////////////////////////////////////////////////////////

class ItemAction : public QAction
{
    Q_OBJECT

private:

    MyTreeItem* m_item;

public:

    ItemAction(const char* _label, MyTreeItem* _item) : QAction(_label, nullptr), m_item(_item)
    {
        connect(this, &QAction::triggered, this, &ItemAction::onToggle);
    }

private:

    void onToggle(bool)
    {
        emit clicked(m_item);
    }

signals:

    void clicked(MyTreeItem* _item);
};

//////////////////////////////////////////////////////////////////////////

class MyTreeWidget : public QTreeWidget
{
    Q_OBJECT

protected:

    typedef ::std::unordered_map<const ::profiler::SerilizedBlock*, MyTreeItem*, do_no_hash<const ::profiler::SerilizedBlock*>::hasher_t> BlockItemMap;

    BlockItemMap            m_itemblocks;
    ::profiler::timestamp_t  m_beginTime;

public:

    MyTreeWidget(const thread_blocks_tree_t& _blocksTree, QWidget* _parent = nullptr) : QTreeWidget(_parent), m_beginTime(-1)
    {
        setAlternatingRowColors(true);
        setItemsExpandable(true);
        setAnimated(true);
        setSortingEnabled(true);
        setColumnCount(10);

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

        setTreeInternal(_blocksTree);

        setBaseSize(640, 480);
    }

    void setTree(const thread_blocks_tree_t& _blocksTree)
    {
        m_itemblocks.clear();
        clear();
        setTreeInternal(_blocksTree);
    }

protected:

    void setTreeInternal(const thread_blocks_tree_t& _blocksTree)
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
            auto item = new MyTreeItem(&threadTree.second);
            item->setText(0, QString("Thread %1").arg(threadTree.first));
            setTreeInternal(threadTree.second.children, item);
            addTopLevelItem(item);
        }
    }

    void setTreeInternal(const BlocksTree::children_t& _children, MyTreeItem* _parent)
    {
        for (const auto& child : _children)
        {
            const auto duration = child.node->block()->duration();
            const auto beginTime = child.node->block()->getBegin() - m_beginTime;
            const auto endTime = child.node->block()->getEnd() - m_beginTime;

            auto item = new MyTreeItem(&child, _parent);
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

    void contextMenuEvent(QContextMenuEvent* event)
    {
        const auto col = currentColumn();
        auto item = static_cast<MyTreeItem*>(currentItem());

        if (item && col >= 0)
        {
            QMenu menu;

            auto action = new QAction("Expand all", nullptr);
            connect(action, &QAction::triggered, this, &MyTreeWidget::onExpandAllClicked);
            menu.addAction(action);

            action = new QAction("Collapse all", nullptr);
            connect(action, &QAction::triggered, this, &MyTreeWidget::onCollapseAllClicked);
            menu.addAction(action);

            switch (col)
            {
                case 4:
                {
                    menu.addSeparator();
                    auto itemAction = new ItemAction("Jump to such item", item);
                    connect(itemAction, &ItemAction::clicked, this, &MyTreeWidget::onJumpToMinItemClicked);
                    menu.addAction(itemAction);
                    break;
                }

                case 5:
                {
                    menu.addSeparator();
                    auto itemAction = new ItemAction("Jump to such item", item);
                    connect(itemAction, &ItemAction::clicked, this, &MyTreeWidget::onJumpToMaxItemClicked);
                    menu.addAction(itemAction);
                    break;
                }
            }

            menu.exec(QCursor::pos());

            event->accept();
        }
    }

private slots:

    void onJumpToMinItemClicked(MyTreeItem* _item)
    {
        auto it = m_itemblocks.find(_item->block()->total_statistics->min_duration_block);
        if (it != m_itemblocks.end())
        {
            scrollToItem(it->second, QAbstractItemView::PositionAtCenter);
            setCurrentItem(it->second);
        }
    }

    void onJumpToMaxItemClicked(MyTreeItem* _item)
    {
        auto it = m_itemblocks.find(_item->block()->total_statistics->max_duration_block);
        if (it != m_itemblocks.end())
        {
            scrollToItem(it->second, QAbstractItemView::PositionAtCenter);
            setCurrentItem(it->second);
        }
    }

    void onCollapseAllClicked(bool)
    {
        collapseAll();
    }

    void onExpandAllClicked(bool)
    {
        expandAll();
    }

};

//////////////////////////////////////////////////////////////////////////

#endif // MY____TREE___VIEW_H
