#include <QApplication>
#include <QTreeView>
#include <QFileSystemModel>
#include <chrono>
//#include "treemodel.h"
#include "main_window.h"
#include "profiler/reader.h"

#ifdef WIN32
#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")
#endif

int main(int argc, char **argv)
{
	QApplication app(argc, argv);

	//QFileSystemModel *model = new QFileSystemModel;
	//model->setRootPath(QDir::currentPath());
// 	const char* filename = 0;
//     if(argc > 1 && argv[1]){
// 		filename = argv[1];
//     }else{
// 		return 255;
// 	}

//     QFile file(filename);
// 	file.open(QIODevice::ReadOnly);
//     TreeModel model(file.readAll());
// 	file.close();


// 	QTreeView *tree = new QTreeView();
// 	tree->setModel(&model);
// 
// 	tree->show();

    auto now = ::std::chrono::duration_cast<std::chrono::seconds>(::std::chrono::system_clock::now().time_since_epoch()).count() >> 1;
    srand((unsigned int)now);

    ProfMainWindow window;
    window.show();

    return app.exec();
}
