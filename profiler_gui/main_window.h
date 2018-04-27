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

#ifndef EASY_PROFILER_GUI__MAIN_WINDOW__H
#define EASY_PROFILER_GUI__MAIN_WINDOW__H

#include <string>
#include <thread>
#include <atomic>
#include <sstream>

#include <QMainWindow>
#include <QTimer>
#include <QStringList>

#include "easy/easy_socket.h"
#include "easy/reader.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//////////////////////////////////////////////////////////////////////////

#define EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW 0

class QDockWidget;

namespace profiler { namespace net { struct EasyProfilerStatus; } }

//////////////////////////////////////////////////////////////////////////

class EasyFileReader Q_DECL_FINAL
{
    ::profiler::SerializedData      m_serializedBlocks; ///< 
    ::profiler::SerializedData m_serializedDescriptors; ///< 
    ::profiler::descriptors_list_t       m_descriptors; ///< 
    ::profiler::blocks_t                      m_blocks; ///< 
    ::profiler::thread_blocks_tree_t      m_blocksTree; ///< 
    ::std::stringstream                       m_stream; ///< 
    ::std::stringstream                 m_errorMessage; ///< 
    QString                                 m_filename; ///< 
    uint32_t             m_descriptorsNumberInFile = 0; ///< 
    ::std::thread                             m_thread; ///< 
    ::std::atomic_bool                         m_bDone; ///< 
    ::std::atomic<int>                      m_progress; ///< 
    ::std::atomic<unsigned int>                 m_size; ///< 
    bool                              m_isFile = false; ///< 

public:

    EasyFileReader();
    ~EasyFileReader();

    const bool isFile() const;
    bool done() const;
    int progress() const;
    unsigned int size() const;
    const QString& filename() const;

    void load(const QString& _filename);
    void load(::std::stringstream& _stream);
    void interrupt();
    void get(::profiler::SerializedData& _serializedBlocks, ::profiler::SerializedData& _serializedDescriptors,
             ::profiler::descriptors_list_t& _descriptors, ::profiler::blocks_t& _blocks, ::profiler::thread_blocks_tree_t& _tree,
             uint32_t& _descriptorsNumberInFile, QString& _filename);

    QString getError();

}; // END of class EasyFileReader.

//////////////////////////////////////////////////////////////////////////

enum EasyListenerRegime : uint8_t
{
    LISTENER_IDLE = 0,
    LISTENER_CAPTURE,
    LISTENER_DESCRIBE
};

class EasySocketListener Q_DECL_FINAL
{
    EasySocket            m_easySocket; ///< 
    ::std::string            m_address; ///< 
    ::std::stringstream m_receivedData; ///< 
    ::std::thread             m_thread; ///< 
    uint64_t            m_receivedSize; ///< 
    uint16_t                    m_port; ///< 
    ::std::atomic_bool    m_bInterrupt; ///< 
    ::std::atomic_bool    m_bConnected; ///< 
    ::std::atomic_bool  m_bStopReceive; ///< 
    EasyListenerRegime        m_regime; ///< 

public:

    EasySocketListener();
    ~EasySocketListener();

    bool connected() const;
    EasyListenerRegime regime() const;
    uint64_t size() const;
    const ::std::string& address() const;
    uint16_t port() const;

    ::std::stringstream& data();
    void clearData();

    bool connect(const char* _ipaddress, uint16_t _port, ::profiler::net::EasyProfilerStatus& _reply);

    bool startCapture();
    void stopCapture();
    void requestBlocksDescription();

    template <class T>
    inline void send(const T& _message) {
        m_easySocket.send(&_message, sizeof(T));
    }

private:

    void listenCapture();
    void listenDescription();

}; // END of class EasySocketListener.

//////////////////////////////////////////////////////////////////////////

class EasyMainWindow : public QMainWindow
{
    Q_OBJECT

protected:

