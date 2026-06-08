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
	// Headless smoke check: open the positional file, then exit immediately with
	// 0 on success / 1 on failure instead of entering the event loop. Drives the
	// automated Qt smoke tests (Tests/run_qt_smoke.sh) under the offscreen QPA
	// platform; not intended for interactive use.
	const QCommandLineOption selfTestOption(
		QStringList() << QStringLiteral("selftest"),
		QStringLiteral("Open the file, report success/failure as the exit code, and quit."));

	parser.addOption(rawOption);
	parser.addOption(widthOption);
	parser.addOption(heightOption);
	parser.addOption(formatOption);
	parser.addOption(selfTestOption);
	parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Image or raw frame file to open."));
	parser.process(app);

	MainWindow window;
	const bool selfTest = parser.isSet(selfTestOption);
	// Headless: surface open/decode failures as warnings, not modal dialogs that
	// would block forever with no one to dismiss them.
	window.setQuiet(selfTest);
	bool opened = false;
	const QStringList positionalArgs = parser.positionalArguments();
	if (!positionalArgs.isEmpty()) {
		const QString fileName = positionalArgs.first();
		if (parser.isSet(rawOption)) {
			bool widthOk = false;
			bool heightOk = false;
			const int width = parser.value(widthOption).toInt(&widthOk);
			const int height = parser.value(heightOption).toInt(&heightOk);

			if (widthOk && heightOk && width > 0 && height > 0) {
				opened = window.openRawFile(fileName, width, height, parser.value(formatOption));
			} else {
				parser.showHelp(1);
			}
		} else {
			opened = window.openFile(fileName);
		}
	}

	if (selfTest) {
		if (positionalArgs.isEmpty()) {
			qWarning("--selftest requires a file argument");
			return 1;
		}
		return opened ? 0 : 1;
	}

	window.show();
	return app.exec();
}
