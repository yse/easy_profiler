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
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include "profiler/reader.h"
#include <sstream>

//////////////////////////////////////////////////////////////////////////

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
class EasyMainWindow;
class TcpReceiverThread : public QThread
{
    Q_OBJECT
        EasyMainWindow* mainwindow;
public:
    QTcpSocket * m_server;
    explicit TcpReceiverThread(QObject *parent, EasyMainWindow* mw);

    void run() Q_DECL_OVERRIDE;
public slots:

    void readTcpData();
    void onConnected();
signals:
    void resultReady(const QString &s);
};

class EasyMainWindow : public QMainWindow
{
    Q_OBJECT
        friend class TcpReceiverThread;
protected:

    typedef EasyMainWindow This;
    typedef QMainWindow  Parent;

    QString                                 m_lastFile;
    QDockWidget*                          m_treeWidget;
    QDockWidget*                        m_graphicsView;
    class QProgressDialog*                  m_progress;
    QTimer                               m_readerTimer;
    ::profiler::SerializedData      m_serializedBlocks;
    ::profiler::SerializedData m_serializedDescriptors;
    EasyFileReader                            m_reader;

    QTcpSocket* m_server = nullptr;

    std::stringstream m_receivedProfileData;
    bool m_recFrames = false;

    QLineEdit* m_hostString = nullptr;
    QLineEdit* m_portString = nullptr;
    bool m_isConnected = false;

    TcpReceiverThread* m_receiver;

    std::thread m_thread;
public:

    EasyMainWindow();
    virtual ~EasyMainWindow();

    // Public virtual methods

    void closeEvent(QCloseEvent* close_event) override;

    void listen();

protected slots:

    void onOpenFileClicked(bool);
    void onReloadFileClicked(bool);
    void onExitClicked(bool);
    void onEncodingChanged(bool);
    void onChronoTextPosChanged(bool);
    void onDrawBordersChanged(bool);
    void onCollapseItemsAfterCloseChanged(bool);
    void onAllItemsExpandedByDefaultChange(bool);
    void onBindExpandStatusChange(bool);
    void onExpandAllClicked(bool);
    void onCollapseAllClicked(bool);
    void onFileReaderTimeout();
    void onFileReaderCancel();
    void onCaptureClicked(bool);

    void readTcpData();
    void onNewConnection();
    void onDisconnection();
    void onConnected();
    void onErrorConnection(QAbstractSocket::SocketError socketError);
    void onDisconnect();
    void onConnectClicked(bool);

    void handleResults(const QString &s);
private:

    // Private non-virtual methods

    void loadFile(const QString& filename);

    void loadSettings();
    void loadGeometry();
    void saveSettingsAndGeometry();

    bool m_isClientPreparedBlocks = false;
    bool m_isClientCaptured = false;

}; // END of class EasyMainWindow.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_GUI__MAIN_WINDOW__H