    typedef EasyMainWindow This;
    typedef QMainWindow  Parent;

    QStringList                            m_lastFiles;
    QString                              m_lastAddress;
    QDockWidget*                          m_treeWidget = nullptr;
    QDockWidget*                        m_graphicsView = nullptr;

#if EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW != 0
    QDockWidget*                      m_descTreeWidget = nullptr;
#endif

    class QProgressDialog*                  m_progress = nullptr;
    class QDialog*                    m_descTreeDialog = nullptr;
    class EasyDescWidget*             m_dialogDescTree = nullptr;
    class QMessageBox*                m_listenerDialog = nullptr;
    QTimer                               m_readerTimer;
    QTimer                             m_listenerTimer;
    ::profiler::SerializedData      m_serializedBlocks;
    ::profiler::SerializedData m_serializedDescriptors;
    EasyFileReader                            m_reader;
    EasySocketListener                      m_listener;

    class QLineEdit* m_addressEdit = nullptr;
    class QLineEdit* m_portEdit = nullptr;
    class QLineEdit* m_frameTimeEdit = nullptr;

    class QMenu*   m_loadActionMenu = nullptr;
    class QAction* m_saveAction = nullptr;
    class QAction* m_deleteAction = nullptr;

    class QAction* m_captureAction = nullptr;
    class QAction* m_connectAction = nullptr;
    class QAction* m_eventTracingEnableAction = nullptr;
    class QAction* m_eventTracingPriorityAction = nullptr;

    uint32_t m_descriptorsNumberInFile = 0;
    uint16_t m_lastPort = 0;
    bool m_bNetworkFileRegime = false;

public:

    explicit EasyMainWindow();
    virtual ~EasyMainWindow();

    // Public virtual methods

    void closeEvent(QCloseEvent* close_event) override;
    void dragEnterEvent(QDragEnterEvent* drag_event) override;
    void dragMoveEvent(QDragMoveEvent* drag_event) override;
    void dragLeaveEvent(QDragLeaveEvent* drag_event) override;
    void dropEvent(QDropEvent* drop_event) override;

protected slots:

    void onOpenFileClicked(bool);
    void onSaveFileClicked(bool);
    void onDeleteClicked(bool);
    void onExitClicked(bool);
    void onEncodingChanged(bool);
    void onChronoTextPosChanged(bool);
    void onUnitsChanged(bool);
    void onEventIndicatorsChange(bool);
    void onEnableDisableStatistics(bool);
    void onCollapseItemsAfterCloseChanged(bool);
    void onAllItemsExpandedByDefaultChange(bool);
    void onBindExpandStatusChange(bool);
    void onHierarchyFlagChange(bool);
    void onExpandAllClicked(bool);
    void onCollapseAllClicked(bool);
    void onSpacingChange(int _value);
    void onMinSizeChange(int _value);
    void onNarrowSizeChange(int _value);
    void onFileReaderTimeout();
    void onListenerTimerTimeout();
    void onFileReaderCancel();
    void onEditBlocksClicked(bool);
    void onDescTreeDialogClose(int);
    void onListenerDialogClose(int);
    void onCaptureClicked(bool);
    void onGetBlockDescriptionsClicked(bool);
    void onConnectClicked(bool);
    void onEventTracingPriorityChange(bool _checked);
    void onEventTracingEnableChange(bool _checked);
    void onFrameTimeEditFinish();

    void onBlockStatusChange(::profiler::block_id_t _id, ::profiler::EasyBlockStatus _status);

private:

    // Private non-virtual methods

    void clear();

    void refreshDiagram();

    void addFileToList(const QString& filename);
    void loadFile(const QString& filename);
    void readStream(::std::stringstream& data);

    void loadSettings();
    void loadGeometry();
    void saveSettingsAndGeometry();

    void setDisconnected(bool _showMessage = true);

}; // END of class EasyMainWindow.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_GUI__MAIN_WINDOW__H
