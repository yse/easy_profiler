#include <QApplication>
#include <QTreeView>
#include <QFileSystemModel>
#include "treemodel.h"
#include "blocks_graphics_view.h"
#include "blocks_tree_widget.h"
#include "main_window.h"
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

    int mode = 2;

    switch (mode)
    {
        case 0:
        {
            const bool test = false;

            if (test)
            {
                // srand for random colors in test
                unsigned int* rseed = new unsigned int;
                srand(*rseed);
                delete rseed;

                ProfGraphicsView gview(true);
                gview.show();

                return app.exec();
            }

            thread_blocks_tree_t threaded_trees;
            fillTreesFromFile("test.prof", threaded_trees, true);

            ProfGraphicsView gview(threaded_trees);
            gview.show();

            return app.exec();
        }

        case 1:
        {
            thread_blocks_tree_t threaded_trees;
            auto nblocks = fillTreesFromFile("test.prof", threaded_trees, true);

            ProfTreeWidget view(nblocks, threaded_trees);
            view.show();

            return app.exec();
        }

        case 2:
        {
            ProfMainWindow window;
            window.show();
            return app.exec();
        }
    }

    return -1;
}
