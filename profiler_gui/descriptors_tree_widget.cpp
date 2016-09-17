/************************************************************************
* file name         : descriptors_tree_widget.cpp
* ----------------- :
* creation time     : 2016/09/17
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of EasyDescWidget and it's auxiliary classes
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
#include <QSignalBlocker>
#include <QSettings>
#include <QDebug>
#include <thread>
#include "descriptors_tree_widget.h"
#include "globals.h"
#include "../src/hashed_cstr.h"

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

EasyDescWidget::EasyDescWidget(QWidget* _parent)
    : Parent(_parent)
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
    connect(this, &Parent::itemExpanded, this, &This::onItemExpand);
    connect(this, &Parent::itemDoubleClicked, this, &This::onDoubleClick);

    loadSettings();
}

EasyDescWidget::~EasyDescWidget()
{
    saveSettings();
}

//////////////////////////////////////////////////////////////////////////

void EasyDescWidget::clearSilent(bool)
{
    const QSignalBlocker b(this);

    setSortingEnabled(false);

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
    typedef ::std::unordered_map<int, QTreeWidgetItem*, ::profiler_gui::do_no_hash<int>::hasher_t> Items;
    Items        children;
    QTreeWidgetItem* item = nullptr;
};

void EasyDescWidget::build()
{
    clearSilent(false);

    auto f = header()->font();
    f.setBold(true);

    typedef ::std::unordered_map<::std::string, FileItems> Files;
    Files m_files;

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
                if (*desc->name() != 0)
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

void EasyDescWidget::onItemExpand(QTreeWidgetItem*)
{
    resizeColumnsToContents();
}

//////////////////////////////////////////////////////////////////////////

void EasyDescWidget::onDoubleClick(QTreeWidgetItem* _item, int _column)
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
        emit EASY_GLOBALS.events.enableStatusChanged(item->desc(), desc.enabled());
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyDescWidget::resizeColumnsToContents()
{
    for (int i = 0; i < DESC_COL_COLUMNS_NUMBER; ++i)
    {
        resizeColumnToContents(i);
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyDescWidget::contextMenuEvent(QContextMenuEvent* _event)
{
    _event->accept();
}

//////////////////////////////////////////////////////////////////////////

void EasyDescWidget::onSelectedBlockChange(uint32_t _block_index)
{

}

//////////////////////////////////////////////////////////////////////////

void EasyDescWidget::loadSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("desc_tree_widget");

    // ...

    settings.endGroup();
}

void EasyDescWidget::saveSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("desc_tree_widget");

    // ...

    settings.endGroup();
}

//////////////////////////////////////////////////////////////////////////
