#include <QApplication>
#include <QTreeView>
#include <QFileSystemModel>
#include "treemodel.h"

int main(int argc, char **argv)
{
	QApplication app(argc, argv);


	//QFileSystemModel *model = new QFileSystemModel;
	//model->setRootPath(QDir::currentPath());

    QFile file("/home/yse/projects/easy_profiler/bin/test.prof");
	file.open(QIODevice::ReadOnly);
    TreeModel model(file.readAll());
	file.close();


	QTreeView *tree = new QTreeView();
	tree->setModel(&model);

	tree->show();

	return app.exec(	);
}
