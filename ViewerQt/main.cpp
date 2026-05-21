#include "MainWindow.h"

#include <QApplication>
#include <QStringList>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);

	MainWindow window;
	const QStringList args = app.arguments();
	if (args.size() > 1) {
		window.openFile(args.at(1));
	}

	window.show();
	return app.exec();
}
