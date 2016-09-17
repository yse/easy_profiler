/************************************************************************
* file name         : main_window.h
* ----------------- :
* creation time     : 2016/06/26
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of MainWindow for easy_profiler GUI.
* ----------------- :
* change log        : * 2016/06/26 Victor Zarubkin: initial commit.
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

#ifndef EASY_PROFILER_GUI__MAIN_WINDOW__H
#define EASY_PROFILER_GUI__MAIN_WINDOW__H

#include <string>
#include <thread>
#include <atomic>
#include <QMainWindow>
#include <QTimer>
#include "profiler/reader.h"

//////////////////////////////////////////////////////////////////////////

#define EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW 0

class QDockWidget;

//////////////////////////////////////////////////////////////////////////

class EasyFileReader final
{
    ::profiler::SerializedData      m_serializedBlocks; ///< 
    ::profiler::SerializedData m_serializedDescriptors; ///< 
    ::profiler::descriptors_list_t       m_descriptors; ///< 
    ::profiler::blocks_t                      m_blocks; ///< 
    ::profiler::thread_blocks_tree_t      m_blocksTree; ///< 
    QString                                 m_filename; ///< 
    ::std::thread                             m_thread; ///< 
    ::std::atomic_bool                         m_bDone; ///< 
    ::std::atomic<int>                      m_progress; ///< 
    ::std::atomic<unsigned int>                 m_size; ///< 

public:

    EasyFileReader();
    ~EasyFileReader();

    bool done() const;
    int progress() const;
    unsigned int size() const;
    const QString& filename() const;

    void load(const QString& _filename);
    void interrupt();
    void get(::profiler::SerializedData& _serializedBlocks, ::profiler::SerializedData& _serializedDescriptors,
             ::profiler::descriptors_list_t& _descriptors, ::profiler::blocks_t& _blocks, ::profiler::thread_blocks_tree_t& _tree,
             QString& _filename);

}; // END of class EasyFileReader.

//////////////////////////////////////////////////////////////////////////

class EasyMainWindow : public QMainWindow
{
    Q_OBJECT

protected:

    typedef EasyMainWindow This;
    typedef QMainWindow  Parent;

    QString                                 m_lastFile;
    QDockWidget*                          m_treeWidget;
    QDockWidget*                        m_graphicsView;

#if EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW != 0
    QDockWidget*                      m_descTreeWidget;
#endif

    class QProgressDialog*                  m_progress;
    class QAction*                  m_editBlocksAction;
    class QDialog*                    m_descTreeDialog;
    class EasyDescWidget*             m_dialogDescTree;
    QTimer                               m_readerTimer;
    ::profiler::SerializedData      m_serializedBlocks;
    ::profiler::SerializedData m_serializedDescriptors;
    EasyFileReader                            m_reader;

public:

    explicit EasyMainWindow();
    virtual ~EasyMainWindow();

    // Public virtual methods

    void closeEvent(QCloseEvent* close_event) override;

protected slots:

    void onOpenFileClicked(bool);
    void onReloadFileClicked(bool);
    void onExitClicked(bool);
    void onEncodingChanged(bool);
    void onChronoTextPosChanged(bool);
    void onEnableDisableStatistics(bool);
    void onDrawBordersChanged(bool);
    void onCollapseItemsAfterCloseChanged(bool);
    void onAllItemsExpandedByDefaultChange(bool);
    void onBindExpandStatusChange(bool);
    void onExpandAllClicked(bool);
    void onCollapseAllClicked(bool);
    void onFileReaderTimeout();
    void onFileReaderCancel();
    void onEditBlocksClicked(bool);
    void onDescTreeDialogClose(int);

private:

    // Private non-virtual methods

    void loadFile(const QString& filename);

    void loadSettings();
    void loadGeometry();
    void saveSettingsAndGeometry();

}; // END of class EasyMainWindow.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_GUI__MAIN_WINDOW__H
