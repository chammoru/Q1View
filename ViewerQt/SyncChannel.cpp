#include "SyncChannel.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QNetworkInterface>
#include <QRandomGenerator>
#include <QUdpSocket>
#include <QVariant>

namespace {

// 'Q','1','S','Y' — guards against unrelated traffic that happens to land on the
// port, and the version lets the layout evolve without misreading old peers.
const quint32 kMagic = 0x5131'5359u;
const quint16 kVersion = 1;
const quint16 kPort = 45531;

QHostAddress groupAddress()
{
	// Administratively-scoped IPv4 multicast (239.0.0.0/8); never routed off-link.
	return QHostAddress(QStringLiteral("239.255.43.21"));
}

// Pin the wire format so it doesn't drift with Qt's default. Qt_5_15 exists in
// both Qt5 and Qt6, keeping the (still-supported) Qt5 CMake fallback buildable;
// on Qt6 we use its native encoding.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
constexpr QDataStream::Version kStreamVersion = QDataStream::Qt_6_0;
#else
constexpr QDataStream::Version kStreamVersion = QDataStream::Qt_5_15;
#endif

} // namespace

SyncChannel::SyncChannel(QObject *parent)
	: QObject(parent),
	  mSocket(nullptr),
	  mInstanceId(0),
	  mEnabled(false)
{
	// Process-unique id: PID in the low half, a random salt in the high half, so
	// two instances launched in the same instant still differ. Used to drop our
	// own multicast, which the kernel loops back to every group member.
	const quint64 pid = static_cast<quint64>(QCoreApplication::applicationPid());
	const quint64 salt = QRandomGenerator::global()->generate64();
	mInstanceId = (salt & 0xFFFF'FFFF'0000'0000ULL) | (pid & 0xFFFF'FFFFULL);
}

SyncChannel::~SyncChannel()
{
	setEnabled(false);
}

bool SyncChannel::setEnabled(bool enabled)
{
	if (enabled == mEnabled) {
		return true;
	}

	if (!enabled) {
		if (mSocket) {
			mSocket->leaveMulticastGroup(groupAddress());
			mSocket->close();
			mSocket->deleteLater();
			mSocket = nullptr;
		}
		mEnabled = false;
		return true;
	}

	mSocket = new QUdpSocket(this);
	// ShareAddress lets several instances on one host bind the same port; without
	// it the second viewer would fail to bind and stay un-synced.
	if (!mSocket->bind(QHostAddress(QHostAddress::AnyIPv4), kPort,
			QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
		mSocket->deleteLater();
		mSocket = nullptr;
		return false;
	}

	if (!mSocket->joinMulticastGroup(groupAddress())) {
		// Some hosts won't join on the default interface; try the loopback one so
		// same-machine mirroring still works when there's no active LAN link.
		bool joined = false;
		const auto interfaces = QNetworkInterface::allInterfaces();
		for (const QNetworkInterface &iface : interfaces) {
			if ((iface.flags() & QNetworkInterface::IsUp)
				&& (iface.flags() & QNetworkInterface::CanMulticast)
				&& mSocket->joinMulticastGroup(groupAddress(), iface)) {
				joined = true;
				break;
			}
		}
		if (!joined) {
			mSocket->close();
			mSocket->deleteLater();
			mSocket = nullptr;
			return false;
		}
	}

	// Keep mirror traffic on this host only, and ensure it loops back to the
	// other local group members (on by default, set explicitly to be safe).
	mSocket->setSocketOption(QAbstractSocket::MulticastTtlOption, 0);
	mSocket->setSocketOption(QAbstractSocket::MulticastLoopbackOption, 1);

	connect(mSocket, &QUdpSocket::readyRead, this, &SyncChannel::onReadyRead);
	mEnabled = true;
	return true;
}

void SyncChannel::send(const SyncMessage &message)
{
	if (!mEnabled || !mSocket) {
		return;
	}

	QByteArray payload;
	QDataStream stream(&payload, QIODevice::WriteOnly);
	stream.setVersion(kStreamVersion);
	stream << kMagic << kVersion << mInstanceId
		   << message.command << message.first << message.second
		   << message.scalar << message.x << message.y << message.text;

	mSocket->writeDatagram(payload, groupAddress(), kPort);
}

void SyncChannel::onReadyRead()
{
	while (mSocket && mSocket->hasPendingDatagrams()) {
		const QNetworkDatagram datagram = mSocket->receiveDatagram();
		const QByteArray payload = datagram.data();

		QDataStream stream(payload);
		stream.setVersion(kStreamVersion);

		quint32 magic = 0;
		quint16 version = 0;
		quint64 senderId = 0;
		stream >> magic >> version >> senderId;
		if (magic != kMagic || version != kVersion) {
			continue;
		}
		// Drop our own datagram, looped back by the kernel to all group members.
		if (senderId == mInstanceId) {
			continue;
		}

		SyncMessage message;
		stream >> message.command >> message.first >> message.second
			   >> message.scalar >> message.x >> message.y >> message.text;
		if (stream.status() != QDataStream::Ok) {
			continue;
		}

		emit received(message);
	}
}
