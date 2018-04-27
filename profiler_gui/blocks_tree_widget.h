/************************************************************************
* file name         : blocks_tree_widget.h
* ----------------- : 
* creation time     : 2016/06/26
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- : 
* description       : The file contains declaration of EasyTreeWidget and it's auxiliary classes
*                   : for displyaing EasyProfiler blocks tree.
* ----------------- : 
* change log        : * 2016/06/26 Victor Zarubkin: moved sources from tree_view.h
*                   :       and renamed classes from My* to Prof*.
*                   :
*                   : * 2016/06/27 Victor Zarubkin: Added possibility to colorize rows
*                   :       with profiler blocks' colors.
*                   :
*                   : * 2016/06/29 Victor Zarubkin: Added clearSilent() method.
*                   :
*                   : * 2016/08/18 Victor Zarubkin: Added loading blocks hierarchy in separate thread;
*                   :       Moved sources of TreeWidgetItem into tree_widget_item.h/.cpp
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

#ifndef EASY__TREE_WIDGET__H_
#define EASY__TREE_WIDGET__H_

#include <QTreeWidget>
#include <QTimer>
#include "tree_widget_loader.h"
#include "tree_widget_item.h"
#include "easy/reader.h"

//////////////////////////////////////////////////////////////////////////

class EasyTreeWidget : public QTreeWidget
{
    Q_OBJECT

    typedef QTreeWidget    Parent;
    typedef EasyTreeWidget   This;

protected:

    EasyTreeWidgetLoader  m_hierarchyBuilder;
    Items                            m_items;
    RootsMap                         m_roots;
    ::profiler_gui::TreeBlocks m_inputBlocks;
    QTimer                       m_fillTimer;
    QString                     m_lastSearch;
    QTreeWidgetItem*             m_lastFound;
    ::profiler::timestamp_t      m_beginTime;
    class QProgressDialog*        m_progress;
    EasyTreeMode                      m_mode;
    bool                        m_bColorRows;
    bool                           m_bLocked;
    bool             m_bSilentExpandCollapse;
    char m_columnsHiddenStatus[COL_COLUMNS_NUMBER];

public:

    explicit EasyTreeWidget(QWidget* _parent = nullptr);
    virtual ~EasyTreeWidget();

    void contextMenuEvent(QContextMenuEvent* _event) override;

    void clearSilent(bool _global = false);
    int findNext(const QString& _str, Qt::MatchFlags _flags);
    int findPrev(const QString& _str, Qt::MatchFlags _flags);

public slots:

    void setTree(const unsigned int _blocksNumber, const ::profiler::thread_blocks_tree_t& _blocksTree);

    void setTreeBlocks(const ::profiler_gui::TreeBlocks& _blocks, ::profiler::timestamp_t _session_begin_time, ::profiler::timestamp_t _left, ::profiler::timestamp_t _right, bool _strict);

protected:

    void resizeEvent(QResizeEvent* _event) override;
    void moveEvent(QMoveEvent* _event) override;

private slots:

    void onJumpToItemClicked(bool);

    void onCollapseAllClicked(bool);

    void onExpandAllClicked(bool);

    void onCollapseAllChildrenClicked(bool);

    void onExpandAllChildrenClicked(bool);

    void onItemExpand(QTreeWidgetItem* _item);
    void onItemCollapse(QTreeWidgetItem* _item);
    void onCurrentItemChange(QTreeWidgetItem* _item, QTreeWidgetItem*);

    void onColorizeRowsTriggered(bool _colorize);

    void onSelectedThreadChange(::profiler::thread_id_t _id);

    void onSelectedBlockChange(uint32_t _block_index);

    void onBlockStatusChangeClicked(bool);

    void resizeColumnsToContents();

    void onHideShowColumn(bool);
    void onModeChange(bool);

    void onFillTimerTimeout();

protected:

    void loadSettings();
    void saveSettings();
    void alignProgressBar();

}; // END of class EasyTreeWidget.

//////////////////////////////////////////////////////////////////////////

class EasyHierarchyWidget : public QWidget
{
    Q_OBJECT

    typedef QWidget           Parent;
    typedef EasyHierarchyWidget This;

private:

    EasyTreeWidget*                 m_tree;
    class QLineEdit*           m_searchBox;
    class QLabel*            m_foundNumber;
    class QAction*          m_searchButton;
    bool            m_bCaseSensitiveSearch;

public:

    // Public virtual methods

    explicit EasyHierarchyWidget(QWidget* _parent = nullptr);
    virtual ~EasyHierarchyWidget();
    void keyPressEvent(QKeyEvent* _event) override;
    void contextMenuEvent(QContextMenuEvent* _event) override;

public:

    // Public non-virtual methods

    EasyTreeWidget* tree();
    void clear(bool _global = false);

private slots:

    // Private slots

    void onSeachBoxReturnPressed();
    void findNext(bool);
    void findPrev(bool);
    void findNextFromMenu(bool);
    void findPrevFromMenu(bool);

private:

    // Private non-virtual methods

    void loadSettings();
    void saveSettings();

}; // END of class EasyHierarchyWidget.


//////////////////////////////////////////////////////////////////////////

#endif // EASY__TREE_WIDGET__H_
