/************************************************************************
* file name         : main_window.h
* ----------------- :
* creation time     : 2016/06/26
* copyright         : (c) 2016 Victor Zarubkin
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of MainWindow for easy_profiler GUI.
* ----------------- :
* change log        : * 2016/06/26 Victor Zarubkin: initial commit.
*                   : *
* ----------------- :
* license           : TODO: add license text
************************************************************************/

#ifndef EASY_PROFILER_GUI__MAIN_WINDOW__H
#define EASY_PROFILER_GUI__MAIN_WINDOW__H

#include <QMainWindow>
#include <string>
#include "profiler/reader.h"

//////////////////////////////////////////////////////////////////////////

class ProfTreeWidget;
class ProfGraphicsView;
class QDockWidget;

class ProfMainWindow : public QMainWindow
{
    Q_OBJECT

protected:

    typedef ProfMainWindow This;

    ::std::string     m_lastFile;
    QDockWidget*    m_treeWidget;
    QDockWidget*  m_graphicsView;

public:

    ProfMainWindow();
    virtual ~ProfMainWindow();

    // Public virtual methods

    void closeEvent(QCloseEvent* close_event) override;

protected slots:

    void onOpenFileClicked(bool);
    void onReloadFileClicked(bool);
    void onExitClicked(bool);
    void onTestViewportClicked(bool);

private:

    // Private non-virtual methods

    void loadFile(const std::string& filename);

	void saveSettings();

}; // END of class ProfMainWindow.

//////////////////////////////////////////////////////////////////////////

#endif // EASY_PROFILER_GUI__MAIN_WINDOW__H
