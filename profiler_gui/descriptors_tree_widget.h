/************************************************************************
* file name         : descriptors_tree_widget.h
* ----------------- : 
* creation time     : 2016/09/17
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- : 
* description       : The file contains declaration of EasyDescWidget and it's auxiliary classes
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

#ifndef EASY__DESCRIPTORS__WIDGET__H_
#define EASY__DESCRIPTORS__WIDGET__H_

#include <QTreeWidget>
#include "profiler/reader.h"

//////////////////////////////////////////////////////////////////////////

class EasyDescWidgetItem : public QTreeWidgetItem
{
    typedef QTreeWidgetItem    Parent;
    typedef EasyDescWidgetItem   This;

    ::profiler::block_id_t m_desc;

public:

    explicit EasyDescWidgetItem(::profiler::block_id_t _desc, Parent* _parent = nullptr);
    virtual ~EasyDescWidgetItem();

    bool operator < (const Parent& _other) const override;

public:

    // Public inline methods

    inline ::profiler::block_id_t desc() const
    {
        return m_desc;
    }

}; // END of class EasyDescWidgetItem.

//////////////////////////////////////////////////////////////////////////

class EasyDescWidget : public QTreeWidget
{
    Q_OBJECT

    typedef QTreeWidget    Parent;
    typedef EasyDescWidget   This;

protected:



public:

    explicit EasyDescWidget(QWidget* _parent = nullptr);
    virtual ~EasyDescWidget();

public slots:

    void clearSilent(bool _global = false);
    void build();

protected:

    void contextMenuEvent(QContextMenuEvent* _event) override;

private slots:

    void onItemExpand(QTreeWidgetItem* _item);
    void onDoubleClick(QTreeWidgetItem* _item, int _column);
    void onSelectedBlockChange(uint32_t _block_index);
    void resizeColumnsToContents();

protected:

    void loadSettings();
    void saveSettings();

}; // END of class EasyDescWidget.

//////////////////////////////////////////////////////////////////////////

#endif // EASY__DESCRIPTORS__WIDGET__H_
