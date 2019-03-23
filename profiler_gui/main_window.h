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
*                   : Copyright(C) 2016-2018  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
*                   :
*                   : The MIT License
*                   :
*                   : Permission is hereby granted, free of charge, to any person obtaining a copy
*                   : of this software and associated documentation files (the "Software"), to deal
*                   : in the Software without restriction, including without limitation the rights 
*                   : to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
*                   : of the Software, and to permit persons to whom the Software is furnished 
*                   : to do so, subject to the following conditions:
*                   : 
*                   : The above copyright notice and this permission notice shall be included in all 
*                   : copies or substantial portions of the Software.
*                   : 
*                   : THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
*                   : INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR 
*                   : PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE 
*                   : LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
*                   : TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE 
*                   : USE OR OTHER DEALINGS IN THE SOFTWARE.
*                   : 
*                   : The Apache License, Version 2.0 (the "License")
*                   :
*                   : You may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
************************************************************************/

#ifndef EASY_PROFILER_GUI__MAIN_WINDOW__H
#define EASY_PROFILER_GUI__MAIN_WINDOW__H

#include <atomic>
#include <sstream>
#include <string>
#include <thread>

#include <QMainWindow>
#include <QDockWidget>
#include <QTimer>
#include <QStringList>

#include <easy/easy_socket.h>
#include <easy/reader.h>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//////////////////////////////////////////////////////////////////////////

#define EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW 0

namespace profiler { namespace net { struct EasyProfilerStatus; } }

//////////////////////////////////////////////////////////////////////////

class FileReader Q_DECL_FINAL
{
    enum class JobType : int8_t
    {
        Idle=0,
        Loading,
        Saving,
    };

    profiler::SerializedData      m_serializedBlocks; ///< 
    profiler::SerializedData m_serializedDescriptors; ///< 
    profiler::descriptors_list_t       m_descriptors; ///< 
    profiler::blocks_t                      m_blocks; ///< 
    profiler::thread_blocks_tree_t      m_blocksTree; ///<
    profiler::bookmarks_t                m_bookmarks; ///<
    profiler::BeginEndTime            m_beginEndTime; ///<
    std::stringstream                       m_stream; ///< 
    std::stringstream                 m_errorMessage; ///< 
    QString                               m_filename; ///<
    profiler::processid_t                  m_pid = 0; ///<
    uint32_t           m_descriptorsNumberInFile = 0; ///<
    uint32_t                           m_version = 0; ///<
    std::thread                             m_thread; ///< 
    std::atomic_bool                         m_bDone; ///< 
    std::atomic<int>                      m_progress; ///< 
    std::atomic<unsigned int>                 m_size; ///<
    JobType                m_jobType = JobType::Idle; ///<
    bool                            m_isFile = false; ///<
    bool                        m_isSnapshot = false; ///< 

public:

    FileReader();
    ~FileReader();

    const bool isFile() const;
    const bool isSaving() const;
    const bool isLoading() const;
    const bool isSnapshot() const;

    bool done() const;
    int progress() const;
    unsigned int size() const;
    const QString& filename() const;

    void load(const QString& _filename);
    void load(std::stringstream& _stream);

    /** \brief Save data to file.
    */
    void save(const QString& _filename, profiler::timestamp_t _beginTime, profiler::timestamp_t _endTime,
              const profiler::SerializedData& _serializedDescriptors, const profiler::descriptors_list_t& _descriptors,
              profiler::block_id_t descriptors_count, const profiler::thread_blocks_tree_t& _trees,
              const profiler::bookmarks_t& bookmarks, profiler::block_getter_fn block_getter,
              profiler::processid_t _pid, bool snapshotMode);

    void interrupt();
    void get(profiler::SerializedData& _serializedBlocks, profiler::SerializedData& _serializedDescriptors,
             profiler::descriptors_list_t& _descriptors, profiler::blocks_t& _blocks, profiler::thread_blocks_tree_t& _trees,
             profiler::bookmarks_t& bookmarks, profiler::BeginEndTime& beginEndTime, uint32_t& _descriptorsNumberInFile,
             uint32_t& _version, profiler::processid_t& _pid, QString& _filename);

    void join();

    QString getError();

}; // END of class FileReader.

//////////////////////////////////////////////////////////////////////////

enum class ListenerRegime : uint8_t
{
    Idle = 0,
    Capture,
    Capture_Receive,
    Descriptors
};

class SocketListener Q_DECL_FINAL
{
    EasySocket            m_easySocket; ///<
    std::string              m_address; ///<
    std::stringstream   m_receivedData; ///<
    std::thread               m_thread; ///<
    uint64_t            m_receivedSize; ///<
    uint16_t                    m_port; ///<
    std::atomic<uint32_t>   m_frameMax; ///<
    std::atomic<uint32_t>   m_frameAvg; ///<
    std::atomic_bool      m_bInterrupt; ///<
    std::atomic_bool      m_bConnected; ///<
    std::atomic_bool    m_bStopReceive; ///<
    std::atomic_bool   m_bCaptureReady; ///<
    std::atomic_bool m_bFrameTimeReady; ///<
    ListenerRegime            m_regime; ///<

public:

    SocketListener();
    ~SocketListener();

    bool connected() const;
    bool captured() const;
    ListenerRegime regime() const;
    uint64_t size() const;
    const std::string& address() const;
    uint16_t port() const;

    std::stringstream& data();
    void clearData();

    void disconnect();
    void closeSocket();
    bool connect(const char* _ipaddress, uint16_t _port, profiler::net::EasyProfilerStatus& _reply, bool _disconnectFirst = false);
    bool reconnect(const char* _ipaddress, uint16_t _port, profiler::net::EasyProfilerStatus& _reply);

