/************************************************************************
* file name         : main_window.cpp
* ----------------- :
* creation time     : 2016/06/26
* copyright         : (c) 2016 Victor Zarubkin
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
* license           : TODO: add license text
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
#include "main_window.h"
#include "blocks_tree_widget.h"
#include "blocks_graphics_view.h"
#include "globals.h"

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

    auto actionOpen = new QAction("Open", nullptr);
    connect(actionOpen, &QAction::triggered, this, &This::onOpenFileClicked);

    auto actionReload = new QAction("Reload", nullptr);
    connect(actionReload, &QAction::triggered, this, &This::onReloadFileClicked);

    auto actionExit = new QAction("Exit", nullptr);
    connect(actionExit, &QAction::triggered, this, &This::onExitClicked);

    auto menu = new QMenu("File");
    menu->addAction(actionOpen);
    menu->addAction(actionReload);
    menu->addSeparator();
    menu->addAction(actionExit);
    menuBar()->addMenu(menu);


    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("main");

    QString encoding = settings.value("encoding","UTF-8").toString();

    auto default_codec_mib = QTextCodec::codecForName(encoding.toStdString().c_str())->mibEnum() ;
    auto default_codec = QTextCodec::codecForMib(default_codec_mib);
    QTextCodec::setCodecForLocale(default_codec);
    settings.endGroup();

    menu = new QMenu("Settings");
    auto encodingMenu = menu->addMenu(tr("&Encoding"));

    QActionGroup* codecs_actions = new QActionGroup(this);
    codecs_actions->setExclusive(true);
    foreach (int mib, QTextCodec::availableMibs())
    {
        auto codec = QTextCodec::codecForMib(mib)->name();

        QAction* action = new QAction(codec,codecs_actions);

        action->setCheckable(true);
        if(mib == default_codec_mib)
        {
            action->setChecked(true);
        }
        encodingMenu->addAction(action);
        connect(action, &QAction::triggered, this, &This::onEncodingChanged);
    }

    menu->addSeparator();
    auto actionBorders = menu->addAction("Draw items' borders");
    actionBorders->setCheckable(true);
    actionBorders->setChecked(::profiler_gui::EASY_GLOBALS.draw_graphics_items_borders);
    connect(actionBorders, &QAction::triggered, this, &This::onDrawBordersChanged);

    menuBar()->addMenu(menu);

    connect(graphicsView->view(), &EasyGraphicsView::intervalChanged, treeWidget, &EasyTreeWidget::setTreeBlocks);
    connect(&m_readerTimer, &QTimer::timeout, this, &This::onFileReaderTimeout);

    loadSettings();

    m_progress = new QProgressDialog("Loading file...", "Cancel", 0, 100, this);
    m_progress->setFixedWidth(300);
    m_progress->setWindowTitle("EasyProfiler");
    m_progress->setModal(true);
    m_progress->hide();
    connect(m_progress, &QProgressDialog::canceled, this, &This::onFileReaderCancel);

    if(QCoreApplication::arguments().size() > 1)
    {
        auto opened_filename = QCoreApplication::arguments().at(1).toStdString();
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
    auto filename = QFileDialog::getOpenFileName(this, "Open profiler log", m_lastFile.c_str(), "Profiler Log File (*.prof);;All Files (*.*)");
    loadFile(filename.toStdString());
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::loadFile(const std::string& stdfilename)
{
    m_progress->setValue(0);
    m_progress->show();
    m_readerTimer.start(20);
    m_reader.load(stdfilename);

//     ::profiler::SerializedData data;
//     ::profiler::thread_blocks_tree_t prof_blocks;
//     auto nblocks = fillTreesFromFile(stdfilename.c_str(), data, prof_blocks, true);
// 
//     if (nblocks != 0)
//     {
//         static_cast<EasyTreeWidget*>(m_treeWidget->widget())->clearSilent(true);
// 
//         m_lastFile = stdfilename;
//         m_serializedData = ::std::move(data);
//         ::profiler_gui::EASY_GLOBALS.selected_thread = 0;
//         ::profiler_gui::set_max(::profiler_gui::EASY_GLOBALS.selected_block);
//         ::profiler_gui::EASY_GLOBALS.profiler_blocks.swap(prof_blocks);
//         ::profiler_gui::EASY_GLOBALS.gui_blocks.resize(nblocks);
//         memset(::profiler_gui::EASY_GLOBALS.gui_blocks.data(), 0, sizeof(::profiler_gui::EasyBlock) * nblocks);
//         for (auto& guiblock : ::profiler_gui::EASY_GLOBALS.gui_blocks) ::profiler_gui::set_max(guiblock.tree_item);
// 
//         static_cast<EasyGraphicsViewWidget*>(m_graphicsView->widget())->view()->setTree(::profiler_gui::EASY_GLOBALS.profiler_blocks);
//     }
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onReloadFileClicked(bool)
{
    if (m_lastFile.empty())
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

void EasyMainWindow::onDrawBordersChanged(bool _checked)
{
    ::profiler_gui::EASY_GLOBALS.draw_graphics_items_borders = _checked;
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::closeEvent(QCloseEvent* close_event)
{
    saveSettings();
    Parent::closeEvent(close_event);
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::loadSettings()
{
    QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
    settings.beginGroup("main");

    auto geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty())
    {
        restoreGeometry(geometry);
    }

    auto last_file = settings.value("last_file");
    if (!last_file.isNull())
    {
        m_lastFile = last_file.toString().toStdString();
    }

    settings.endGroup();
}

void EasyMainWindow::saveSettings()
{
	QSettings settings(::profiler_gui::ORGANAZATION_NAME, ::profiler_gui::APPLICATION_NAME);
	settings.beginGroup("main");

	settings.setValue("geometry", this->saveGeometry());
    settings.setValue("last_file", m_lastFile.c_str());
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

            ::profiler::SerializedData data;
            ::profiler::thread_blocks_tree_t prof_blocks;
            ::std::string stdfilename;
            m_reader.get(data, prof_blocks, stdfilename);

            m_lastFile = ::std::move(stdfilename);
            m_serializedData = ::std::move(data);
            ::profiler_gui::EASY_GLOBALS.selected_thread = 0;
            ::profiler_gui::set_max(::profiler_gui::EASY_GLOBALS.selected_block);
            ::profiler_gui::EASY_GLOBALS.profiler_blocks.swap(prof_blocks);
            ::profiler_gui::EASY_GLOBALS.gui_blocks.resize(nblocks);
            memset(::profiler_gui::EASY_GLOBALS.gui_blocks.data(), 0, sizeof(::profiler_gui::EasyBlock) * nblocks);
            for (auto& guiblock : ::profiler_gui::EASY_GLOBALS.gui_blocks) ::profiler_gui::set_max(guiblock.tree_item);

            static_cast<EasyGraphicsViewWidget*>(m_graphicsView->widget())->view()->setTree(::profiler_gui::EASY_GLOBALS.profiler_blocks);
        }

        m_reader.interrupt();

        m_readerTimer.stop();
        m_progress->setValue(100);
        m_progress->hide();
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
    m_progress->hide();
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

void EasyFileReader::load(const ::std::string& _filename)
{
    interrupt();

    m_filename = _filename;
    m_thread = ::std::move(::std::thread([](::std::atomic_bool& isDone, ::std::atomic<unsigned int>& blocks_number, ::std::atomic<int>& progress, const char* filename, ::profiler::SerializedData& serialized_blocks, ::profiler::thread_blocks_tree_t& threaded_trees) {
        blocks_number.store(fillTreesFromFile(progress, filename, serialized_blocks, threaded_trees, true));
        isDone.store(true);
    }, ::std::ref(m_bDone), ::std::ref(m_size), ::std::ref(m_progress), m_filename.c_str(), ::std::ref(m_serializedData), ::std::ref(m_blocksTree)));
}

void EasyFileReader::interrupt()
{
    m_progress.store(-100);
    if (m_thread.joinable())
        m_thread.join();

    m_bDone.store(false);
    m_progress.store(0);
    m_size.store(0);
    m_serializedData.clear();
    m_blocksTree.clear();
}

void EasyFileReader::get(::profiler::SerializedData& _data, ::profiler::thread_blocks_tree_t& _tree, ::std::string& _filename)
{
    if (done())
    {
        m_serializedData.swap(_data);
        m_blocksTree.swap(_tree);
        m_filename.swap(_filename);
    }
}

//////////////////////////////////////////////////////////////////////////
