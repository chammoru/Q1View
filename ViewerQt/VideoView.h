#ifndef Q1VIEW_VIEWERQT_VIDEOVIEW_H
#define Q1VIEW_VIEWERQT_VIDEOVIEW_H

#include <QString>
#include <QStringList>
#include <QWidget>

class QAudioOutput;
class QLabel;
class QMediaPlayer;
class QSlider;
class QToolButton;
class QVideoWidget;

// Self-contained video-file player used when the build links Qt6::Multimedia.
// It hosts a QVideoWidget plus a transport bar (play/pause, a seekable position
// slider with elapsed/total time, and a volume slider with mute), mirroring the
// MFC viewer's in-canvas playback controls (Viewer/AudioPlayer.cpp drives audio
// there; here QMediaPlayer's own QAudioOutput does). Scope is *file playback*;
// capture-device preview (the Windows VidCapThread/OpenCV path) is intentionally
// out of scope for this first pass.
//
// The whole class is only compiled behind Q1VIEW_ENABLE_QT_MULTIMEDIA, so the
// viewer still builds with a Widgets-only Qt where the module is absent.
class VideoView : public QWidget
{
	Q_OBJECT

public:
	explicit VideoView(QWidget *parent = nullptr);
	~VideoView() override;

	// File extensions (without the dot) the player advertises and accepts.
	static QStringList supportedExtensions();
	// True when fileName carries one of supportedExtensions(), case-insensitively.
	static bool isVideoFile(const QString &fileName);
	// "*.mp4 *.mkv ..." for an Open dialog filter / directory navigation.
	static QStringList nameFilters();

	// Load and start playing fileName. Returns false if the player reports an
	// error opening it. Note media loads asynchronously, so a true return only
	// means "no immediate error"; watch loaded()/errorOccurred() for the result.
	bool open(const QString &fileName);
	void stop();
	void play();
	void pause();
	// Toggle play/pause as if the transport-bar button was pressed; emits
	// playbackToggled() so the host can mirror it.
	void togglePlay();
	bool isPlaying() const;

signals:
	// Emitted once the current source has actually loaded and is ready to play.
	void loaded();
	// Emitted when the user toggles playback via the transport bar's play/pause
	// button (not for programmatic play()/pause()), carrying the new intended
	// state. Lets the host mirror the button press to other windows.
	void playbackToggled(bool playing);
	// Emitted when the player fails to decode/open the current source, so the
	// host window can surface a message and reset to an empty view.
	void errorOccurred(const QString &message);

private:
	void updatePlayIcon();
	void setPositionLabel(qint64 position, qint64 duration);

	QMediaPlayer *mPlayer;
	QAudioOutput *mAudioOutput;
	QVideoWidget *mVideoWidget;
	QToolButton *mPlayButton;
	QToolButton *mMuteButton;
	QSlider *mPositionSlider;
	QSlider *mVolumeSlider;
	QLabel *mTimeLabel;
	// True while we are programmatically moving mPositionSlider in response to a
	// positionChanged signal, so its valueChanged does not seek back on itself.
	bool mUpdatingSlider;
};

#endif
