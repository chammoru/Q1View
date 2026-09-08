#include "../ViewerQt/ThumbnailPane.h"
#include "../ViewerQt/MainWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QUrl>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>
#include <memory>
#include <stdexcept>
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
#include "../ViewerQt/VideoView.h"
#include <QMediaPlayer>
#endif
#ifdef Q_OS_WIN
#include "../QCommon/inc/QFileActionsWin.h"
#include <wrl/client.h>
#endif

struct QtDrawerTests {
    static void check(bool ok, const char* message) {
        if (!ok) throw std::runtime_error(message);
        qInfo("PASS: %s", message);
    }
    static void pump(int ms) {
        QElapsedTimer timer; timer.start();
        do { QApplication::processEvents(); QThread::msleep(1); } while (timer.elapsed() < ms);
    }
    static QAction* action(QMenu* menu, const QString& name) {
        for (QAction* item : menu->actions()) {
            if (item->text() == name) return item;
            if (item->menu()) if (auto* found = action(item->menu(), name)) return found;
        }
        return nullptr;
    }
    static void run() {
        QTemporaryDir fixture;
        check(fixture.isValid(), "temporary drawer fixture created");
        const QString child = fixture.path() + QString::fromUtf8("/\xED\x95\x98\xEC\x9C\x84 folder");
        check(QDir().mkpath(child), "Unicode subfolder created");
        const QString file = child + QString::fromUtf8("/\xEC\x82\xAC\xEC\xA7\x84 name.png");
        QImage image(16, 16, QImage::Format_RGB32); image.fill(Qt::blue);
        check(image.save(file), "Unicode photo fixture created");
        ThumbnailPane pane;
        pane.setNameFilters({"*.png"}); pane.resize(400, 700); pane.show();
        int activations = 0;
        QObject::connect(&pane, &ThumbnailPane::fileActivated, [&] { ++activations; });
        pane.setCurrentFile(file); pump(50);
        check(pane.currentItem() != nullptr, "current file selected");
        std::unique_ptr<QMenu> menu(pane.createContextMenu(pane.currentItem()));
        check(action(menu.get(), "Open") && action(menu.get(), "Copy") && action(menu.get(), "Copy full path"), "file menu actions available");
        check(activations == 0, "context menu construction never activates a file");
#ifdef Q_OS_WIN
        Microsoft::WRL::ComPtr<IDataObject> previous;
        OleGetClipboard(&previous);
        struct Restore { IDataObject* value; ~Restore() { OleSetClipboard(value); OleFlushClipboard(); } } restore{previous.Get()};
#endif
        action(menu.get(), "Copy full path")->trigger();
        check(QApplication::clipboard()->text() == file, "Unicode full path clipboard");
        action(menu.get(), "Copy file name")->trigger();
        check(QApplication::clipboard()->text() == QFileInfo(file).fileName(), "file name clipboard");
        action(menu.get(), "Copy")->trigger();
#ifdef Q_OS_WIN
        check(OpenClipboard(reinterpret_cast<HWND>(pane.winId())) != FALSE, "native file clipboard opened");
        HDROP drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
        wchar_t copied[32768] = {};
        const auto count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        DragQueryFileW(drop, 0, copied, _countof(copied)); CloseClipboard();
        check(count == 1 && QString::fromWCharArray(copied) == file, "Qt file copy supports Explorer paste");
        check(action(menu.get(), "Properties"), "Windows properties available");
        check(!q1view::ShowInExplorer((file + ".missing").toStdWString()) &&
            !q1view::ShowFileProperties(nullptr, (file + ".missing").toStdWString()), "missing native actions fail safely");
#else
        check(QApplication::clipboard()->mimeData()->urls() == QList<QUrl>{QUrl::fromLocalFile(file)}, "file copy contains file URL");
        check(!action(menu.get(), "Properties") && action(menu.get(), "Open containing folder"), "platform-appropriate native actions");
#endif
        QKeyEvent back(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
        QApplication::sendEvent(&pane, &back);
        check(pane.mFolder == fixture.path() && pane.currentItem() &&
            pane.currentItem()->data(Qt::UserRole + 1).toString() == child, "Backspace reveals child in parent");
        action(menu.get(), "Open")->trigger();
        check(activations == 0, "stale popup cannot open a recycled entry");
        menu.reset(pane.createContextMenu(pane.currentItem()));
        action(menu.get(), "Open folder")->trigger();
        check(pane.mFolder == child && activations == 0, "folder action never activates media");
        QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::AltModifier);
        QApplication::sendEvent(&pane, &up);
        check(pane.mFolder == fixture.path(), "Alt+Up parent navigation");
        check(!pane.navigateTo(child + "/missing") && pane.mFolder == fixture.path(), "missing directory preserves state");
        for (int i = 0; i < 30; ++i) { pane.navigateTo(child); pane.goToParent(); pump(1); }
        check(activations == 0, "rapid navigation and thumbnail decoding never activate media");
        pane.navigateTo(QDir::rootPath());
        menu.reset(pane.createContextMenu(nullptr));
        check(!pane.canGoToParent() && menu->actions().size() == 1 && !menu->actions().first()->isEnabled(), "root background disables parent");
        pane.setCurrentFile(file); QFile::remove(file);
        menu.reset(pane.createContextMenu(pane.currentItem()));
        check(!action(menu.get(), "Open")->isEnabled() && !action(menu.get(), "Copy")->isEnabled(), "removed item disables file actions");
        // A real MainWindow proves folder-only browsing is isolated from media
        // identity and the view geometry; optionally exercise actual video too.
        image.save(file);
        MainWindow window; window.setQuiet(true);
        check(window.openFile(file), "main window opens photo");
        window.show(); pump(100);
        const QRect geometry = window.geometry();
        const QString current = window.mCurrentFile;
        for (int i = 0; i < 5; ++i) { window.mThumbPane->navigateTo(child); window.mThumbPane->goToParent(); pump(5); }
        check(window.geometry() == geometry && window.mCurrentFile == current, "main photo and window remain stable during drawer navigation");
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
        const QString video = qEnvironmentVariable("Q1VIEW_GALLERY_TEST_VIDEO");
        if (!video.isEmpty()) {
            check(window.openFile(video), "Qt video opens");
            QElapsedTimer wait; wait.start();
            auto* player = window.mVideoView->findChild<QMediaPlayer*>();
            while (wait.elapsed() < 10000 && (!window.mVideoView->isPlaying() || player->position() == 0)) pump(50);
            check(window.mVideoView->isPlaying() && player->position() > 0, "Qt video advances");
            const qint64 position = player->position();
            const QRect videoGeometry = window.geometry();
            for (int i = 0; i < 15; ++i) { window.mThumbPane->navigateTo(child); window.mThumbPane->goToParent(); pump(20); }
            check(window.mVideoView->isPlaying() && player->position() >= position &&
                window.mCurrentFile == video && window.geometry() == videoGeometry, "Qt playing media continues through rapid drawer navigation");
        }
#endif
    }
};
int RunQtDrawerTests() {
    try { QtDrawerTests::run(); qInfo("ALL QT DRAWER TESTS PASSED"); return 0; }
    catch (const std::exception& error) { qCritical("FAIL: %s", error.what()); return 1; }
}
