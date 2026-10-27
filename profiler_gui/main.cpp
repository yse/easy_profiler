/************************************************************************
* file name         : main.cpp
* ----------------- :
* creation time     : 2016/04/29
* authors           : Sergey Yagovtsev, Victor Zarubkin
* email             : yse.sey@gmail.com, v.s.zarubkin@gmail.com
* ----------------- :
* description       : Main file for EasyProfiler GUI.
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2019  Sergey Yagovtsev, Victor Zarubkin
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

#include <chrono>
#include <cstdio>
#include <cstring>
#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QImageReader>
#include <QToolBar>
#include "main_window.h"
#include "globals.h"
#include "thread_pool.h"

#if defined(_WIN32) && defined (_BUILD_RELEASE_)
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

namespace {

// An icon is asked for a pixmap rather than for availableSizes(): a scalable
// SVG icon served by Qt's SVG icon engine reports no available sizes at all,
// which is correct and says nothing about whether it renders.
bool rendersAtAll(const QIcon& icon)
{
    return !icon.isNull() && !icon.pixmap(QSize(16, 16)).isNull();
}

// Every icon in the application is an SVG resource, which Qt reads through a
// plugin it loads at run time. A bundle missing that plugin starts normally and
// draws empty toolbars with no diagnostic, so it is checked directly rather
// than inferred from any one icon.
QString checkSvgSupport()
{
    if (!QImageReader::supportedImageFormats().contains(QByteArrayLiteral("svg")))
    {
        return QStringLiteral("Qt cannot read SVG; the Qt SVG image plugin is missing");
    }

    if (!rendersAtAll(QIcon(QStringLiteral(":/images/logo"))))
    {
        return QStringLiteral("the application icon does not render");
    }

    return QString();
}

// Returns what is wrong with the window, or an empty string when it is healthy.
//
// Counts icons rather than demanding that every one render: one icon with a bad
// resource path is a cosmetic defect in a single widget, while a bundle without
// the SVG plugin loses all of them at once. Anything that does not render is
// reported either way.
QString checkMainWindow(const MainWindow& window)
{
    const QList<QToolBar*> toolbars = window.findChildren<QToolBar*>();
    if (toolbars.isEmpty())
    {
        return QStringLiteral("no toolbars were created");
    }

    int renderable = 0;
    for (const QToolBar* toolbar : toolbars)
    {
        for (const QAction* action : toolbar->actions())
        {
            if (action->icon().isNull())
            {
                continue;
            }
            if (!rendersAtAll(action->icon()))
            {
                std::fprintf(stderr, "warning: the icon of action \"%s\" does not render\n",
                             qPrintable(action->text()));
                continue;
            }
            ++renderable;
        }
    }

    // The toolbars carry about a dozen icons between them.
    if (renderable < 8)
    {
        return QStringLiteral("only %1 toolbar icons render").arg(renderable);
    }

    return QString();
}

// Builds the main window, lets it settle and reports whether it is usable.
// This is what a release job runs against a deployed bundle.
int runSelfTest()
{
    QString problem = checkSvgSupport();

    if (problem.isEmpty())
    {
        MainWindow window;
        window.show();
        QApplication::processEvents();
        problem = checkMainWindow(window);
    }

    if (!problem.isEmpty())
    {
        std::fprintf(stderr, "profiler_gui self-test failed: %s\n", qPrintable(problem));
        return 1;
    }

    std::fprintf(stdout, "profiler_gui self-test passed\n");
    return 0;
}

bool hasSelfTestFlag(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] != nullptr && std::strcmp(argv[i], "--self-test") == 0)
        {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    auto now = ::std::chrono::duration_cast<std::chrono::seconds>(::std::chrono::system_clock::now().time_since_epoch()).count() >> 1;
    srand((unsigned int)now);

    // Instanciate thread pool to avoid data races for compilers without C++11 thread-safe statics
    ThreadPool::instance();

    QApplication app(argc, argv);

    // Instanciate easy globals after QApplication to allow creation of global fonts, and on the main thread to avoid data races
    profiler_gui::Globals::instance();

    if (hasSelfTestFlag(argc, argv))
    {
        return runSelfTest();
    }

    MainWindow window;
    window.show();

    return app.exec();
}
