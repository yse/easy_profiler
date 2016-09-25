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

#include <chrono>
#include <fstream>

#include <QApplication>
#include <QCoreApplication>
#include <QStatusBar>
#include <QDockWidget>
#include <QFileDialog>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QCloseEvent>
#include <QSettings>
#include <QTextCodec>
#include <QFont>
#include <QProgressDialog>
#include <QSignalBlocker>
#include <QDebug>
#include <QToolBar>
#include <QMessageBox>
#include <QLineEdit>
#include <QLabel>
#include <QDialog>
#include <QVBoxLayout>

#include "main_window.h"
#include "blocks_tree_widget.h"
#include "blocks_graphics_view.h"
#include "descriptors_tree_widget.h"
#include "globals.h"
#include "profiler/easy_net.h"

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

//////////////////////////////////////////////////////////////////////////

const int LOADER_TIMER_INTERVAL = 40;

//////////////////////////////////////////////////////////////////////////

EasyMainWindow::EasyMainWindow() : Parent()
{
    { QIcon icon(":/logo"); if (!icon.isNull()) QApplication::setWindowIcon(icon); }

    setObjectName("ProfilerGUI_MainWindow");
    setWindowTitle("EasyProfiler Reader beta");
    setDockNestingEnabled(true);
    resize(800, 600);
    
    setStatusBar(new QStatusBar());

    auto graphicsView = new EasyGraphicsViewWidget();
    m_graphicsView = new QDockWidget("Diagram");
    m_graphicsView->setObjectName("ProfilerGUI_Diagram");
    m_graphicsView->setMinimumHeight(50);
    m_graphicsView->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_graphicsView->setWidget(graphicsView);

    auto treeWidget = new EasyTreeWidget();
    m_treeWidget = new QDockWidget("Hierarchy");
    m_treeWidget->setObjectName("ProfilerGUI_Hierarchy");
    m_treeWidget->setMinimumHeight(50);
    m_treeWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_treeWidget->setWidget(treeWidget);

    addDockWidget(Qt::TopDockWidgetArea, m_graphicsView);
    addDockWidget(Qt::BottomDockWidgetArea, m_treeWidget);

#if EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW != 0
    auto descTree = new EasyDescWidget();
    m_descTreeWidget = new QDockWidget("Blocks");
    m_descTreeWidget->setObjectName("ProfilerGUI_Blocks");
    m_descTreeWidget->setMinimumHeight(50);
    m_descTreeWidget->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_descTreeWidget->setWidget(descTree);
    addDockWidget(Qt::BottomDockWidgetArea, m_descTreeWidget);
#endif

    auto toolbar = addToolBar("MainToolBar");
    toolbar->setObjectName("ProfilerGUI_MainToolBar");
    toolbar->addAction(QIcon(":/Delete"), tr("Clear all"), this, SLOT(onDeleteClicked(bool)));

    toolbar->addAction(QIcon(":/List"), tr("Blocks"), this, SLOT(onEditBlocksClicked(bool)));
    m_captureAction = toolbar->addAction(QIcon(":/Start"), tr("Capture"), this, SLOT(onCaptureClicked(bool)));
    m_captureAction->setEnabled(false);

    toolbar->addSeparator();
    m_connectAction = toolbar->addAction(QIcon(":/Connection"), tr("Connect"), this, SLOT(onConnectClicked(bool)));

    toolbar->addWidget(new QLabel(" IP:"));
    m_ipEdit = new QLineEdit();
    m_ipEdit->setMaximumWidth(100);
    QRegExp rx("[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}");
    m_ipEdit->setInputMask("000.000.000.000;");
    m_ipEdit->setValidator(new QRegExpValidator(rx, m_ipEdit));
    m_ipEdit->setText("127.0.0.1");
    toolbar->addWidget(m_ipEdit);

    toolbar->addWidget(new QLabel(" Port:"));
    m_portEdit = new QLineEdit();
    m_portEdit->setMaximumWidth(80);
    m_portEdit->setValidator(new QIntValidator(1024, 65536, m_portEdit));
    m_portEdit->setText(QString::number(::profiler::DEFAULT_PORT));
    toolbar->addWidget(m_portEdit);

    

    /*m_server = new QTcpSocket( );
    m_server->setSocketOption(QAbstractSocket::LowDelayOption, 0);
    m_server->setReadBufferSize(16 * 1024);
    
    //connect(m_server, SIGNAL(readyRead()), SLOT(readTcpData()));
    connect(m_server, SIGNAL(connected()), SLOT(onConnected()));
    connect(m_server, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onErrorConnection(QAbstractSocket::SocketError)), Qt::UniqueConnection);
    connect(m_server, SIGNAL(disconnected()), SLOT(onDisconnect()), Qt::UniqueConnection);
    
    m_receiver = new TcpReceiverThread(this, this);
   
    connect(m_receiver, &TcpReceiverThread::resultReady, this, &This::handleResults);
    connect(m_receiver, &TcpReceiverThread::finished, m_receiver, &QObject::deleteLater);

    m_receiver->start();
    */

    //m_thread = std::thread(&This::listen, this);

    //connect(m_server, &QAbstractSocket::readyRead, m_receiver, &TcpReceiverThread::readTcpData);

    //m_receiver->run();

    //m_server->connectToHost(m_ipEdit->text(), m_portEdit->text().toUShort());
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



    auto menu = menuBar()->addMenu("&File");
    menu->addAction(QIcon(":/Open"), "&Open", this, SLOT(onOpenFileClicked(bool)));
    menu->addAction(QIcon(":/Reload"), "&Reload", this, SLOT(onReloadFileClicked(bool)));
    menu->addSeparator();
    menu->addAction(QIcon(":/Exit"), "&Exit", this, SLOT(onExitClicked(bool)));



    menu = menuBar()->addMenu("&View");

    menu->addAction(QIcon(":/Expand"), "Expand all", this, SLOT(onExpandAllClicked(bool)));
    menu->addAction(QIcon(":/Collapse"), "Collapse all", this,SLOT(onCollapseAllClicked(bool)));

    menu->addSeparator();

    auto action = menu->addAction("Draw items' borders");
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

    action = menu->addAction("Draw event indicators");
    action->setCheckable(true);
    action->setChecked(EASY_GLOBALS.enable_event_indicators);
    connect(action, &QAction::triggered, this, &This::onEventIndicatorsChange);

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



    menu = menuBar()->addMenu("&Settings");
    action = menu->addAction("Statistics enabled");
    action->setCheckable(true);
    action->setChecked(EASY_GLOBALS.enable_statistics);
    connect(action, &QAction::triggered, this, &This::onEnableDisableStatistics);
    if (EASY_GLOBALS.enable_statistics)
    {
        auto f = action->font();
        f.setBold(true);
        action->setFont(f);
        SET_ICON(action, ":/Stats");
    }
    else
    {
        action->setText("Statistics disabled");
        SET_ICON(action, ":/Stats-off");
    }

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



    connect(graphicsView->view(), &EasyGraphicsView::intervalChanged, treeWidget, &EasyTreeWidget::setTreeBlocks);
    connect(&m_readerTimer, &QTimer::timeout, this, &This::onFileReaderTimeout);
    connect(&m_listenerTimer, &QTimer::timeout, this, &This::onListenerTimerTimeout);
    

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

    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::blockStatusChanged, this, &This::onBlockStatusChange);
    connect(&EASY_GLOBALS.events, &::profiler_gui::EasyGlobalSignals::blocksRefreshRequired, this, &This::onGetBlockDescriptionsClicked);
}

