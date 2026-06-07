#pragma once

#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QtGlobal>

class QUdpSocket;
class QString;
class QTime;

class UdpMessageHandler final : public QObject
{
    Q_OBJECT

public:
    explicit UdpMessageHandler(QObject *parent = nullptr);
    bool startQso(const QString &wsjtId,
                  const QTime &time,
                  qint32 snr,
                  double deltaTime,
                  quint32 deltaFrequency,
                  const QString &mode,
                  const QString &message,
                  bool lowConfidence);

signals:
    void decodeRecordReceived(const QString &wsjtId,
                              const QTime &time,
                              const QString &band,
                              const QString &callsign,
                              const QString &grid,
                              const QString &message,
                              qint32 snr,
                              double deltaTime,
                              quint32 deltaFrequency,
                              const QString &mode,
                              bool lowConfidence);

private slots:
    void readPendingDatagrams();

private:
    void parseMessage(QByteArray buffer, const QHostAddress &sender, quint16 senderPort);

    QUdpSocket *udpSocket = nullptr;
};
