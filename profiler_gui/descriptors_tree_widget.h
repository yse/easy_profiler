/************************************************************************
* file name         : descriptors_tree_widget.h
* ----------------- : 
* creation time     : 2016/09/17
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- : 
* description       : The file contains declaration of EasyDescTreeWidget and it's auxiliary classes
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
#include <QString>
#include <vector>
#include "profiler/profiler.h"

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

class EasyDescTreeWidget : public QTreeWidget
{
    Q_OBJECT

    typedef QTreeWidget    Parent;
    typedef EasyDescTreeWidget   This;

    typedef ::std::vector<EasyDescWidgetItem*> Items;

protected:

    Items                 m_items;
    QString          m_lastSearch;
    QTreeWidgetItem*  m_lastFound;
    int            m_searchColumn;
    bool                m_bLocked;

public:

    // Public virtual methods

    explicit EasyDescTreeWidget(QWidget* _parent = nullptr);
    virtual ~EasyDescTreeWidget();
    void contextMenuEvent(QContextMenuEvent* _event) override;
    void keyPressEvent(QKeyEvent* _event) override;

public:

    // Public non-virtual methods

    int findNext(const QString& _str);
    int findPrev(const QString& _str);

public slots:

    void clearSilent(bool _global = false);
    void build();

private slots:

    void onSearchColumnChange(bool);
    void onBlockStatusChangeClicked(bool);
    void onCurrentItemChange(QTreeWidgetItem* _item, QTreeWidgetItem* _prev);
    void onItemExpand(QTreeWidgetItem* _item);
    void onDoubleClick(QTreeWidgetItem* _item, int _column);
    void onSelectedBlockChange(uint32_t _block_index);
    void onBlockStatusChange(::profiler::block_id_t _id, ::profiler::EasyBlockStatus _status);
    void resizeColumnsToContents();

private:

    // Private methods

    void loadSettings();
    void saveSettings();

}; // END of class EasyDescTreeWidget.

//////////////////////////////////////////////////////////////////////////

class EasyDescWidget : public QWidget
{
    Q_OBJECT

    typedef QWidget      Parent;
    typedef EasyDescWidget This;

private:

    EasyDescTreeWidget*    m_tree;
    class QLineEdit*  m_searchBox;
    class QLabel*   m_foundNumber;
    class QAction* m_searchButton;

public:

    // Public virtual methods

    explicit EasyDescWidget(QWidget* _parent = nullptr);
    virtual ~EasyDescWidget();
    void keyPressEvent(QKeyEvent* _event) override;

public:

    // Public non-virtual methods

    void build();
    void clear();

private slots:

    void onSeachBoxReturnPressed();
    void findNext(bool);
    void findPrev(bool);
    void findNextFromMenu(bool);
    void findPrevFromMenu(bool);

}; // END of class EasyDescWidget.

//////////////////////////////////////////////////////////////////////////

#endif // EASY__DESCRIPTORS__WIDGET__H_
