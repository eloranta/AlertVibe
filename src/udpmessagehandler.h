#pragma once

#include <QByteArray>
#include <QObject>

class QUdpSocket;
class QString;
class QTime;

class UdpMessageHandler final : public QObject
{
    Q_OBJECT

public:
    explicit UdpMessageHandler(QObject *parent = nullptr);

signals:
    void decodeRecordReceived(const QTime &time,
                              const QString &callsign,
                              const QString &grid,
                              const QString &message);

private slots:
    void readPendingDatagrams();

private:
    void parseMessage(QByteArray buffer);

    QUdpSocket *udpSocket = nullptr;
};
