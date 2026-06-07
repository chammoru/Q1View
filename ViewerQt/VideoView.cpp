#include "VideoView.h"

#include <QAudioOutput>
#include <QBoxLayout>
#include <QFileInfo>
#include <QLabel>
#include <QMediaPlayer>
#include <QSlider>
#include <QStyle>
#include <QTime>
#include <QToolButton>
#include <QUrl>
#include <QVideoWidget>

namespace {

// Container extensions the player advertises. QMediaPlayer's actual decode
// support depends on the platform backend (FFmpeg on most install-qt builds),
// but this list is the practical superset the Open dialog and directory
// navigation offer; an unsupported codec surfaces through errorOccurred().
const char *const kVideoExtensions[] = {
	"mp4", "m4v", "mov", "mkv", "webm", "avi", "wmv", "flv", "mpg", "mpeg",
	"ts", "m2ts", "3gp", "ogv",
};

QString formatTime(qint64 ms)
{
	if (ms < 0) {
		ms = 0;
	}
	const QTime t = QTime(0, 0).addMSecs(static_cast<int>(ms));
	// Drop the hours field for short clips, like a typical media player.
	return ms >= 3600 * 1000 ? t.toString(QStringLiteral("h:mm:ss"))
							  : t.toString(QStringLiteral("m:ss"));
}

} // namespace

VideoView::VideoView(QWidget *parent)
	: QWidget(parent),
	  mPlayer(new QMediaPlayer(this)),
	  mAudioOutput(new QAudioOutput(this)),
	  mVideoWidget(new QVideoWidget(this)),
	  mPlayButton(new QToolButton(this)),
	  mMuteButton(new QToolButton(this)),
	  mPositionSlider(new QSlider(Qt::Horizontal, this)),
	  mVolumeSlider(new QSlider(Qt::Horizontal, this)),
	  mTimeLabel(new QLabel(this)),
	  mUpdatingSlider(false)
{
	mPlayer->setVideoOutput(mVideoWidget);
	mPlayer->setAudioOutput(mAudioOutput);
	mAudioOutput->setVolume(1.0);

	mVideoWidget->setMinimumSize(160, 90);

	mPlayButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	mPlayButton->setToolTip(tr("Play/Pause (Space)"));
	mPlayButton->setAutoRaise(true);

	mMuteButton->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
	mMuteButton->setToolTip(tr("Mute"));
	mMuteButton->setCheckable(true);
	mMuteButton->setAutoRaise(true);

	mPositionSlider->setRange(0, 0);
	mTimeLabel->setText(QStringLiteral("0:00 / 0:00"));
	mTimeLabel->setMinimumWidth(96);

	mVolumeSlider->setRange(0, 100);
	mVolumeSlider->setValue(100);
	mVolumeSlider->setMaximumWidth(96);
	mVolumeSlider->setToolTip(tr("Volume"));

	QHBoxLayout *controls = new QHBoxLayout;
	controls->setContentsMargins(6, 4, 6, 4);
	controls->setSpacing(6);
	controls->addWidget(mPlayButton);
	controls->addWidget(mPositionSlider, 1);
	controls->addWidget(mTimeLabel);
	controls->addWidget(mMuteButton);
	controls->addWidget(mVolumeSlider);

	QVBoxLayout *layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(mVideoWidget, 1);
	layout->addLayout(controls);

	connect(mPlayButton, &QToolButton::clicked, this, &VideoView::togglePlay);

	connect(mPlayer, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
		if (!mPositionSlider->isSliderDown()) {
			mUpdatingSlider = true;
			mPositionSlider->setValue(static_cast<int>(position));
			mUpdatingSlider = false;
		}
		setPositionLabel(position, mPlayer->duration());
	});
	connect(mPlayer, &QMediaPlayer::durationChanged, this, [this](qint64 duration) {
		mPositionSlider->setRange(0, static_cast<int>(duration));
		setPositionLabel(mPlayer->position(), duration);
	});
	connect(mPlayer, &QMediaPlayer::playbackStateChanged, this,
		[this](QMediaPlayer::PlaybackState) { updatePlayIcon(); });
	connect(mPlayer, &QMediaPlayer::mediaStatusChanged, this,
		[this](QMediaPlayer::MediaStatus status) {
			// LoadedMedia fires once, when the clip is ready (duration known);
			// the host uses it to confirm the source actually opened.
			if (status == QMediaPlayer::LoadedMedia) {
				emit loaded();
			}
		});
	connect(mPlayer, &QMediaPlayer::errorOccurred, this,
		[this](QMediaPlayer::Error error, const QString &errorString) {
			if (error != QMediaPlayer::NoError) {
				emit errorOccurred(errorString);
			}
		});

	// Seek only on user-driven slider moves (mUpdatingSlider guards the
	// programmatic updates from positionChanged above).
	connect(mPositionSlider, &QSlider::valueChanged, this, [this](int value) {
		if (!mUpdatingSlider && mPlayer->isSeekable()) {
			mPlayer->setPosition(value);
		}
	});

	connect(mVolumeSlider, &QSlider::valueChanged, this, [this](int value) {
		mAudioOutput->setVolume(value / 100.0f);
		if (value > 0 && mMuteButton->isChecked()) {
			mMuteButton->setChecked(false);
		}
	});
	connect(mMuteButton, &QToolButton::toggled, this, [this](bool muted) {
		mAudioOutput->setMuted(muted);
		mMuteButton->setIcon(style()->standardIcon(
			muted ? QStyle::SP_MediaVolumeMuted : QStyle::SP_MediaVolume));
	});
}

