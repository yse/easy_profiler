/************************************************************************
* file name         : main_window.cpp
* ----------------- :
* creation time     : 2016/06/26
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of MainWindow for easy_profiler GUI.
* ----------------- :
* change log        : * 2016/06/26 Victor Zarubkin: Initial commit.
*                   :
*                   : * 2016/06/27 Victor Zarubkin: Passing blocks number to EasyTreeWidget::setTree().
*                   :
*                   : * 2016/06/29 Victor Zarubkin: Added menu with tests.
*                   :
*                   : * 2016/06/30 Sergey Yagovtsev: Open file by command line argument
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

#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QSettings>
#include <QTextCodec>
#include <QProgressDialog>
#include <QSignalBlocker>
#include <QDebug>
#include <QToolBar>
#include <QMessageBox>

#include <QDialog>
#include <QLineEdit>
#include <QFormLayout>
#include <QPushButton>

#include "main_window.h"
#include "blocks_tree_widget.h"
#include "blocks_graphics_view.h"
#include "globals.h"

#include "profiler/easy_net.h"

#include <thread>
#include <chrono>
#include <fstream>
//////////////////////////////////////////////////////////////////////////

const int LOADER_TIMER_INTERVAL = 40;

//////////////////////////////////////////////////////////////////////////

EasyMainWindow::EasyMainWindow() : Parent(), m_treeWidget(nullptr), m_graphicsView(nullptr), m_progress(nullptr)
{
    setObjectName("ProfilerGUI_MainWindow");
    setWindowTitle("EasyProfiler Reader v0.2.0");
    setDockNestingEnabled(true);
    resize(800, 600);
    
    setStatusBar(new QStatusBar());

    auto graphicsView = new EasyGraphicsViewWidget();
    m_graphicsView = new QDockWidget("Blocks diagram");
    m_graphicsView->setMinimumHeight(50);
    m_graphicsView->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_graphicsView->setWidget(graphicsView);

    auto treeWidget = new EasyTreeWidget();
    m_treeWidget = new QDockWidget("Blocks hierarchy");
    m_treeWidget->setMinimumHeight(50);
    m_treeWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_treeWidget->setWidget(treeWidget);

    addDockWidget(Qt::TopDockWidgetArea, m_graphicsView);
    addDockWidget(Qt::BottomDockWidgetArea, m_treeWidget);

    QToolBar *fileToolBar = addToolBar(tr("File"));
    QAction *connectAct = new QAction(tr("&Connect"), this);
    QAction *newAct = new QAction(tr("&Capture"), this);
    fileToolBar->addAction(connectAct);
    fileToolBar->addAction(newAct);
    

    connect(newAct, &QAction::triggered, this, &This::onCaptureClicked);
    connect(connectAct, &QAction::triggered, this, &This::onConnectClicked);

    m_hostString = new QLineEdit();
    QRegExp rx("[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}");
    QRegExpValidator regValidator(rx, 0);
    m_hostString->setInputMask("000.000.000.000;");
    m_hostString->setValidator(&regValidator);
    m_hostString->setText("127.0.0.1");

    fileToolBar->addWidget(m_hostString);

    m_portString = new QLineEdit();
    m_portString->setValidator(new QIntValidator(1024, 65536, this));
    m_portString->setText(QString::number(profiler::DEFAULT_PORT));

    fileToolBar->addWidget(m_portString);

    

    m_server = new QTcpSocket( this );
    m_server->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    
    connect(m_server, SIGNAL(readyRead()), SLOT(readTcpData()));
    connect(m_server, SIGNAL(connected()), SLOT(onConnected()));
    connect(m_server, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onErrorConnection(QAbstractSocket::SocketError)), Qt::UniqueConnection);
    connect(m_server, SIGNAL(disconnected()), SLOT(onDisconnect()), Qt::UniqueConnection);

    m_server->connectToHost(m_hostString->text(), m_portString->text().toUShort());
    //TODO:
    //connected
    //error

    /*if (!m_server->listen(QHostAddress(QHostAddress::Any), 28077)) {
            QMessageBox::critical(0,
                                  "Server Error",
                                  "Unable to start the server:"
                                  + m_server->errorString()
                                 );
            m_server->close();
        }

    connect(m_server, SIGNAL(newConnection()),
               this,         SLOT(onNewConnection())
               );
    */
    loadSettings();



    auto menu = new QMenu("&File");

    auto action = menu->addAction("&Open");
    connect(action, &QAction::triggered, this, &This::onOpenFileClicked);

    action = menu->addAction("&Reload");
    connect(action, &QAction::triggered, this, &This::onReloadFileClicked);

    menu->addSeparator();
    action = menu->addAction("&Exit");
    connect(action, &QAction::triggered, this, &This::onExitClicked);

    menuBar()->addMenu(menu);



    menu = new QMenu("&View");

    action = menu->addAction("Expand all");
    connect(action, &QAction::triggered, this, &This::onExpandAllClicked);

    action = menu->addAction("Collapse all");
    connect(action, &QAction::triggered, this, &This::onCollapseAllClicked);

    menu->addSeparator();
    action = menu->addAction("Draw items' borders");
    action->setCheckable(true);
    action->setChecked(EASY_GLOBALS.draw_graphics_items_borders);
    connect(action, &QAction::triggered, this, &This::onDrawBordersChanged);

    action = menu->addAction("Collapse items on tree reset");
    action->setCheckable(true);
    action->setChecked(EASY_GLOBALS.collapse_items_on_tree_close);
    connect(action, &QAction::triggered, this, &This::onCollapseItemsAfterCloseChanged);

    action = menu->addAction("Expand all on file open");
    action->setCheckable(true);
    action->setChecked(EASY_GLOBALS.all_items_expanded_by_default);
    connect(action, &QAction::triggered, this, &This::onAllItemsExpandedByDefaultChange);

    action = menu->addAction("Bind scene and tree expand");
    action->setCheckable(true);
    action->setChecked(EASY_GLOBALS.bind_scene_and_tree_expand_status);
    connect(action, &QAction::triggered, this, &This::onBindExpandStatusChange);

    menu->addSeparator();
    auto submenu = menu->addMenu("Chronometer text");
    auto actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    action = new QAction("At top", actionGroup);
    action->setCheckable(true);
    action->setData(static_cast<int>(::profiler_gui::ChronoTextPosition_Top));
    if (EASY_GLOBALS.chrono_text_position == ::profiler_gui::ChronoTextPosition_Top)
        action->setChecked(true);
    submenu->addAction(action);
    connect(action, &QAction::triggered, this, &This::onChronoTextPosChanged);

    action = new QAction("At center", actionGroup);
    action->setCheckable(true);
    action->setData(static_cast<int>(::profiler_gui::ChronoTextPosition_Center));
    if (EASY_GLOBALS.chrono_text_position == ::profiler_gui::ChronoTextPosition_Center)
        action->setChecked(true);
    submenu->addAction(action);
    connect(action, &QAction::triggered, this, &This::onChronoTextPosChanged);

    action = new QAction("At bottom", actionGroup);
    action->setCheckable(true);
    action->setData(static_cast<int>(::profiler_gui::ChronoTextPosition_Bottom));
    if (EASY_GLOBALS.chrono_text_position == ::profiler_gui::ChronoTextPosition_Bottom)
        action->setChecked(true);
    submenu->addAction(action);
    connect(action, &QAction::triggered, this, &This::onChronoTextPosChanged);

    menuBar()->addMenu(menu);



    menu = new QMenu("&Settings");
    submenu = menu->addMenu("&Encoding");
    actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);

    auto default_codec_mib = QTextCodec::codecForLocale()->mibEnum();
    foreach (int mib, QTextCodec::availableMibs())
    {
        auto codec = QTextCodec::codecForMib(mib)->name();

        action = new QAction(codec, actionGroup);
        action->setCheckable(true);
        if (mib == default_codec_mib)
            action->setChecked(true);

        submenu->addAction(action);
        connect(action, &QAction::triggered, this, &This::onEncodingChanged);
    }

    menuBar()->addMenu(menu);

    connect(graphicsView->view(), &EasyGraphicsView::intervalChanged, treeWidget, &EasyTreeWidget::setTreeBlocks);
    connect(&m_readerTimer, &QTimer::timeout, this, &This::onFileReaderTimeout);


    m_progress = new QProgressDialog("Loading file...", "Cancel", 0, 100, this);
    m_progress->setFixedWidth(300);
    m_progress->setWindowTitle("EasyProfiler");
    m_progress->setModal(true);
    m_progress->setValue(100);
    //m_progress->hide();
    connect(m_progress, &QProgressDialog::canceled, this, &This::onFileReaderCancel);


    loadGeometry();

    if(QCoreApplication::arguments().size() > 1)
    {
        auto opened_filename = QCoreApplication::arguments().at(1);
        loadFile(opened_filename);
    }
}

