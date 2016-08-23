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
#include <QSignalBlocker>
#include <QDebug>
#include "main_window.h"
#include "blocks_tree_widget.h"
#include "blocks_graphics_view.h"
#include "globals.h"

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
    action->setChecked(::profiler_gui::EASY_GLOBALS.draw_graphics_items_borders);
    connect(action, &QAction::triggered, this, &This::onDrawBordersChanged);

    action = menu->addAction("Collapse items on tree reset");
    action->setCheckable(true);
    action->setChecked(::profiler_gui::EASY_GLOBALS.collapse_items_on_tree_close);
    connect(action, &QAction::triggered, this, &This::onCollapseItemsAfterCloseChanged);

    action = menu->addAction("Expand all on file open");
    action->setCheckable(true);
    action->setChecked(::profiler_gui::EASY_GLOBALS.all_items_expanded_by_default);
    connect(action, &QAction::triggered, this, &This::onAllItemsExpandedByDefaultChange);

    action = menu->addAction("Bind scene and tree expand");
    action->setCheckable(true);
    action->setChecked(::profiler_gui::EASY_GLOBALS.bind_scene_and_tree_expand_status);
    connect(action, &QAction::triggered, this, &This::onBindExpandStatusChange);

    menuBar()->addMenu(menu);

    menu = new QMenu("&Settings");
    auto encodingMenu = menu->addMenu(tr("&Encoding"));

    QActionGroup* codecs_actions = new QActionGroup(this);
    codecs_actions->setExclusive(true);
    auto default_codec_mib = QTextCodec::codecForLocale()->mibEnum();
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
    //m_progress->show();
    m_readerTimer.start(LOADER_TIMER_INTERVAL);
    m_reader.load(filename);

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

void EasyMainWindow::onDrawBordersChanged(bool _checked)
{
    ::profiler_gui::EASY_GLOBALS.draw_graphics_items_borders = _checked;
}

void EasyMainWindow::onCollapseItemsAfterCloseChanged(bool _checked)
{
    ::profiler_gui::EASY_GLOBALS.collapse_items_on_tree_close = _checked;
}

void EasyMainWindow::onAllItemsExpandedByDefaultChange(bool _checked)
{
    ::profiler_gui::EASY_GLOBALS.all_items_expanded_by_default = _checked;
}

void EasyMainWindow::onBindExpandStatusChange(bool _checked)
{
    ::profiler_gui::EASY_GLOBALS.bind_scene_and_tree_expand_status = _checked;
}

//////////////////////////////////////////////////////////////////////////

void EasyMainWindow::onExpandAllClicked(bool)
{
    for (auto& block : ::profiler_gui::EASY_GLOBALS.gui_blocks)
        block.expanded = true;

    emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();

    auto tree = static_cast<EasyTreeWidget*>(m_treeWidget->widget());
    const QSignalBlocker b(tree);
    tree->expandAll();
}

void EasyMainWindow::onCollapseAllClicked(bool)
{
    for (auto& block : ::profiler_gui::EASY_GLOBALS.gui_blocks)
        block.expanded = false;

    emit ::profiler_gui::EASY_GLOBALS.events.itemsExpandStateChanged();

    auto tree = static_cast<EasyTreeWidget*>(m_treeWidget->widget());
    const QSignalBlocker b(tree);
    tree->collapseAll();
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

    auto flag = settings.value("draw_graphics_items_borders");
    if (!flag.isNull())
    {
        ::profiler_gui::EASY_GLOBALS.draw_graphics_items_borders = flag.toBool();
    }

    flag = settings.value("collapse_items_on_tree_close");
    if (!flag.isNull())
    {
        ::profiler_gui::EASY_GLOBALS.collapse_items_on_tree_close = flag.toBool();
    }

    flag = settings.value("all_items_expanded_by_default");
    if (!flag.isNull())
    {
        ::profiler_gui::EASY_GLOBALS.all_items_expanded_by_default = flag.toBool();
    }

    flag = settings.value("bind_scene_and_tree_expand_status");
    if (!flag.isNull())
    {
        ::profiler_gui::EASY_GLOBALS.bind_scene_and_tree_expand_status = flag.toBool();
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
    settings.setValue("draw_graphics_items_borders", ::profiler_gui::EASY_GLOBALS.draw_graphics_items_borders);
    settings.setValue("collapse_items_on_tree_close", ::profiler_gui::EASY_GLOBALS.collapse_items_on_tree_close);
    settings.setValue("all_items_expanded_by_default", ::profiler_gui::EASY_GLOBALS.all_items_expanded_by_default);
    settings.setValue("bind_scene_and_tree_expand_status", ::profiler_gui::EASY_GLOBALS.bind_scene_and_tree_expand_status);
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
            QString filename;
            m_reader.get(data, prof_blocks, filename);

            if (prof_blocks.size() > 0xff)
            {
                qWarning() << "Warning: file " << filename << " contains " << prof_blocks.size() << " threads!";
                qWarning() << "Warning:    Currently, maximum number of displayed threads is 255! Some threads will not be displayed.";
            }

            m_lastFile = ::std::move(filename);
            m_serializedData = ::std::move(data);
            ::profiler_gui::EASY_GLOBALS.selected_thread = 0;
            ::profiler_gui::set_max(::profiler_gui::EASY_GLOBALS.selected_block);
            ::profiler_gui::EASY_GLOBALS.profiler_blocks.swap(prof_blocks);
            ::profiler_gui::EASY_GLOBALS.gui_blocks.resize(nblocks);
            memset(::profiler_gui::EASY_GLOBALS.gui_blocks.data(), 0, sizeof(::profiler_gui::EasyBlock) * nblocks);
            for (auto& guiblock : ::profiler_gui::EASY_GLOBALS.gui_blocks) ::profiler_gui::set_max(guiblock.tree_item);

            static_cast<EasyGraphicsViewWidget*>(m_graphicsView->widget())->view()->setTree(::profiler_gui::EASY_GLOBALS.profiler_blocks);
        }
        else
        {
            qWarning() << "Warning: Can not open file " << m_reader.filename();
        }

        m_reader.interrupt();

        m_readerTimer.stop();
        m_progress->setValue(100);
        //m_progress->hide();

        if (::profiler_gui::EASY_GLOBALS.all_items_expanded_by_default)
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
    m_thread = ::std::move(::std::thread([](::std::atomic_bool& isDone, ::std::atomic<unsigned int>& blocks_number, ::std::atomic<int>& progress, const QString& filename, ::profiler::SerializedData& serialized_blocks, ::profiler::thread_blocks_tree_t& threaded_trees) {
        blocks_number.store(fillTreesFromFile(progress, filename.toStdString().c_str(), serialized_blocks, threaded_trees, true));
        isDone.store(true);
    }, ::std::ref(m_bDone), ::std::ref(m_size), ::std::ref(m_progress), ::std::ref(m_filename), ::std::ref(m_serializedData), ::std::ref(m_blocksTree)));
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

void EasyFileReader::get(::profiler::SerializedData& _data, ::profiler::thread_blocks_tree_t& _tree, QString& _filename)
{
    if (done())
    {
        m_serializedData.swap(_data);
        m_blocksTree.swap(_tree);
        m_filename.swap(_filename);
    }
}

//////////////////////////////////////////////////////////////////////////
