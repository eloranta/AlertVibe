#pragma once

#include <QByteArray>
#include <QObject>

class QUdpSocket;

class UdpMessageHandler final : public QObject
{
    Q_OBJECT

public:
    explicit UdpMessageHandler(QObject *parent = nullptr);

private slots:
    void readPendingDatagrams();

private:
    void parseMessage(QByteArray buffer);

    QUdpSocket *udpSocket = nullptr;
};
