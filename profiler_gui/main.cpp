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
#include <QApplication>
//#include <QTreeView>
//#include <QFileSystemModel>
//#include "treemodel.h"
#include "main_window.h"
#include "easy/reader.h"


//#ifdef _WIN32
//#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
//#endif

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    //QFileSystemModel *model = new QFileSystemModel;
    //model->setRootPath(QDir::currentPath());
//     const char* filename = 0;
//     if(argc > 1 && argv[1]){
//         filename = argv[1];
//     }else{
//         return 255;
//     }

//     QFile file(filename);
//     file.open(QIODevice::ReadOnly);
//     TreeModel model(file.readAll());
//     file.close();


//     QTreeView *tree = new QTreeView();
//     tree->setModel(&model);
// 
//     tree->show();

    auto now = ::std::chrono::duration_cast<std::chrono::seconds>(::std::chrono::system_clock::now().time_since_epoch()).count() >> 1;
    srand((unsigned int)now);

    EasyMainWindow window;
    window.show();

    return app.exec();
}
