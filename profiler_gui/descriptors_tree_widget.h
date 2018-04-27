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
*                   :
*                   : Licensed under the Apache License, Version 2.0 (the "License");
*                   : you may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
*                   :
*                   :
*                   : GNU General Public License Usage
*                   : Alternatively, this file may be used under the terms of the GNU
*                   : General Public License as published by the Free Software Foundation,
*                   : either version 3 of the License, or (at your option) any later version.
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
#include <unordered_set>
#include "easy/profiler.h"

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
    typedef ::std::vector<QTreeWidgetItem*> TreeItems;
    typedef ::std::unordered_set<::std::string> ExpandedFiles;

protected:

    ExpandedFiles    m_expandedFilesTemp;
    Items                        m_items;
    TreeItems           m_highlightItems;
    QString                 m_lastSearch;
    QTreeWidgetItem*         m_lastFound;
    int               m_lastSearchColumn;
    int                   m_searchColumn;
    bool                       m_bLocked;

public:

    // Public virtual methods

    explicit EasyDescTreeWidget(QWidget* _parent = nullptr);
    virtual ~EasyDescTreeWidget();
    void contextMenuEvent(QContextMenuEvent* _event) override;

public:

    // Public non-virtual methods

    int findNext(const QString& _str, Qt::MatchFlags _flags);
    int findPrev(const QString& _str, Qt::MatchFlags _flags);

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

    void resetHighlight();
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
    bool   m_bCaseSensitiveSearch;

public:

    // Public virtual methods

    explicit EasyDescWidget(QWidget* _parent = nullptr);
    virtual ~EasyDescWidget();
    void keyPressEvent(QKeyEvent* _event) override;
    void contextMenuEvent(QContextMenuEvent* _event) override;

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

private:

    // Private non-virtual slots

    void loadSettings();
    void saveSettings();

}; // END of class EasyDescWidget.

//////////////////////////////////////////////////////////////////////////

#endif // EASY__DESCRIPTORS__WIDGET__H_
