#include <QApplication>
#include <QTreeView>
#include <QFileSystemModel>
#include "treemodel.h"
#include "graphics_view.h"
#include "profiler/reader.h"

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

    const bool test = false;

    if (test)
    {
        // srand for random colors in test
        unsigned int* rseed = new unsigned int;
        srand(*rseed);
        delete rseed;

        MyGraphicsView gview;
        gview.show();

        return app.exec();
    }

    thread_blocks_tree_t threaded_trees;
    int blocks_counter = fillTreesFromFile("test.prof", threaded_trees, false);

    MyGraphicsView gview(threaded_trees);
    gview.show();

    return app.exec();
}
