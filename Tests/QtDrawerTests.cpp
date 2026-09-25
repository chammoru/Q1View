#include "../ViewerQt/ThumbnailPane.h"
#include "../ViewerQt/MainWindow.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QFontInfo>
#include <QUrl>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QFileSystemWatcher>
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
    static void recycleTests() {
        QTemporaryDir fixture;
        check(fixture.isValid(), "recycle fixture is isolated");
        const QString sub = fixture.path() + "/folder";
        check(QDir().mkpath(sub), "directory exclusion fixture created");
        QImage image(16, 16, QImage::Format_RGB32); image.fill(Qt::green);
        QStringList files;
        for (const QString& name : {QString("a.png"), QString("b.png"), QString("c.png"),
            QString::fromUtf8("\xEC\x82\xAC\xEC\xA7\x84.png")}) {
            files.append(fixture.path() + '/' + name);
            check(image.save(files.back()), "recycle photo fixture created");
        }
        ThumbnailPane pane;
        pane.setNameFilters({"*.png"}); pane.resize(400, 700); pane.show();
        pane.setCurrentFile(files[0]);
        auto rowFor = [&](const QString& path) {
            for (int i = 0; i < pane.count(); ++i) if (pane.item(i)->data(Qt::UserRole + 1).toString() == path) return i;
            return -1;
        };
        auto click = [&](int r, Qt::KeyboardModifiers modifiers, Qt::MouseButton button = Qt::LeftButton) {
            pane.scrollToItem(pane.item(r)); pump(1);
            QMouseEvent event(QEvent::MouseButtonPress, pane.visualItemRect(pane.item(r)).center(), button, button, modifiers);
            QApplication::sendEvent(pane.viewport(), &event);
        };
        const int first = rowFor(files[0]), last = rowFor(files[3]);
        click(first, Qt::NoModifier); click(last, Qt::ControlModifier);
        check(pane.selectedMediaPaths() == QStringList({files[0], files[3]}) && pane.selectedItems().size() == 2,
            "Qt Ctrl-click toggles a noncontiguous selection");
        click(last, Qt::ControlModifier);
        check(pane.selectedMediaPaths() == QStringList({files[0]}), "Qt Ctrl-click removes a selected file");
        click(first, Qt::NoModifier); click(first + 2, Qt::ShiftModifier);
        check(pane.selectedMediaPaths().size() == 3, "Qt Shift-click selects the contiguous range");
        click(last, Qt::ControlModifier | Qt::ShiftModifier);
        check(pane.selectedMediaPaths().size() == 4, "Qt Ctrl-Shift adds the range");
        click(first, Qt::NoModifier); click(last, Qt::ControlModifier);
        click(first, Qt::NoModifier, Qt::RightButton);
        check(pane.selectedMediaPaths().size() == 2, "Qt right-click preserves an existing multi-selection");
        std::unique_ptr<QMenu> menu(pane.createContextMenu(pane.item(first)));
#ifdef Q_OS_WIN
        const QString deleteLabel = "Move 2 items to Recycle Bin";
#else
        const QString deleteLabel = "Move 2 items to Trash";
#endif
        check(action(menu.get(), deleteLabel), "Qt recycle command includes the selected count");
        int confirmations = 0;
        pane.mConfirmRecycle = [&](int count) { ++confirmations; check(count == 2, "Qt confirmation captures the batch count"); return false; };
        action(menu.get(), deleteLabel)->trigger();
        check(confirmations == 1 && QFileInfo::exists(files[0]) && QFileInfo::exists(files[3]), "cancel keeps files and selection");
        QKeyEvent shiftDelete(QEvent::KeyPress, Qt::Key_Delete, Qt::ShiftModifier);
        QApplication::sendEvent(&pane, &shiftDelete);
        check(confirmations == 1, "Shift-Delete never invokes a permanent-delete operation");
        QKeyEvent all(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
        QApplication::sendEvent(&pane, &all);
        check(pane.selectedMediaPaths() == files && pane.selectedItems().size() == 4, "Ctrl-A excludes parent and directory entries");
        pane.selectionModel()->select(pane.model()->index(first, 0), QItemSelectionModel::ClearAndSelect);
        check(pane.selectedMediaPaths() == QStringList({files[0]}),
            "native Qt selection updates stay in sync with the drawer model");
        QApplication::sendEvent(&pane, &all);
        pane.mConfirmRecycle = [&](int) { pane.navigateTo(sub); return true; };
        pane.recycleSelected();
        check(pane.mFolder == sub && QFileInfo::exists(files[0]), "folder change during confirmation invalidates the deletion request");
        action(menu.get(), deleteLabel)->trigger();
        check(QFileInfo::exists(files[3]), "stale context action cannot delete recycled row indices");
        pane.setCurrentFile(files[0]);
        pane.selectRow(rowFor(files[0])); pane.selectRow(rowFor(files[3]), Qt::ControlModifier);
        QString report;
        pane.mReportRecycle = [&](const QString& message) { report = message; };
#ifdef Q_OS_WIN
        HANDLE locked = CreateFileW(files[0].toStdWString().c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        check(locked != INVALID_HANDLE_VALUE, "locked-file fixture opened without delete sharing");
        pane.mConfirmRecycle = [](int) { return true; };
#else
        pane.mConfirmRecycle = [&](int) { QFile::remove(files[0]); return true; };
#endif
        QKeyEvent remove(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
        QApplication::sendEvent(&pane, &remove);
#ifdef Q_OS_WIN
        CloseHandle(locked);
        check(QFileInfo::exists(files[0]) && pane.selectedMediaPaths() == QStringList({files[0]}),
            "locked failure stays in the folder and selected");
#endif
        check(!QFileInfo::exists(files[3]) && report.contains("1 of 2") && QFileInfo(sub).isDir(),
            "partial failure retains Unicode recycle success and excludes directories");
        // Recreate only our fixture, then exercise the real MainWindow ownership.
        image.save(files[0]);
        MainWindow window; window.setQuiet(true); window.show();
        check(window.openFile(files[1]), "active recycle photo opens"); pump(30);
        auto& drawer = *window.mThumbPane;
        drawer.setCurrentFile(files[1]);
        drawer.mConfirmRecycle = [](int) { return true; };
#ifdef Q_OS_WIN
        QString activeFailure;
        drawer.mReportRecycle = [&](const QString& message) { activeFailure = message; };
        HANDLE activeLock = CreateFileW(files[1].toStdWString().c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        check(activeLock != INVALID_HANDLE_VALUE, "active recycle failure fixture opened");
        drawer.recycleSelected(); CloseHandle(activeLock); pump(30);
        check(activeFailure.contains("0 of 1") && window.mCurrentFile == files[1] && !window.mImage.isNull() &&
            drawer.selectedMediaPaths() == QStringList({files[1]}),
            "failed active recycle reopens the image and preserves its selection");
#endif
        drawer.mReportRecycle = [](const QString&) { check(false, "active recycle should succeed"); };
        const QRect geometry = window.geometry();
        window.mReloadTimer->start();
        drawer.recycleSelected(); pump(40);
        check(!QFileInfo::exists(files[1]) && window.mCurrentFile == files[2] && window.geometry() == geometry,
            "active photo recycling selects next survivor without resizing");
        check(!window.mReloadTimer->isActive() && !window.mFileWatcher->files().contains(files[1]),
            "old watcher and queued reload are invalidated");
        drawer.recycleSelected(); pump(30);
        check(window.mCurrentFile == files[0] && !QFileInfo::exists(files[2]), "last active item selects previous survivor");
        drawer.recycleSelected(); pump(30);
        check(window.mCurrentFile.isEmpty() && window.mImage.isNull() && !window.mShowingVideo && QFileInfo(sub).isDir(),
            "deleting final media leaves an empty view and preserves directories");
    }
    static void run() {
        recycleTests();
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
        check(QFontInfo(pane.font()).family().contains("Pretendard", Qt::CaseInsensitive),
            "bundled Pretendard typography loaded for Qt drawer");
        check(pane.font().pixelSize() == 13 && pane.font().weight() == QFont::Normal,
            "Qt file labels use the 13 px regular typography target");
        check(pane.item(0) && pane.item(0)->text() == "[..]" && pane.item(0)->toolTip() == "..",
            "typographic parent entry is first in Qt drawer");
        check(pane.item(0)->font().pixelSize() == 13 && pane.item(0)->font().weight() == QFont::Medium,
            "Qt directory labels use the 13 px medium typography target");
        std::unique_ptr<QMenu> parentMenu(pane.createContextMenu(pane.item(0)));
        check(parentMenu->isEmpty(), "Qt parent navigation uses the entry, not a context command");
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
        check(pane.item(0) && pane.item(0)->text() == "[..]", "parent entry remains visible after folder navigation");
        pane.onItemActivated(pane.item(0));
        check(pane.mFolder == fixture.path() && pane.currentItem() &&
            pane.currentItem()->data(Qt::UserRole + 1).toString() == child, "Qt parent entry returns and reveals child");
        check(pane.currentItem()->text() == QString::fromUtf8("[\xED\x95\x98\xEC\x9C\x84 folder]") &&
            pane.currentItem()->toolTip() == QString::fromUtf8("\xED\x95\x98\xEC\x9C\x84 folder"),
            "Qt Unicode folder uses bracket notation while tooltip keeps its original name");
        pane.navigateTo(child);
        QKeyEvent up(QEvent::KeyPress, Qt::Key_Up, Qt::AltModifier);
        QApplication::sendEvent(&pane, &up);
        check(pane.mFolder == fixture.path(), "Alt+Up parent navigation");
        check(!pane.navigateTo(child + "/missing") && pane.mFolder == fixture.path(), "missing directory preserves state");
        for (int i = 0; i < 30; ++i) { pane.navigateTo(child); pane.goToParent(); pump(1); }
        check(activations == 0, "rapid navigation and thumbnail decoding never activate media");
        pane.navigateTo(QDir::rootPath());
        menu.reset(pane.createContextMenu(nullptr));
        check(!pane.canGoToParent() && menu->isEmpty(), "root omits parent entry and redundant background command");
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
            QTemporaryDir recycleFolder;
            check(recycleFolder.isValid(), "isolated Qt playing-video recycle directory");
            const QString playing = recycleFolder.path() + "/a-playing." + QFileInfo(video).suffix();
            const QString survivor = recycleFolder.path() + "/b-next.png";
            check(QFile::copy(video, playing) && image.save(survivor), "playing-video deletion uses only a fixture copy");
            check(window.openFile(playing), "Qt recycle copy opens");
            wait.restart();
            while (wait.elapsed() < 10000 && (!window.mVideoView->isPlaying() || player->position() == 0)) pump(50);
            check(window.mVideoView->isPlaying() && player->position() > 0, "Qt recycle fixture is actively playing");
            window.mThumbPane->setCurrentFile(playing);
            window.mThumbPane->mConfirmRecycle = [](int) { return true; };
            window.mThumbPane->mReportRecycle = [](const QString&) { check(false, "Qt playing-video recycling should succeed"); };
            window.mThumbPane->recycleSelected(); pump(100);
            check(!QFileInfo::exists(playing) && QFileInfo::exists(video) && window.mCurrentFile == survivor &&
                !window.mShowingVideo && !window.mVideoView->isPlaying() && player->source().isEmpty(),
                "Qt video releases its source before recycling and opens the next survivor");
        }
#endif
    }
};
int RunQtDrawerTests() {
    try { QtDrawerTests::run(); qInfo("ALL QT DRAWER TESTS PASSED"); return 0; }
    catch (const std::exception& error) { qCritical("FAIL: %s", error.what()); return 1; }
}
