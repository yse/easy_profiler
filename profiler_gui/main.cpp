#include <QApplication>
#include <QTreeView>
#include <QFileSystemModel>
#include "treemodel.h"

int main(int argc, char **argv)
{
	QApplication app(argc, argv);


	//QFileSystemModel *model = new QFileSystemModel;
	//model->setRootPath(QDir::currentPath());
	const char* filename = 0;
    if(argc > 1 && argv[1]){
		filename = argv[1];
    }else{
		return 255;
	}

    QFile file(filename);
	file.open(QIODevice::ReadOnly);
    TreeModel model(file.readAll());
	file.close();


	QTreeView *tree = new QTreeView();
	tree->setModel(&model);

	tree->show();

	return app.exec(	);
}