EasyMainWindow::~EasyMainWindow()
{
    delete m_progress;
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onOpenFileClicked(bool)
{
    auto filename = QFileDialog::getOpenFileName(this, "Open profiler log", m_lastFile, "Profiler Log File (*.prof);;All Files (*.*)");
    loadFile(filename);
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::loadFile(const QString& filename)
{
    const auto i = filename.lastIndexOf(QChar('/'));
    const auto j = filename.lastIndexOf(QChar('\\'));
    m_progress->setLabelText(QString("Loading %1...").arg(filename.mid(::std::max(i, j) + 1)));

    m_progress->setValue(0);
    m_progress->show();
    m_readerTimer.start(LOADER_TIMER_INTERVAL);
    m_reader.load(filename);
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onReloadFileClicked(bool)
{
    if (m_lastFile.isEmpty())
        return;
    loadFile(m_lastFile);
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onExitClicked(bool)
{
    close();
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onEncodingChanged(bool)
{
   auto _sender = qobject_cast<QAction*>(sender());
   auto name = _sender->text();
   QTextCodec *codec = QTextCodec::codecForName(name.toStdString().c_str());
   QTextCodec::setCodecForLocale(codec);
}

void EasyMainWindow::onChronoTextPosChanged(bool)
{
    auto _sender = qobject_cast<QAction*>(sender());
    EASY_GLOBALS.chrono_text_position = static_cast<::profiler_gui::ChronometerTextPosition>(_sender->data().toInt());
    emit EASY_GLOBALS.events.chronoPositionChanged();
}

void EasyMainWindow::onDrawBordersChanged(bool _checked)
{
    EASY_GLOBALS.draw_graphics_items_borders = _checked;
    emit EASY_GLOBALS.events.drawBordersChanged();
}

void EasyMainWindow::onCollapseItemsAfterCloseChanged(bool _checked)
{
    EASY_GLOBALS.collapse_items_on_tree_close = _checked;
}

void EasyMainWindow::onAllItemsExpandedByDefaultChange(bool _checked)
{
    EASY_GLOBALS.all_items_expanded_by_default = _checked;
}

void EasyMainWindow::onBindExpandStatusChange(bool _checked)
{
    EASY_GLOBALS.bind_scene_and_tree_expand_status = _checked;
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onExpandAllClicked(bool)
{
    for (auto& block : EASY_GLOBALS.gui_blocks)
        block.expanded = true;

    emit EASY_GLOBALS.events.itemsExpandStateChanged();

    auto tree = static_cast<EasyTreeWidget*>(m_treeWidget->widget());
    const QSignalBlocker b(tree);
    tree->expandAll();
}

void EasyMainWindow::onCollapseAllClicked(bool)
{
    for (auto& block : EASY_GLOBALS.gui_blocks)
        block.expanded = false;

    emit EASY_GLOBALS.events.itemsExpandStateChanged();

    auto tree = static_cast<EasyTreeWidget*>(m_treeWidget->widget());
    const QSignalBlocker b(tree);
    tree->collapseAll();
}

void EasyMainWindow::onConnectClicked(bool)
{
    if (!m_isConnected)
    {
        qInfo() << "Try connect to: " << m_hostString->text() << ":" << m_portString->text();
        m_server->connectToHost(m_hostString->text(), m_portString->text().toUShort());
    }
}

void EasyMainWindow::onCaptureClicked(bool)
{
    
    if (!m_isConnected)
    {
        m_server->connectToHost(m_hostString->text(), m_portString->text().toUShort());
        QMessageBox::warning(this, "Warning", "No connection with profiling app", QMessageBox::Close);
        return;
    }
    
    
    profiler::net::Message requestMessage(profiler::net::MESSAGE_TYPE_REQUEST_START_CAPTURE);
    m_server->write((const char*)&requestMessage, sizeof(requestMessage));

    /**
    QDialog *dialog = new QDialog();
    dialog->setWindowTitle(tr("Set network parameters"));
    QFormLayout *layout = new QFormLayout;


    QLineEdit* hostString = new QLineEdit();
    QRegExp rx("[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}");
    QRegExpValidator regValidator(rx, 0);
    hostString->setInputMask("000.000.000.000;");
    hostString->setValidator(&regValidator);
    hostString->setText("127.0.0.1");

    QLineEdit* portString = new QLineEdit();
    portString->setValidator(new QIntValidator(1024, 65536, this));
    portString->setText(QString::number(profiler::DEFAULT_PORT));

    //layout->addWidget(hostString);
    layout->addRow(tr("&Address:"), hostString);
    layout->addRow(tr("&Port:"), portString);


    layout->addWidget(new QPushButton(tr("Connect")));
    dialog->setLayout(layout);
    dialog->exec();
    

    //qInfo() << portString->text();
    
    //TODO dialog
    
    if(m_client != nullptr)
    {
        
    }else
    {
        
        QMessageBox::warning(this,"Warning" ,"No connection with profiling app",QMessageBox::Close);
        return;
    }
    /**/

    QMessageBox::information(this,"Capturing frames..." ,"Close this window to stop capturing.",QMessageBox::Close);

    m_isConnected = (m_server->state() == QTcpSocket::ConnectedState);

    if (m_isConnected)
    {
        profiler::net::Message requestMessage(profiler::net::MESSAGE_TYPE_REQUEST_STOP_CAPTURE);
        m_client->write((const char*)&requestMessage, sizeof(requestMessage));
    }else
    {
        QMessageBox::warning(this,"Warning" ,"Application was disconnected",QMessageBox::Close);
        return;
    }
}

void EasyMainWindow::readTcpData()
{
    QTcpSocket* pClientSocket = (QTcpSocket*)sender();
    m_client =  pClientSocket;
    static qint64 necessarySize = 0;
    static qint64 loadedSize = 0;
    while(m_server->bytesAvailable())
    {
        //QByteArray data = m_server->readAll();
        //qInfo() << m_server->bytesAvailable();
        auto bytesExpected = necessarySize - loadedSize;
        QByteArray data;
        if (m_recFrames){
            data = m_server->read(qMin(bytesExpected, m_server->bytesAvailable()));
        }
        else
        {
            data = m_server->readAll();
        }


        profiler::net::Message* message = (profiler::net::Message*)data.data();
        //qInfo() << "rec size: " << data.size() << " " << QString(data);;
        if(!m_recFrames && !message->isEasyNetMessage()){
            return;
        }
        else if (m_recFrames){
            static bool first = true;
            if (first)
                m_server->waitForBytesWritten(5000);
            first = false;
            m_receivedProfileData.write(data.data(),data.size());
            loadedSize += data.size();
            if (m_receivedProfileData.str().size() == necessarySize)
            {
                m_recFrames = false;
                qInfo() << "recieve all";
                m_recFrames = false;


                qInfo() << "Write FILE";
                std::string tempfilename = "test_rec.prof";
                std::ofstream of(tempfilename, std::fstream::binary);
                of << m_receivedProfileData.str();
                of.close();

                m_receivedProfileData.str(std::string());
                m_receivedProfileData.clear();
                loadFile(QString(tempfilename.c_str()));
                m_recFrames = false;
            }
            if (m_receivedProfileData.str().size() > necessarySize)
            {
                qInfo() << "recieve more than necessary d=" << m_receivedProfileData.str().size() - necessarySize;
            }
            //qInfo() << necessarySize << " " << loadedSize << m_receivedProfileData.str().size() << m_receivedProfileData.str().length();
            qInfo() << necessarySize << " " << loadedSize;
            if (m_recFrames)
                continue;
        }

        switch (message->type) {
            case profiler::net::MESSAGE_TYPE_ACCEPTED_CONNECTION:
            {
                qInfo() << "Receive MESSAGE_TYPE_ACCEPTED_CONNECTION";
            }
                break;
            case profiler::net::MESSAGE_TYPE_REPLY_START_CAPTURING:
            {
                qInfo() << "Receive MESSAGE_TYPE_REPLY_START_CAPTURING";

                m_isClientCaptured = true;
            }
                break;
            case profiler::net::MESSAGE_TYPE_REPLY_PREPARE_BLOCKS:
            {
                qInfo() << "Receive MESSAGE_TYPE_REPLY_PREPARE_BLOCKS";
                m_isClientPreparedBlocks = true;
            }
                break;
            case profiler::net::MESSAGE_TYPE_REPLY_END_SEND_BLOCKS:
            {
                qInfo() << "Receive MESSAGE_TYPE_REPLY_END_SEND_BLOCKS";

            }
                break;
            case profiler::net::MESSAGE_TYPE_REPLY_BLOCKS:
            {
                qInfo() << "Receive MESSAGE_TYPE_REPLY_BLOCKS";
                m_recFrames = true;
                profiler::net::DataMessage* dm = (profiler::net::DataMessage*)message;
                necessarySize = dm->size;
                loadedSize = 0;
                m_receivedProfileData.write(data.data()+sizeof(profiler::net::DataMessage),data.size() - sizeof(profiler::net::DataMessage));
                loadedSize += data.size() - sizeof(profiler::net::DataMessage);
                
            }   break;

            default:
                //qInfo() << "Receive unknown " << message->type;
                break;

        }
    }


}

void EasyMainWindow::onConnected()
{
    qInfo() << "onConnected()";
    m_isConnected = true;
}
void EasyMainWindow::onErrorConnection(QAbstractSocket::SocketError socketError)
{
    qInfo() << m_server->error();
}

void EasyMainWindow::onDisconnect()
{
    qInfo() << "onDisconnect()";
    m_isConnected = false;
}

void EasyMainWindow::onNewConnection()
{
    //m_client = m_server->nextPendingConnection();

    qInfo() << "New connection!" << m_client;
    
    connect(m_client, SIGNAL(disconnected()), this, SLOT(onDisconnection())) ;
    connect(m_client, SIGNAL(readyRead()), this, SLOT(readTcpData())   );
}

void EasyMainWindow::onDisconnection()
{
    m_client = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::closeEvent(QCloseEvent* close_event)
{
    saveSettingsAndGeometry();
    Parent::closeEvent(close_event);
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::loadSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("main");

    auto last_file = settings.value("last_file");
    if (!last_file.isNull())
    {
        m_lastFile = last_file.toString();
    }


    auto val = settings.value("chrono_text_position");
    if (!val.isNull())
    {
        EASY_GLOBALS.chrono_text_position = static_cast<::profiler_gui::ChronometerTextPosition>(val.toInt());
    }


    auto flag = settings.value("draw_graphics_items_borders");
    if (!flag.isNull())
    {
        EASY_GLOBALS.draw_graphics_items_borders = flag.toBool();
    }

    flag = settings.value("collapse_items_on_tree_close");
    if (!flag.isNull())
    {
        EASY_GLOBALS.collapse_items_on_tree_close = flag.toBool();
    }

    flag = settings.value("all_items_expanded_by_default");
    if (!flag.isNull())
    {
        EASY_GLOBALS.all_items_expanded_by_default = flag.toBool();
    }

    flag = settings.value("bind_scene_and_tree_expand_status");
    if (!flag.isNull())
    {
        EASY_GLOBALS.bind_scene_and_tree_expand_status = flag.toBool();
    }

    QString encoding = settings.value("encoding", "UTF-8").toString();
    auto default_codec_mib = QTextCodec::codecForName(encoding.toStdString().c_str())->mibEnum();
    auto default_codec = QTextCodec::codecForMib(default_codec_mib);
    QTextCodec::setCodecForLocale(default_codec);

    settings.endGroup();
}

void EasyMainWindow::loadGeometry()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("main");
    auto geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    settings.endGroup();
}

void EasyMainWindow::saveSettingsAndGeometry()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("main");

    settings.setValue("geometry", this->saveGeometry());
    settings.setValue("last_file", m_lastFile);
    settings.setValue("chrono_text_position", static_cast<int>(EASY_GLOBALS.chrono_text_position));
    settings.setValue("draw_graphics_items_borders", EASY_GLOBALS.draw_graphics_items_borders);
    settings.setValue("collapse_items_on_tree_close", EASY_GLOBALS.collapse_items_on_tree_close);
    settings.setValue("all_items_expanded_by_default", EASY_GLOBALS.all_items_expanded_by_default);
    settings.setValue("bind_scene_and_tree_expand_status", EASY_GLOBALS.bind_scene_and_tree_expand_status);
    settings.setValue("encoding", QTextCodec::codecForLocale()->name());

    settings.endGroup();
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onFileReaderTimeout()
{
    if (m_reader.done())
    {
        auto nblocks = m_reader.size();
        if (nblocks != 0)
        {
            static_cast<EasyTreeWidget*>(m_treeWidget->widget())->clearSilent(true);

            ::profiler::SerializedData serialized_blocks, serialized_descriptors;
            ::profiler::descriptors_list_t descriptors;
            ::profiler::blocks_t blocks;
            ::profiler::thread_blocks_tree_t threads_map;
            QString filename;
            m_reader.get(serialized_blocks, serialized_descriptors, descriptors, blocks, threads_map, filename);

            if (threads_map.size() > 0xff)
            {
                qWarning() << "Warning: file " << filename << " contains " << threads_map.size() << " threads!";
                qWarning() << "Warning:    Currently, maximum number of displayed threads is 255! Some threads will not be displayed.";
            }

            m_lastFile = ::std::move(filename);
            m_serializedBlocks = ::std::move(serialized_blocks);
            m_serializedDescriptors = ::std::move(serialized_descriptors);
            EASY_GLOBALS.selected_thread = 0;
            ::profiler_gui::set_max(EASY_GLOBALS.selected_block);
            EASY_GLOBALS.profiler_blocks.swap(threads_map);
            EASY_GLOBALS.descriptors.swap(descriptors);

            EASY_GLOBALS.gui_blocks.clear();
            EASY_GLOBALS.gui_blocks.resize(nblocks);
            memset(EASY_GLOBALS.gui_blocks.data(), 0, sizeof(::profiler_gui::EasyBlock) * nblocks);
            for (decltype(nblocks) i = 0; i < nblocks; ++i) {
                auto& guiblock = EASY_GLOBALS.gui_blocks[i];
                guiblock.tree = ::std::move(blocks[i]);
                ::profiler_gui::set_max(guiblock.tree_item);
            }

            static_cast<EasyGraphicsViewWidget*>(m_graphicsView->widget())->view()->setTree(EASY_GLOBALS.profiler_blocks);
        }
        else
        {
            qWarning() << "Warning: Can not open file " << m_reader.filename() << " or file is corrupted";
        }

        m_reader.interrupt();

        m_readerTimer.stop();
        m_progress->setValue(100);
        //m_progress->hide();

        if (EASY_GLOBALS.all_items_expanded_by_default)
        {
            onExpandAllClicked(true);
        }
    }
    else
    {
        m_progress->setValue(m_reader.progress());
    }
}

void EasyMainWindow::onFileReaderCancel()
{
    m_readerTimer.stop();
    m_reader.interrupt();
    m_progress->setValue(100);
    //m_progress->hide();
}

//////////////////////////////////////////////////////////////////////////

EasyFileReader::EasyFileReader()
{

}

EasyFileReader::~EasyFileReader()
{
    interrupt();
}

bool EasyFileReader::done() const
{
    return m_bDone.load();
}

int EasyFileReader::progress() const
{
    return m_progress.load();
}

unsigned int EasyFileReader::size() const
{
    return m_size.load();
}

const QString& EasyFileReader::filename() const
{
    return m_filename;
}

void EasyFileReader::load(const QString& _filename)
{
    interrupt();

    m_filename = _filename;
    m_thread = ::std::move(::std::thread([this]() {
        m_size.store(fillTreesFromFile(m_progress, m_filename.toStdString().c_str(), m_serializedBlocks, m_serializedDescriptors, m_descriptors, m_blocks, m_blocksTree, true));
        m_progress.store(100);
        m_bDone.store(true);
    }));
}

void EasyFileReader::interrupt()
{
    m_progress.store(-100);
    if (m_thread.joinable())
        m_thread.join();

    m_bDone.store(false);
    m_progress.store(0);
    m_size.store(0);
    m_serializedBlocks.clear();
    m_serializedDescriptors.clear();
    m_descriptors.clear();
    m_blocks.clear();
    m_blocksTree.clear();
}

void EasyFileReader::get(::profiler::SerializedData& _serializedBlocks, ::profiler::SerializedData& _serializedDescriptors,
                         ::profiler::descriptors_list_t& _descriptors, ::profiler::blocks_t& _blocks, ::profiler::thread_blocks_tree_t& _tree,
                         QString& _filename)
{
    if (done())
    {
        m_serializedBlocks.swap(_serializedBlocks);
        m_serializedDescriptors.swap(_serializedDescriptors);
        ::profiler::descriptors_list_t(::std::move(m_descriptors)).swap(_descriptors);
        m_blocks.swap(_blocks);
        m_blocksTree.swap(_tree);
        m_filename.swap(_filename);
    }
}

//////////////////////////////////////////////////////////////////////////
