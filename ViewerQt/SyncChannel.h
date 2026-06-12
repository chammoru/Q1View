#ifndef Q1VIEW_VIEWERQT_SYNCCHANNEL_H
#define Q1VIEW_VIEWERQT_SYNCCHANNEL_H

#include <QObject>
#include <QString>
#include <QtGlobal>

class QUdpSocket;

// One mirrored action, the cross-platform analogue of the MFC viewer's
// ViewerSyncInputState (Viewer/MainFrm.h). Fields are reused per command as
// noted; unused fields stay zero.
struct SyncMessage
{
	// Trimmed mirror of ViewerSyncInputCommand: the subset the Qt viewer can
	// currently honour. Wire values are explicit so they stay stable across
	// builds even if the list grows.
	enum Command : quint32 {
		None = 0,
		SeekFrame = 1,       // first = target frame index
		FirstFile = 2,
		LastFile = 3,
		PreviousFile = 4,
		NextFile = 5,
		ViewState = 6,       // scalar = zoom, x = hScroll, y = vScroll
		Rotate = 7,
		Playback = 8,        // first != 0 => play, else stop
		DisplayOptions = 9,  // first = OR of DisplayOption bits
		ColorSpace = 10,     // text = colour-space name
		Resolution = 11,     // first = width, second = height
		Fps = 12,            // scalar = frames per second
	};

	// Bit flags for the DisplayOptions command, matching the toggles the Qt
	// viewer exposes (a subset of ViewerSyncDisplayOptions).
	enum DisplayOption : quint32 {
		YOnly = 0x02,
		Interpolate = 0x04,
		Coordinates = 0x08,
		HexPixel = 0x10,
		SourceYuv = 0x20,
	};

	quint32 command = None;
	qint32 first = 0;
	qint32 second = 0;
	double scalar = 0.0;
	double x = 0.0;
	double y = 0.0;
	QString text;
};

// Loopback UDP-multicast broadcast bus replacing the Windows WM_COPYDATA path
// used for "Sync Input" (Viewer/MainFrm.cpp EnumWindows/SendMessageTimeout).
// Every ViewerQt instance with sync enabled joins the same group on the local
// host; send() multicasts a SyncMessage that all the other instances apply.
// Datagrams use TTL 0 so they never leave the machine, and each carries the
// sender's instance id so a socket ignores its own looped-back traffic.
class SyncChannel : public QObject
{
	Q_OBJECT

public:
	explicit SyncChannel(QObject *parent = nullptr);
	~SyncChannel() override;

	bool isEnabled() const { return mEnabled; }
	// Joins (true) or leaves (false) the multicast group. Returns false if the
	// socket could not be set up, in which case sync stays off.
	bool setEnabled(bool enabled);

	void send(const SyncMessage &message);

signals:
	void received(const SyncMessage &message);

private:
	void onReadyRead();

	QUdpSocket *mSocket;
	quint64 mInstanceId;
	bool mEnabled;
};

#endif
