#include <QApplication>
#include <QTreeView>
#include <QFileSystemModel>

int main(int argc, char **argv)
{
	QApplication app(argc, argv);


	QFileSystemModel *model = new QFileSystemModel;
	model->setRootPath(QDir::currentPath());
	QTreeView *tree = new QTreeView();
	tree->setModel(model);

	tree->show();

	return app.exec();
}