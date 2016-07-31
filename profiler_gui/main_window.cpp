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
*                   : * 2016/06/27 Victor Zarubkin: Passing blocks number to ProfTreeWidget::setTree().
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
#include "main_window.h"
#include "blocks_tree_widget.h"
#include "blocks_graphics_view.h"

//////////////////////////////////////////////////////////////////////////

ProfMainWindow::ProfMainWindow() : QMainWindow(), m_treeWidget(nullptr), m_graphicsView(nullptr)
{
    setObjectName("ProfilerGUI_MainWindow");
    setWindowTitle("easy_profiler reader");
    setDockNestingEnabled(true);
    resize(800, 600);
    
    setStatusBar(new QStatusBar());

    auto graphicsView = new ProfGraphicsViewWidget(false);
    m_graphicsView = new QDockWidget("Blocks diagram");
    m_graphicsView->setMinimumHeight(50);
    m_graphicsView->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_graphicsView->setWidget(graphicsView);

    auto treeWidget = new ProfTreeWidget();
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

    auto actionTestView = new QAction("Test viewport", nullptr);
    connect(actionTestView, &QAction::triggered, this, &This::onTestViewportClicked);

    auto menu = new QMenu("File");
    menu->addAction(actionOpen);
    menu->addAction(actionReload);
    menu->addSeparator();
    menu->addAction(actionExit);
    menuBar()->addMenu(menu);

    menu = new QMenu("Tests");
    menu->addAction(actionTestView);
    menuBar()->addMenu(menu);


    connect(graphicsView->view(), &ProfGraphicsView::treeblocksChanged, treeWidget, &ProfTreeWidget::setTreeBlocks);


    if(QCoreApplication::arguments().size() > 1)
    {
        auto opened_filename = QCoreApplication::arguments().at(1).toStdString();
        loadFile(opened_filename);
    }
}

ProfMainWindow::~ProfMainWindow()
{
}

//////////////////////////////////////////////////////////////////////////

void ProfMainWindow::onOpenFileClicked(bool)
{
    auto filename = QFileDialog::getOpenFileName(this, "Open profiler log", m_lastFile.c_str(), "Profiler Log File (*.prof);;All Files (*.*)");
    loadFile(filename.toStdString());
}

//////////////////////////////////////////////////////////////////////////

void ProfMainWindow::loadFile(const std::string& stdfilename)
{
    thread_blocks_tree_t prof_blocks;
    auto nblocks = fillTreesFromFile(stdfilename.c_str(), prof_blocks, true);

    if (nblocks != 0)
    {
        m_lastFile = stdfilename;
        m_currentProf.swap(prof_blocks);
        static_cast<ProfGraphicsViewWidget*>(m_graphicsView->widget())->view()->setTree(m_currentProf);
    }
}

//////////////////////////////////////////////////////////////////////////

void ProfMainWindow::onReloadFileClicked(bool)
{
    if (m_lastFile.empty())
    {
        return;
    }

    thread_blocks_tree_t prof_blocks;
    auto nblocks = fillTreesFromFile(m_lastFile.c_str(), prof_blocks, true);

    if (nblocks != 0)
    {
        m_currentProf.swap(prof_blocks);
        static_cast<ProfGraphicsViewWidget*>(m_graphicsView->widget())->view()->setTree(m_currentProf);
    }
}

//////////////////////////////////////////////////////////////////////////

void ProfMainWindow::onExitClicked(bool)
{
    close();
}

//////////////////////////////////////////////////////////////////////////

void ProfMainWindow::onTestViewportClicked(bool)
{
    static_cast<ProfTreeWidget*>(m_treeWidget->widget())->clearSilent();

    auto view = static_cast<ProfGraphicsViewWidget*>(m_graphicsView->widget())->view();
    view->clearSilent();
    m_currentProf.clear();

    view->test(18000, 40000000, 5);
    //view->test(3, 300, 4);
}

//////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////