EasyMainWindow::~EasyMainWindow()
{
    delete m_progress;
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onOpenFileClicked(bool)
{
    auto filename = QFileDialog::getOpenFileName(this, "Open profiler log", m_lastFile, "Profiler Log File (*.prof);;All Files (*.*)");
    if (!filename.isEmpty())
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

void EasyMainWindow::readStream(::std::stringstream& data)
{
    m_progress->setLabelText(tr("Reading from stream..."));

    m_progress->setValue(0);
    m_progress->show();
    m_readerTimer.start(LOADER_TIMER_INTERVAL);
    m_reader.load(data);
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onReloadFileClicked(bool)
{
    if (m_lastFile.isEmpty())
        return;
    loadFile(m_lastFile);
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onDeleteClicked(bool)
{
    static_cast<EasyTreeWidget*>(m_treeWidget->widget())->clearSilent(true);
    static_cast<EasyGraphicsViewWidget*>(m_graphicsView->widget())->clear();

#if EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW != 0
    static_cast<EasyDescWidget*>(m_descTreeWidget->widget())->clear();
#endif
    if (m_dialogDescTree != nullptr)
        m_dialogDescTree->clear();

    EASY_GLOBALS.selected_thread = 0;
    ::profiler_gui::set_max(EASY_GLOBALS.selected_block);
    EASY_GLOBALS.profiler_blocks.clear();
    EASY_GLOBALS.descriptors.clear();
    EASY_GLOBALS.gui_blocks.clear();

    m_serializedBlocks.clear();
    m_serializedDescriptors.clear();
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
    static_cast<EasyGraphicsViewWidget*>(m_graphicsView->widget())->view()->scene()->update();
}

void EasyMainWindow::onEventIndicatorsChange(bool _checked)
{
    EASY_GLOBALS.enable_event_indicators = _checked;
    static_cast<EasyGraphicsViewWidget*>(m_graphicsView->widget())->view()->scene()->update();
}

void EasyMainWindow::onEnableDisableStatistics(bool _checked)
{
    EASY_GLOBALS.enable_statistics = _checked;

    auto action = qobject_cast<QAction*>(sender());
    if (action != nullptr)
    {
        auto f = action->font();
        f.setBold(_checked);
        action->setFont(f);

        if (_checked)
        {
            action->setText("Statistics enabled");
            SET_ICON(action, ":/Stats");
        }
        else
        {
            action->setText("Statistics disabled");
            SET_ICON(action, ":/Stats-off");
        }
    }
}

void EasyMainWindow::onDrawBordersChanged(bool _checked)
{
    EASY_GLOBALS.draw_graphics_items_borders = _checked;
    static_cast<EasyGraphicsViewWidget*>(m_graphicsView->widget())->view()->scene()->update();
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

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onEditBlocksClicked(bool)
{
    if (m_descTreeDialog != nullptr)
    {
        m_descTreeDialog->raise();
        return;
    }

    m_descTreeDialog = new QDialog();
    m_descTreeDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    m_descTreeDialog->setWindowTitle("EasyProfiler");
    m_descTreeDialog->resize(800, 600);
    connect(m_descTreeDialog, &QDialog::finished, this, &This::onDescTreeDialogClose);

    auto l = new QVBoxLayout(m_descTreeDialog);
    m_dialogDescTree = new EasyDescWidget(m_descTreeDialog);
    l->addWidget(m_dialogDescTree);
    m_descTreeDialog->setLayout(l);

    m_dialogDescTree->build();
    m_descTreeDialog->show();
}

void EasyMainWindow::onDescTreeDialogClose(int)
{
    disconnect(m_descTreeDialog, &QDialog::finished, this, &This::onDescTreeDialogClose);
    m_dialogDescTree = nullptr;
    m_descTreeDialog = nullptr;
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::closeEvent(QCloseEvent* close_event)
{
    saveSettingsAndGeometry();

    if (m_descTreeDialog != nullptr)
    {
        m_descTreeDialog->reject();
        m_descTreeDialog = nullptr;
        m_dialogDescTree = nullptr;
    }

    Parent::closeEvent(close_event);
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::loadSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("main");

    auto last_file = settings.value("last_file");
    if (!last_file.isNull())
        m_lastFile = last_file.toString();


    auto val = settings.value("chrono_text_position");
    if (!val.isNull())
        EASY_GLOBALS.chrono_text_position = static_cast<::profiler_gui::ChronometerTextPosition>(val.toInt());


    auto flag = settings.value("draw_graphics_items_borders");
    if (!flag.isNull())
        EASY_GLOBALS.draw_graphics_items_borders = flag.toBool();

    flag = settings.value("collapse_items_on_tree_close");
    if (!flag.isNull())
        EASY_GLOBALS.collapse_items_on_tree_close = flag.toBool();

    flag = settings.value("all_items_expanded_by_default");
    if (!flag.isNull())
        EASY_GLOBALS.all_items_expanded_by_default = flag.toBool();

    flag = settings.value("bind_scene_and_tree_expand_status");
    if (!flag.isNull())
        EASY_GLOBALS.bind_scene_and_tree_expand_status = flag.toBool();

    flag = settings.value("enable_event_indicators");
    if (!flag.isNull())
        EASY_GLOBALS.enable_event_indicators = flag.toBool();

    flag = settings.value("enable_statistics");
    if (!flag.isNull())
        EASY_GLOBALS.enable_statistics = flag.toBool();

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

    auto state = settings.value("windowState").toByteArray();
    if (!state.isEmpty())
        restoreState(state);

    settings.endGroup();
}

void EasyMainWindow::saveSettingsAndGeometry()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("main");

    settings.setValue("geometry", this->saveGeometry());
    settings.setValue("windowState", this->saveState());
    settings.setValue("last_file", m_lastFile);
    settings.setValue("chrono_text_position", static_cast<int>(EASY_GLOBALS.chrono_text_position));
    settings.setValue("draw_graphics_items_borders", EASY_GLOBALS.draw_graphics_items_borders);
    settings.setValue("collapse_items_on_tree_close", EASY_GLOBALS.collapse_items_on_tree_close);
    settings.setValue("all_items_expanded_by_default", EASY_GLOBALS.all_items_expanded_by_default);
    settings.setValue("bind_scene_and_tree_expand_status", EASY_GLOBALS.bind_scene_and_tree_expand_status);
    settings.setValue("enable_event_indicators", EASY_GLOBALS.enable_event_indicators);
    settings.setValue("enable_statistics", EASY_GLOBALS.enable_statistics);
    settings.setValue("encoding", QTextCodec::codecForLocale()->name());

    settings.endGroup();
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onListenerTimerTimeout()
{
    if (!m_listener.connected())
        m_listenerDialog->reject();
}

void EasyMainWindow::onListenerDialogClose(int)
{
    m_listenerTimer.stop();
    disconnect(m_listenerDialog, &QDialog::finished, this, &This::onListenerDialogClose);
    m_listenerDialog = nullptr;

    switch (m_listener.regime())
    {
        case LISTENER_CAPTURE:
        {
            m_listenerDialog = new QMessageBox(QMessageBox::Information, "Receiving data...", "This process may take some time.", QMessageBox::NoButton, this);
            m_listenerDialog->setAttribute(Qt::WA_DeleteOnClose, true);
            m_listenerDialog->show();

            m_listener.stopCapture();

            m_listenerDialog->reject();
            m_listenerDialog = nullptr;

            if (m_listener.size() != 0)
            {
                readStream(m_listener.data());
                m_listener.clearData();
            }

            break;
        }

        case LISTENER_DESCRIBE:
        {
            break;
        }

        default:
            return;
    }

    if (!m_listener.connected())
    {
        QMessageBox::warning(this, "Warning", "Application was disconnected", QMessageBox::Close);
        EASY_GLOBALS.connected = false;
        m_captureAction->setEnabled(false);
        SET_ICON(m_connectAction, ":/Connection");

        emit EASY_GLOBALS.events.connectionChanged(false);
    }
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
                if (m_reader.isFile())
                    qWarning() << "Warning: file " << filename << " contains " << threads_map.size() << " threads!";
                else
                    qWarning() << "Warning: input stream contains " << threads_map.size() << " threads!";
                qWarning() << "Warning:    Currently, maximum number of displayed threads is 255! Some threads will not be displayed.";
            }

            if (m_reader.isFile())
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

#if EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW != 0
            static_cast<EasyDescWidget*>(m_descTreeWidget->widget())->build();
#endif
            if (m_dialogDescTree != nullptr)
                m_dialogDescTree->build();
        }
        else if (m_reader.isFile())
        {
            qWarning() << "Warning: Can not open file " << m_reader.filename() << " or file is corrupted";
        }
        else
        {
            qWarning() << "Warning: Can not read from stream: bad data";
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

const bool EasyFileReader::isFile() const
{
    return m_isFile;
}

bool EasyFileReader::done() const
{
    return m_bDone.load(::std::memory_order_acquire);
}

int EasyFileReader::progress() const
{
    return m_progress.load(::std::memory_order_acquire);
}

unsigned int EasyFileReader::size() const
{
    return m_size.load(::std::memory_order_acquire);
}

const QString& EasyFileReader::filename() const
{
    return m_filename;
}

void EasyFileReader::load(const QString& _filename)
{
    interrupt();

    m_isFile = true;
    m_filename = _filename;
    m_thread = ::std::move(::std::thread([this](bool _enableStatistics) {
        m_size.store(fillTreesFromFile(m_progress, m_filename.toStdString().c_str(), m_serializedBlocks, m_serializedDescriptors, m_descriptors, m_blocks, m_blocksTree, _enableStatistics), ::std::memory_order_release);
        m_progress.store(100, ::std::memory_order_release);
        m_bDone.store(true, ::std::memory_order_release);
    }, EASY_GLOBALS.enable_statistics));
}

void EasyFileReader::load(::std::stringstream& _stream)
{
    interrupt();

    m_isFile = false;
    m_filename.clear();
    m_stream.swap(_stream);
    m_thread = ::std::move(::std::thread([this](bool _enableStatistics) {
        m_size.store(fillTreesFromStream(m_progress, m_stream, m_serializedBlocks, m_serializedDescriptors, m_descriptors, m_blocks, m_blocksTree, _enableStatistics), ::std::memory_order_release);
        m_progress.store(100, ::std::memory_order_release);
        m_bDone.store(true, ::std::memory_order_release);
    }, EASY_GLOBALS.enable_statistics));
}

void EasyFileReader::interrupt()
{
    m_progress.store(-100, ::std::memory_order_release);
    if (m_thread.joinable())
        m_thread.join();

    m_bDone.store(false, ::std::memory_order_release);
    m_progress.store(0, ::std::memory_order_release);
    m_size.store(0, ::std::memory_order_release);
    m_serializedBlocks.clear();
    m_serializedDescriptors.clear();
    m_descriptors.clear();
    m_blocks.clear();
    m_blocksTree.clear();

    decltype(m_stream) dummy;
    dummy.swap(m_stream);
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

void EasyMainWindow::onConnectClicked(bool)
{
    if(EASY_GLOBALS.connected)
        return;

    if (!m_listener.connect(m_ipEdit->text().toStdString().c_str(), m_portEdit->text().toUShort()))
    {
        QMessageBox::warning(this, "Warning", "Cannot connect with application", QMessageBox::Close);
        return;
    }

    qInfo() << "Connected successfully";
    EASY_GLOBALS.connected = true;
    m_captureAction->setEnabled(true);
    SET_ICON(m_connectAction, ":/Connection-on");

    emit EASY_GLOBALS.events.connectionChanged(true);
}

void EasyMainWindow::onCaptureClicked(bool)
{
    if (!EASY_GLOBALS.connected)
    {
        QMessageBox::warning(this, "Warning", "No connection with profiling app", QMessageBox::Close);
        return;
    }

    if (m_listener.regime() != LISTENER_IDLE)
    {
        if (m_listener.regime() == LISTENER_CAPTURE)
            QMessageBox::warning(this, "Warning", "Already capturing frames.\nFinish old capturing session first.", QMessageBox::Close);
        else
            QMessageBox::warning(this, "Warning", "Capturing blocks description.\nFinish old capturing session first.", QMessageBox::Close);
        return;
    }

    m_listener.startCapture();
    m_listenerTimer.start(250);

    m_listenerDialog = new QMessageBox(QMessageBox::Information, "Capturing frames...", "Close this dialog to stop capturing.", QMessageBox::Close, this);
    m_listenerDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    connect(m_listenerDialog, &QDialog::finished, this, &This::onListenerDialogClose);
    m_listenerDialog->show();
}

void EasyMainWindow::onGetBlockDescriptionsClicked(bool)
{
    if (!EASY_GLOBALS.connected)
    {
        QMessageBox::warning(this, "Warning", "No connection with profiling app", QMessageBox::Close);
        return;
    }

    if (m_listener.regime() != LISTENER_IDLE)
    {
        if (m_listener.regime() == LISTENER_DESCRIBE)
            QMessageBox::warning(this, "Warning", "Already capturing blocks description.\nFinish old capturing session first.", QMessageBox::Close);
        else
            QMessageBox::warning(this, "Warning", "Capturing capturing frames.\nFinish old capturing session first.", QMessageBox::Close);
        return;
    }

    m_listenerDialog = new QMessageBox(QMessageBox::Information, "Waiting for blocks...", "This may take some time.", QMessageBox::NoButton, this);
    m_listenerDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    m_listenerDialog->show();

    m_listener.requestBlocksDescription();

    m_listenerDialog->reject();
    m_listenerDialog = nullptr;

    if (m_listener.size() != 0)
    {
        // Read descriptions from stream
        decltype(EASY_GLOBALS.descriptors) descriptors;
        decltype(m_serializedDescriptors) serializedDescriptors;
        if (readDescriptionsFromStream(m_listener.data(), serializedDescriptors, descriptors))
        {
            if (EASY_GLOBALS.descriptors.size() > descriptors.size())
                onDeleteClicked(true); // Clear all contents because new descriptors list conflicts with old one

            EASY_GLOBALS.descriptors.swap(descriptors);
            m_serializedDescriptors.swap(serializedDescriptors);

            if (m_descTreeDialog != nullptr)
            {
#if EASY_GUI_USE_DESCRIPTORS_DOCK_WINDOW != 0
                static_cast<EasyDescWidget*>(m_descTreeWidget->widget())->build();
#endif
                m_dialogDescTree->build();
                m_descTreeDialog->raise();
            }
            else
            {
                onEditBlocksClicked(true);
            }
        }

        m_listener.clearData();
    }

    if (!m_listener.connected())
    {
        QMessageBox::warning(this, "Warning", "Application was disconnected", QMessageBox::Close);
        EASY_GLOBALS.connected = false;
        m_captureAction->setEnabled(false);
        SET_ICON(m_connectAction, ":/Connection");

        emit EASY_GLOBALS.events.connectionChanged(false);
    }
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onBlockStatusChange(::profiler::block_id_t _id, ::profiler::EasyBlockStatus _status)
{
    if (EASY_GLOBALS.connected)
        m_listener.sendBlockStatus(_id, _status);
}

//////////////////////////////////////////////////////////////////////////

EasySocketListener::EasySocketListener() : m_receivedSize(0), m_regime(LISTENER_IDLE)
{
    m_bInterrupt = ATOMIC_VAR_INIT(false);
    m_bConnected = ATOMIC_VAR_INIT(false);
}

EasySocketListener::~EasySocketListener()
{
    m_bInterrupt.store(true, ::std::memory_order_release);
    if (m_thread.joinable())
        m_thread.join();
}

bool EasySocketListener::connected() const
{
    return m_bConnected.load(::std::memory_order_acquire);
}

EasyListenerRegime EasySocketListener::regime() const
{
    return m_regime;
}

uint64_t EasySocketListener::size() const
{
    return m_receivedSize;
}

::std::stringstream& EasySocketListener::data()
{
    return m_receivedData;
}

void EasySocketListener::clearData()
{
    decltype(m_receivedData) dummy;
    dummy.swap(m_receivedData);
    m_receivedSize = 0;
}

bool EasySocketListener::connect(const char* _ipaddress, uint16_t _port)
{
    if (connected())
        return true;

    m_easySocket.flush();
    m_easySocket.init();
    int res = m_easySocket.setAddress(_ipaddress, _port);
    res = m_easySocket.connect();

    bool isConnected = res == 0;
    m_bConnected.store(isConnected, ::std::memory_order_release);

    return isConnected;
}

void EasySocketListener::startCapture()
{
    clearData();

    profiler::net::Message request(profiler::net::MESSAGE_TYPE_REQUEST_START_CAPTURE);
    m_easySocket.send(&request, sizeof(request));

    m_regime = LISTENER_CAPTURE;
    m_thread = ::std::move(::std::thread(&EasySocketListener::listenCapture, this));
}

void EasySocketListener::stopCapture()
{
    if (!m_thread.joinable() || m_regime != LISTENER_CAPTURE)
        return;

    profiler::net::Message request(profiler::net::MESSAGE_TYPE_REQUEST_STOP_CAPTURE);
    m_easySocket.send(&request, sizeof(request));

    m_thread.join();

    m_regime = LISTENER_IDLE;
}

void EasySocketListener::requestBlocksDescription()
{
    clearData();

    profiler::net::Message request(profiler::net::MESSAGE_TYPE_REQUEST_BLOCKS_DESCRIPTION);
    m_easySocket.send(&request, sizeof(request));

    m_regime = LISTENER_DESCRIBE;
    listenDescription();
    m_regime = LISTENER_IDLE;
}

void EasySocketListener::sendBlockStatus(::profiler::block_id_t _id, ::profiler::EasyBlockStatus _status)
{
    profiler::net::BlockStatusMessage message(_id, static_cast<uint8_t>(_status));
    m_easySocket.send(&message, sizeof(message));
}

//////////////////////////////////////////////////////////////////////////

void EasySocketListener::listenCapture()
{
    // TODO: Merge functions listenCapture() and listenDescription()

    static const int buffer_size = 8 * 1024 * 1024;
    char* buffer = new char[buffer_size];
    int seek = 0, bytes = 0;
    auto timeBegin = ::std::chrono::system_clock::now();

    bool isListen = true, disconnected = false;
    while (isListen && !m_bInterrupt.load(::std::memory_order_acquire))
    {
        if ((bytes - seek) == 0)
        {
            bytes = m_easySocket.receive(buffer, buffer_size);

            if (bytes == -1)
            {
                if (m_easySocket.state() == EasySocket::CONNECTION_STATE_DISCONNECTED)
                {
                    m_bConnected.store(false, ::std::memory_order_release);
                    isListen = false;
                    disconnected = true;
                }

                seek = 0;
                bytes = 0;

                continue;
            }

            seek = 0;
        }

        if (bytes == 0)
        {
            isListen = false;
            break;
        }

        char* buf = buffer + seek;

        if (bytes > 0)
        {
            auto message = reinterpret_cast<const ::profiler::net::Message*>(buf);
            if (!message->isEasyNetMessage())
                continue;

            switch (message->type)
            {
                case profiler::net::MESSAGE_TYPE_ACCEPTED_CONNECTION:
                {
                    qInfo() << "Receive MESSAGE_TYPE_ACCEPTED_CONNECTION";
                    //m_easySocket.send(&request, sizeof(request));
                    seek += sizeof(profiler::net::Message);
                    break;
                }

                case profiler::net::MESSAGE_TYPE_REPLY_START_CAPTURING:
                {
                    qInfo() << "Receive MESSAGE_TYPE_REPLY_START_CAPTURING";
                    seek += sizeof(profiler::net::Message);
                    break;
                }

                case profiler::net::MESSAGE_TYPE_REPLY_BLOCKS_END:
                {
                    qInfo() << "Receive MESSAGE_TYPE_REPLY_BLOCKS_END";
                    seek += sizeof(profiler::net::Message);

                    const auto dt = ::std::chrono::duration_cast<std::chrono::milliseconds>(::std::chrono::system_clock::now() - timeBegin);
                    const auto bytesNumber = m_receivedData.str().size();
                    qInfo() << "recieved " << bytesNumber << " bytes, " << dt.count() << " ms, average speed = " << double(bytesNumber) * 1e3 / double(dt.count()) / 1024. << " kBytes/sec";

                    seek = 0;
                    bytes = 0;

                    isListen = false;

                    break;
                }

                case profiler::net::MESSAGE_TYPE_REPLY_BLOCKS:
                {
                    qInfo() << "Receive MESSAGE_TYPE_REPLY_BLOCKS";

                    seek += sizeof(profiler::net::DataMessage);
                    profiler::net::DataMessage* dm = (profiler::net::DataMessage*)message;
                    timeBegin = std::chrono::system_clock::now();

                    int neededSize = dm->size;


                    buf = buffer + seek;
                    auto bytesNumber = ::std::min((int)dm->size, bytes - seek);
                    m_receivedSize += bytesNumber;
                    m_receivedData.write(buf, bytesNumber);
                    neededSize -= bytesNumber;

                    if (neededSize == 0)
                        seek += bytesNumber;
                    else
                    {
                        seek = 0;
                        bytes = 0;
                    }


                    int loaded = 0;
                    while (neededSize > 0)
                    {
                        bytes = m_easySocket.receive(buffer, buffer_size);

                        if (bytes == -1)
                        {
                            if (m_easySocket.state() == EasySocket::CONNECTION_STATE_DISCONNECTED)
                            {
                                m_bConnected.store(false, ::std::memory_order_release);
                                isListen = false;
                                disconnected = true;
                                neededSize = 0;
                            }

                            break;
                        }

                        buf = buffer;
                        int toWrite = ::std::min(bytes, neededSize);
                        m_receivedSize += toWrite;
                        m_receivedData.write(buf, toWrite);
                        neededSize -= toWrite;
                        loaded += toWrite;
                        seek = toWrite;
                    }

                    break;
                }

                default:
                    //qInfo() << "Receive unknown " << message->type;
                    break;
            }
        }
    }

    if (disconnected)
        clearData();

    delete [] buffer;
}

void EasySocketListener::listenDescription()
{
    // TODO: Merge functions listenDescription() and listenCapture()

    static const int buffer_size = 8 * 1024 * 1024;
    char* buffer = new char[buffer_size];
    int seek = 0, bytes = 0;

    bool isListen = true, disconnected = false;
    while (isListen && !m_bInterrupt.load(::std::memory_order_acquire))
    {
        if ((bytes - seek) == 0)
        {
            bytes = m_easySocket.receive(buffer, buffer_size);

            if (bytes == -1)
            {
                if (m_easySocket.state() == EasySocket::CONNECTION_STATE_DISCONNECTED)
                {
                    m_bConnected.store(false, ::std::memory_order_release);
                    isListen = false;
                    disconnected = true;
                }

                seek = 0;
                bytes = 0;

                continue;
            }

            seek = 0;
        }

        if (bytes == 0)
        {
            isListen = false;
            break;
        }

        char* buf = buffer + seek;

        if (bytes > 0)
        {
            auto message = reinterpret_cast<const ::profiler::net::Message*>(buf);
            if (!message->isEasyNetMessage())
                continue;

            switch (message->type)
            {
                case profiler::net::MESSAGE_TYPE_ACCEPTED_CONNECTION:
                {
                    qInfo() << "Receive MESSAGE_TYPE_ACCEPTED_CONNECTION";
                    seek += sizeof(profiler::net::Message);
                    break;
                }

                case profiler::net::MESSAGE_TYPE_REPLY_BLOCKS_DESCRIPTION_END:
                {
                    qInfo() << "Receive MESSAGE_TYPE_REPLY_BLOCKS_DESCRIPTION_END";
                    seek += sizeof(profiler::net::Message);

                    seek = 0;
                    bytes = 0;

                    isListen = false;

                    break;
                }

                case profiler::net::MESSAGE_TYPE_REPLY_BLOCKS_DESCRIPTION:
                {
                    qInfo() << "Receive MESSAGE_TYPE_REPLY_BLOCKS";

                    seek += sizeof(profiler::net::DataMessage);
                    profiler::net::DataMessage* dm = (profiler::net::DataMessage*)message;
                    int neededSize = dm->size;

                    buf = buffer + seek;
                    auto bytesNumber = ::std::min((int)dm->size, bytes - seek);
                    m_receivedSize += bytesNumber;
                    m_receivedData.write(buf, bytesNumber);
                    neededSize -= bytesNumber;
                    
                    if (neededSize == 0)
                        seek += bytesNumber;
                    else{
                        seek = 0;
                        bytes = 0;
                    }

                    int loaded = 0;
                    while (neededSize > 0)
                    {
                        bytes = m_easySocket.receive(buffer, buffer_size);

                        if (bytes == -1)
                        {
                            if (m_easySocket.state() == EasySocket::CONNECTION_STATE_DISCONNECTED)
                            {
                                m_bConnected.store(false, ::std::memory_order_release);
                                isListen = false;
                                disconnected = true;
                                neededSize = 0;
                            }

                            break;
                        }

                        buf = buffer;
                        int toWrite = ::std::min(bytes, neededSize);
                        m_receivedSize += toWrite;
                        m_receivedData.write(buf, toWrite);
                        neededSize -= toWrite;
                        loaded += toWrite;
                        seek = toWrite;
                    }

                    break;
                }

                default:
                    break;
            }
        }
    }

    if (disconnected)
        clearData();

    delete[] buffer;
}

//////////////////////////////////////////////////////////////////////////