VideoView::~VideoView() = default;

QStringList VideoView::supportedExtensions()
{
	QStringList extensions;
	for (const char *ext : kVideoExtensions) {
		extensions << QString::fromLatin1(ext);
	}
	return extensions;
}

bool VideoView::isVideoFile(const QString &fileName)
{
	const QString suffix = QFileInfo(fileName).suffix().toLower();
	for (const char *ext : kVideoExtensions) {
		if (suffix == QLatin1String(ext)) {
			return true;
		}
	}
	return false;
}

QStringList VideoView::nameFilters()
{
	QStringList filters;
	for (const char *ext : kVideoExtensions) {
		filters << QStringLiteral("*.%1").arg(QString::fromLatin1(ext));
	}
	return filters;
}

bool VideoView::open(const QString &fileName)
{
	mPlayer->setSource(QUrl::fromLocalFile(fileName));
	mPlayer->play();
	return mPlayer->error() == QMediaPlayer::NoError;
}

void VideoView::stop()
{
	mPlayer->stop();
	mPlayer->setSource(QUrl());
}

void VideoView::play()
{
	mPlayer->play();
}

void VideoView::pause()
{
	mPlayer->pause();
}

bool VideoView::isPlaying() const
{
	return mPlayer->playbackState() == QMediaPlayer::PlayingState;
}

void VideoView::togglePlay()
{
	// Driven by the transport bar's play/pause button. Emit the intended state
	// (playbackState() updates asynchronously) so the host can mirror it.
	const bool willPlay = mPlayer->playbackState() != QMediaPlayer::PlayingState;
	if (willPlay) {
		mPlayer->play();
	} else {
		mPlayer->pause();
	}
	emit playbackToggled(willPlay);
}

void VideoView::updatePlayIcon()
{
	const bool playing = mPlayer->playbackState() == QMediaPlayer::PlayingState;
	mPlayButton->setIcon(style()->standardIcon(
		playing ? QStyle::SP_MediaPause : QStyle::SP_MediaPlay));
}

void VideoView::setPositionLabel(qint64 position, qint64 duration)
{
	mTimeLabel->setText(QStringLiteral("%1 / %2")
		.arg(formatTime(position), formatTime(duration)));
}
