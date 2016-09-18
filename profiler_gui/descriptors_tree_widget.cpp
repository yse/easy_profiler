/************************************************************************
* file name         : descriptors_tree_widget.cpp
* ----------------- :
* creation time     : 2016/09/17
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of EasyDescTreeWidget and it's auxiliary classes
*                   : for displyaing EasyProfiler blocks descriptors tree.
* ----------------- :
* change log        : * 2016/09/17 Victor Zarubkin: initial commit.
*                   :
*                   : *
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
#include <QAction>
#include <QHeaderView>
#include <QString>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QSignalBlocker>
#include <QSettings>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <thread>
#include "descriptors_tree_widget.h"
#include "globals.h"

#ifdef _WIN32
#include <Windows.h>
#endif

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//////////////////////////////////////////////////////////////////////////

enum DescColumns
{
    DESC_COL_FILE_LINE = 0,
    DESC_COL_NAME,
    DESC_COL_STATUS,

    DESC_COL_COLUMNS_NUMBER
};

const auto ENABLED_COLOR = ::profiler::colors::LightGreen900;
const auto DISABLED_COLOR = ::profiler::colors::DarkRed;

//////////////////////////////////////////////////////////////////////////

EasyDescWidgetItem::EasyDescWidgetItem(::profiler::block_id_t _desc, Parent* _parent) : Parent(_parent), m_desc(_desc)
{

}

EasyDescWidgetItem::~EasyDescWidgetItem()
{

}

bool EasyDescWidgetItem::operator < (const Parent& _other) const
{
    const auto col = treeWidget()->sortColumn();

    switch (col)
    {
        case DESC_COL_FILE_LINE:
        {
            if (parent() != nullptr)
                return data(col, Qt::UserRole).toInt() < _other.data(col, Qt::UserRole).toInt();
        }
    }

    return Parent::operator < (_other);
}

//////////////////////////////////////////////////////////////////////////

EasyDescTreeWidget::EasyDescTreeWidget(QWidget* _parent)
    : Parent(_parent)
    , m_lastFound(nullptr)
    , m_searchColumn(DESC_COL_NAME)
    , m_bLocked(false)
{
    setAutoFillBackground(false);
    setAlternatingRowColors(true);
    setItemsExpandable(true);
    setAnimated(true);
    setSortingEnabled(false);
    setColumnCount(DESC_COL_COLUMNS_NUMBER);

    auto header_item = new QTreeWidgetItem();
    header_item->setText(DESC_COL_FILE_LINE, "File/Line");
    header_item->setText(DESC_COL_NAME, "Name");
    header_item->setText(DESC_COL_STATUS, "Status");
    setHeaderItem(header_item);

    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::selectedBlockChanged, this, &This::onSelectedBlockChange);
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::enableStatusChanged, this, &This::onEnableStatusChange);
    connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
    connect(this, &Parent::itemDoubleClicked, this, &This::onDoubleClick);
    connect(this, &Parent::currentItemChanged, this, &This::onCurrentItemChange);

    loadSettings();
}

EasyDescTreeWidget::~EasyDescTreeWidget()
{
    saveSettings();
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::contextMenuEvent(QContextMenuEvent* _event)
{
    _event->accept();

    QMenu menu;
    auto action = menu.addAction("Expand all");
    SET_ICON(action, ":/Expand");
    connect(action, &QAction::triggered, this, &This::expandAll);

    action = menu.addAction("Collapse all");
    SET_ICON(action, ":/Collapse");
    connect(action, &QAction::triggered, this, &This::collapseAll);

    menu.addSeparator();
    auto submenu = menu.addMenu("Search by");
    auto header_item = headerItem();
    for (int i = 0; i < DESC_COL_STATUS; ++i)
    {
        action = new QAction(header_item->text(i), nullptr);
        action->setCheckable(true);
        if (i == m_searchColumn)
            action->setChecked(true);
        connect(action, &QAction::triggered, this, &This::onSearchColumnChange);
        submenu->addAction(action);
    }

    menu.exec(QCursor::pos());
}

void EasyDescTreeWidget::onSearchColumnChange(bool)
{
    auto action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
        m_searchColumn = action->data().toInt();
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::keyPressEvent(QKeyEvent* _event)
{
    Parent::keyPressEvent(_event);

    if (_event->key() == Qt::Key_F3)
    {
        if (_event->modifiers() & Qt::ShiftModifier)
            findPrev(m_lastSearch);
        else
            findNext(m_lastSearch);
    }

    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::clearSilent(bool)
{
    const QSignalBlocker b(this);

    setSortingEnabled(false);
    m_lastFound = nullptr;
    m_lastSearch.clear();

    m_items.clear();

    ::std::vector<QTreeWidgetItem*> topLevelItems;
    topLevelItems.reserve(topLevelItemCount());
    for (int i = topLevelItemCount() - 1; i >= 0; --i)
        topLevelItems.push_back(takeTopLevelItem(i));

    auto deleter_thread = ::std::thread([](decltype(topLevelItems) _items) {
        for (auto item : _items)
            delete item;
    }, ::std::move(topLevelItems));

#ifdef _WIN32
    SetThreadPriority(deleter_thread.native_handle(), THREAD_PRIORITY_LOWEST);
#endif

    deleter_thread.detach();

    //clear();
}

//////////////////////////////////////////////////////////////////////////

struct FileItems
{
    typedef ::std::unordered_map<int, EasyDescWidgetItem*, ::profiler_gui::do_no_hash<int>::hasher_t> Items;
    Items children;
    QTreeWidgetItem* item = nullptr;
};

void EasyDescTreeWidget::build()
{
    clearSilent(false);

    auto f = font();
    f.setBold(true);

    typedef ::std::unordered_map<::std::string, FileItems> Files;
    Files m_files;

    m_items.resize(EASY_GLOBALS.descriptors.size());
    memset(m_items.data(), 0, sizeof(void*) * m_items.size());

    const QSignalBlocker b(this);
    ::profiler::block_id_t id = 0;
    for (auto desc : EASY_GLOBALS.descriptors)
    {
        if (desc != nullptr)
        {
            auto& p = m_files[desc->file()];
            if (p.item == nullptr)
            {
                p.item = new QTreeWidgetItem();
                p.item->setText(DESC_COL_FILE_LINE, desc->file());
            }

            auto it = p.children.find(desc->line());
            if (it == p.children.end())
            {
                auto item = new EasyDescWidgetItem(id, p.item);
                item->setText(DESC_COL_FILE_LINE, QString::number(desc->line()));
                item->setData(DESC_COL_FILE_LINE, Qt::UserRole, desc->line());
                item->setText(DESC_COL_NAME, desc->name());

                item->setFont(DESC_COL_STATUS, f);

                QBrush brush;
                if (desc->enabled())
                {
                    item->setText(DESC_COL_STATUS, "ON");
                    brush.setColor(QColor::fromRgba(ENABLED_COLOR));
                }
                else
                {
                    item->setText(DESC_COL_STATUS, "OFF");
                    brush.setColor(QColor::fromRgba(DISABLED_COLOR));
                }

                item->setForeground(DESC_COL_STATUS, brush);

                m_items[id] = item;
            }
            else
            {
                m_items[id] = it->second;
            }
        }

        ++id;
    }

    for (auto& p : m_files)
    {
        addTopLevelItem(p.second.item);
    }

    setSortingEnabled(true);
    sortByColumn(DESC_COL_FILE_LINE, Qt::AscendingOrder);
    resizeColumnsToContents();
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::onItemExpand(QTreeWidgetItem*)
{
    resizeColumnsToContents();
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::onDoubleClick(QTreeWidgetItem* _item, int _column)
{
    if (_column >= DESC_COL_NAME && _item->parent() != nullptr)
    {
        auto item = static_cast<EasyDescWidgetItem*>(_item);
        auto& desc = easyDescriptor(item->desc());

        QBrush brush;
        if (desc.enabled())
        {
            desc.setEnabled(false);
            item->setText(DESC_COL_STATUS, "OFF");
            brush.setColor(QColor::fromRgba(DISABLED_COLOR));
        }
        else
        {
            desc.setEnabled(true);
            item->setText(DESC_COL_STATUS, "ON");
            brush.setColor(QColor::fromRgba(ENABLED_COLOR));
        }

        item->setForeground(DESC_COL_STATUS, brush);

        m_bLocked = true;
        emit EASY_GLOBALS.events.enableStatusChanged(item->desc(), desc.enabled());
        m_bLocked = false;
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::onCurrentItemChange(QTreeWidgetItem* _item, QTreeWidgetItem* _prev)
{
    if (_prev != nullptr)
    {
        auto f = font();
        for (int i = 0; i < DESC_COL_STATUS; ++i)
            _prev->setFont(i, f);
    }

    if (_item != nullptr)
    {
        auto f = font();
        f.setBold(true);
        for (int i = 0; i < DESC_COL_STATUS; ++i)
            _item->setFont(i, f);
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::onEnableStatusChange(::profiler::block_id_t _id, bool _enabled)
{
    if (m_bLocked)
        return;

    auto item = m_items[_id];
    if (item == nullptr)
        return;

    QBrush brush;
    if (_enabled)
    {
        item->setText(DESC_COL_STATUS, "ON");
        brush.setColor(QColor::fromRgba(ENABLED_COLOR));
    }
    else
    {
        item->setText(DESC_COL_STATUS, "OFF");
        brush.setColor(QColor::fromRgba(DISABLED_COLOR));
    }

    item->setForeground(DESC_COL_STATUS, brush);
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::resizeColumnsToContents()
{
    for (int i = 0; i < DESC_COL_COLUMNS_NUMBER; ++i)
        resizeColumnToContents(i);
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::onSelectedBlockChange(uint32_t _block_index)
{
    if (_block_index == ::profiler_gui::numeric_max(_block_index))
        return;

    auto item = m_items[blocksTree(_block_index).node->id()];
    if (item == nullptr)
        return;

    scrollToItem(item, QAbstractItemView::PositionAtCenter);
    setCurrentItem(item);
}

//////////////////////////////////////////////////////////////////////////

void EasyDescTreeWidget::loadSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("desc_tree_widget");

    auto val = settings.value("searchColumn");
    if (!val.isNull())
        m_searchColumn = val.toInt();

    settings.endGroup();
}

void EasyDescTreeWidget::saveSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("desc_tree_widget");

    settings.setValue("searchColumn", m_searchColumn);

    settings.endGroup();
}

//////////////////////////////////////////////////////////////////////////

int EasyDescTreeWidget::findNext(const QString& _str)
{
    if (_str.isEmpty())
    {
        for (auto item : m_items)
        {
            for (int i = 0; i < DESC_COL_COLUMNS_NUMBER; ++i)
                item->setBackground(i, Qt::NoBrush);
        }

        return 0;
    }

    const bool isNewSearch = (m_lastSearch != _str);
    auto itemsList = findItems(_str, Qt::MatchContains | Qt::MatchRecursive, m_searchColumn);

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
        for (auto item : m_items)
        {
            for (int i = 0; i < DESC_COL_COLUMNS_NUMBER; ++i)
                item->setBackground(i, Qt::NoBrush);
        }

        m_lastSearch = _str;
        m_lastFound = !itemsList.empty() ? itemsList.front() : nullptr;

        for (auto item : itemsList)
        {
            if (item->parent() != nullptr) for (int i = 0; i < DESC_COL_COLUMNS_NUMBER; ++i)
                item->setBackgroundColor(i, QColor::fromRgba(0x80000000 | (0x00ffffff & ::profiler::colors::Yellow)));
        }
    }

    if (m_lastFound != nullptr)
    {
        scrollToItem(m_lastFound, QAbstractItemView::PositionAtCenter);
        setCurrentItem(m_lastFound);
    }

    return itemsList.size();
}

int EasyDescTreeWidget::findPrev(const QString& _str)
{
    if (_str.isEmpty())
    {
        for (auto item : m_items)
        {
            for (int i = 0; i < DESC_COL_COLUMNS_NUMBER; ++i)
                item->setBackground(i, Qt::NoBrush);
        }

        return 0;
    }

    const bool isNewSearch = (m_lastSearch != _str);
    auto itemsList = findItems(_str, Qt::MatchContains | Qt::MatchRecursive, m_searchColumn);

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
        for (auto item : m_items)
        {
            for (int i = 0; i < DESC_COL_COLUMNS_NUMBER; ++i)
                item->setBackground(i, Qt::NoBrush);
        }

        m_lastSearch = _str;
        m_lastFound = !itemsList.empty() ? itemsList.front() : nullptr;

        for (auto item : itemsList)
        {
            if (item->parent() != nullptr) for (int i = 0; i < DESC_COL_COLUMNS_NUMBER; ++i)
                item->setBackgroundColor(i, QColor::fromRgba(0x80000000 | (0x00ffffff & ::profiler::colors::Yellow)));
        }
    }

    if (m_lastFound != nullptr)
    {
        scrollToItem(m_lastFound, QAbstractItemView::PositionAtCenter);
        setCurrentItem(m_lastFound);
    }

    return itemsList.size();
}

//////////////////////////////////////////////////////////////////////////

EasyDescWidget::EasyDescWidget(QWidget* _parent) : Parent(_parent)
    , m_tree(new EasyDescTreeWidget())
    , m_searchBox(new QLineEdit())
    , m_foundNumber(new QLabel("Found 0 matches"))
{
    m_searchBox->setMinimumWidth(64);

    auto searchbox = new QHBoxLayout();
    searchbox->setContentsMargins(0, 0, 0, 0);
    searchbox->addWidget(new QLabel("Search:"));
    searchbox->addWidget(m_searchBox);
    searchbox->addStretch(50);
    searchbox->addWidget(m_foundNumber, Qt::AlignRight);

    auto lay = new QVBoxLayout(this);
    lay->setContentsMargins(1, 1, 1, 1);
    lay->addLayout(searchbox);
    lay->addWidget(m_tree);

    connect(m_searchBox, &QLineEdit::returnPressed, this, &This::onSeachBoxReturnPressed);
}

EasyDescWidget::~EasyDescWidget()
{

}

void EasyDescWidget::keyPressEvent(QKeyEvent* _event)
{
    if (_event->key() == Qt::Key_F3)
    {
        int matches = 0;
        if (_event->modifiers() & Qt::ShiftModifier)
            matches = m_tree->findPrev(m_searchBox->text());
        else
            matches = m_tree->findNext(m_searchBox->text());

        if (matches == 1)
            m_foundNumber->setText(QString("Found 1 match"));
        else
            m_foundNumber->setText(QString("Found %1 matches").arg(matches));
    }

    _event->accept();
}

void EasyDescWidget::build()
{
    m_tree->build();
}

void EasyDescWidget::onSeachBoxReturnPressed()
{
    auto matches = m_tree->findNext(m_searchBox->text());

    if (matches == 1)
        m_foundNumber->setText(QString("Found 1 match"));
    else
        m_foundNumber->setText(QString("Found %1 matches").arg(matches));
}

//////////////////////////////////////////////////////////////////////////
