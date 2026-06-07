#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QIcon>
#include <QStringList>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QCoreApplication::setOrganizationName(QStringLiteral("Q1View"));
	QCoreApplication::setApplicationName(QStringLiteral("Q1ViewQt"));
	// Window / taskbar icon, shared with the MFC viewer (Viewer/res/Viewer.ico).
	app.setWindowIcon(QIcon(QStringLiteral(":/Viewer.ico")));

	QCommandLineParser parser;
	parser.setApplicationDescription(QStringLiteral("Experimental cross-platform Q1View Qt viewer"));
	parser.addHelpOption();

	const QCommandLineOption rawOption(
		QStringList() << QStringLiteral("r") << QStringLiteral("raw"),
		QStringLiteral("Open the positional file as a raw image."));
	const QCommandLineOption widthOption(
		QStringList() << QStringLiteral("w") << QStringLiteral("width"),
		QStringLiteral("Raw image width."),
		QStringLiteral("pixels"));
	const QCommandLineOption heightOption(
		QStringList() << QStringLiteral("H") << QStringLiteral("height"),
		QStringLiteral("Raw image height."),
		QStringLiteral("pixels"));
	const QCommandLineOption formatOption(
		QStringList() << QStringLiteral("f") << QStringLiteral("format"),
		QStringLiteral("Raw image color space, for example yuv420, nv12, bgr888."),
		QStringLiteral("name"),
		QStringLiteral("yuv420"));

	parser.addOption(rawOption);
	parser.addOption(widthOption);
	parser.addOption(heightOption);
	parser.addOption(formatOption);
	parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Image or raw frame file to open."));
	parser.process(app);

	MainWindow window;
	const QStringList positionalArgs = parser.positionalArguments();
	if (!positionalArgs.isEmpty()) {
		const QString fileName = positionalArgs.first();
		if (parser.isSet(rawOption)) {
			bool widthOk = false;
			bool heightOk = false;
			const int width = parser.value(widthOption).toInt(&widthOk);
			const int height = parser.value(heightOption).toInt(&heightOk);

			if (widthOk && heightOk && width > 0 && height > 0) {
				window.openRawFile(fileName, width, height, parser.value(formatOption));
			} else {
				parser.showHelp(1);
			}
		} else {
			window.openFile(fileName);
		}
	}

	window.show();
	return app.exec();
}
