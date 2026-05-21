#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QStringList>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QCoreApplication::setOrganizationName(QStringLiteral("Q1View"));
	QCoreApplication::setApplicationName(QStringLiteral("Q1ViewQt"));

	MainWindow window;
	const QStringList args = app.arguments();
	if (args.size() > 1) {
		window.openFile(args.at(1));
	}

	window.show();
	return app.exec();
}