    bool startCapture();
    void stopCapture();
    void finalizeCapture();
    void requestBlocksDescription();

    bool frameTime(uint32_t& _maxTime, uint32_t& _avgTime);
    bool requestFrameTime();

    template <class T>
    void send(const T& _message) {
        m_easySocket.send(&_message, sizeof(T));
    }

private:

    void listenCapture();
    void listenDescription();
    void listenFrameTime();

}; // END of class SocketListener.

//////////////////////////////////////////////////////////////////////////

class DockWidget : public QDockWidget
{
    Q_OBJECT

    class QPushButton* m_floatingButton;

public:

    explicit DockWidget(const QString& title, QWidget* parent = nullptr);
    ~DockWidget() override;

private slots:

    void toggleState();
    void onTopLevelChanged();

}; // end of class DockWidget.

struct DialogWithGeometry EASY_FINAL
{
    QByteArray geometry;
    class Dialog* ptr = nullptr;

    void create(QWidget* content, QWidget* parent = nullptr);
    void saveGeometry();
    void restoreGeometry();

}; // end of struct DialogWithGeometry.

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:

    using This = MainWindow;
    using Parent = QMainWindow;

    DialogWithGeometry m_descTreeDialog;

    QStringList                            m_lastFiles;
    QString                                    m_theme;
    QString                              m_lastAddress;

    QDockWidget*                m_treeWidget = nullptr;
    QDockWidget*              m_graphicsView = nullptr;
    QDockWidget*                 m_fpsViewer = nullptr;

#if EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW != 0
    QDockWidget*                      m_descTreeWidget = nullptr;
#endif

    class QProgressDialog*        m_progress = nullptr;
    class BlockDescriptorsWidget* m_dialogDescTree = nullptr;
    class Dialog*                 m_listenerDialog = nullptr;
    QTimer                               m_readerTimer;
    QTimer                             m_listenerTimer;
    QTimer                           m_fpsRequestTimer;
    profiler::SerializedData        m_serializedBlocks;
    profiler::SerializedData   m_serializedDescriptors;
    profiler::BeginEndTime              m_beginEndTime;
    FileReader                                m_reader;
    SocketListener                          m_listener;

    class QLineEdit*   m_addressEdit = nullptr;
    class QLineEdit*      m_portEdit = nullptr;
    class QLineEdit* m_frameTimeEdit = nullptr;

    class QMenu*   m_loadActionMenu = nullptr;
    class QAction*     m_saveAction = nullptr;
    class QAction*   m_deleteAction = nullptr;

    class QAction*              m_captureAction = nullptr;
    class QAction*              m_connectAction = nullptr;
    class QAction*   m_eventTracingEnableAction = nullptr;
    class QAction* m_eventTracingPriorityAction = nullptr;

    uint32_t m_descriptorsNumberInFile = 0;
    uint16_t                m_lastPort = 0;
    bool      m_bNetworkFileRegime = false;
    bool        m_bOpenedCacheFile = false;
    bool         m_bCloseAfterSave = false;

public:

    explicit MainWindow();
    ~MainWindow() override;

    // Public virtual methods

    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* close_event) override;
    void changeEvent(QEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* drag_event) override;
    void dragMoveEvent(QDragMoveEvent* drag_event) override;
    void dragLeaveEvent(QDragLeaveEvent* drag_event) override;
    void dropEvent(QDropEvent* drop_event) override;

signals:
    void activationChanged();

protected slots:

    void onThemeChange(bool);
    void onOpenFileClicked(bool);
    void onSaveFileClicked(bool);
    void onDeleteClicked(bool);
    void onExitClicked(bool);
    void onEncodingChanged(bool);
    void onRulerTextPosChanged(bool);
    void onUnitsChanged(bool);
    void onEnableDisableStatistics(bool);
    void onCollapseItemsAfterCloseChanged(bool);
    void onAllItemsExpandedByDefaultChange(bool);
    void onBindExpandStatusChange(bool);
    void onHierarchyFlagChange(bool);
    void onExpandAllClicked(bool);
    void onCollapseAllClicked(bool);
    void onViewportInfoClicked(bool);
    void onSpacingChange(int _value);
    void onMinSizeChange(int _value);
    void onNarrowSizeChange(int _value);
    void onFpsIntervalChange(int _value);
    void onFpsHistoryChange(int _value);
    void onFpsMonitorLineWidthChange(int _value);
    void onFileReaderTimeout();
    void onFrameTimeRequestTimeout();
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
    void onFrameTimeChanged();
    void onSnapshotClicked(bool);
    void onCustomWindowHeaderTriggered(bool _checked);
    void onRightWindowHeaderPosition(bool _checked);
    void onLeftWindowHeaderPosition(bool _checked);

    void onBlockStatusChange(profiler::block_id_t _id, profiler::EasyBlockStatus _status);

    void onSelectValue(profiler::thread_id_t _thread_id, uint32_t _value_index, const profiler::ArbitraryValue& _value);

    void checkFrameTimeReady();

    void validateLastDir();

private:

    // Private non-virtual methods

    void closeProgressDialogAndClearReader();
    void onLoadingFinish(profiler::block_index_t& _nblocks);
    void onSavingFinish();

    void configureSizes();

    void clear();

    void refreshDiagram();

    void addFileToList(const QString& filename, bool changeWindowTitle = true);
    void loadFile(const QString& filename);
    void readStream(std::stringstream& data);

    void loadSettings();
    void loadGeometry();
    void saveSettingsAndGeometry();

    void setDisconnected(bool _showMessage = true);

    void destroyProgressDialog();
    void createProgressDialog(const QString& text);

    void validateLineEdits();

}; // END of class MainWindow.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_GUI__MAIN_WINDOW__H
