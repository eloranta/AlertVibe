#include "mainwindow.h"

#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QIODevice>
#include <QString>
#include <QTime>
#include <QUdpSocket>
#include <QWidget>

namespace {
enum MessageType : quint32
{
    Heartbeat,
    Status,
    Decode,
    Clear,
    Reply,
    QSOLogged,
    Close,
    Replay,
    HaltTx,
    FreeText,
    WSPRDecode,
    Location,
    LoggedADIF,
    HighlightCallsign,
    SwitchConfiguration,
    Configure,
};

QString readUtf8String(QDataStream &stream)
{
    quint32 count = 0;
    stream >> count;

    if (count == 0xffffffff) {
        return {};
    }

    QByteArray bytes;
    bytes.resize(qsizetype(count));
    const qsizetype bytesRead = stream.readRawData(bytes.data(), bytes.size());
    if (bytesRead != bytes.size()) {
        qWarning() << "Incomplete string field in UDP message";
        const qsizetype validBytes = bytesRead > 0 ? bytesRead : 0;
        return QString::fromUtf8(bytes.constData(), int(validBytes));
    }

    return QString::fromUtf8(bytes);
}

QString messageTypeName(quint32 messageType)
{
    switch (messageType) {
    case Heartbeat:
        return "Heartbeat";
    case Status:
        return "Status";
    case Decode:
        return "Decode";
    case Clear:
        return "Clear";
    case Reply:
        return "Reply";
    case QSOLogged:
        return "QSOLogged";
    case Close:
        return "Close";
    case Replay:
        return "Replay";
    case HaltTx:
        return "HaltTx";
    case FreeText:
        return "FreeText";
    case WSPRDecode:
        return "WSPRDecode";
    case Location:
        return "Location";
    case LoggedADIF:
        return "LoggedADIF";
    case HighlightCallsign:
        return "HighlightCallsign";
    case SwitchConfiguration:
        return "SwitchConfiguration";
    case Configure:
        return "Configure";
    default:
        return QString("Unknown(%1)").arg(messageType);
    }
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("AlertVibe");
    resize(900, 600);
    setCentralWidget(new QWidget(this));

    udpSocket = new QUdpSocket(this);
    connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::readPendingDatagrams);

    if (!udpSocket->bind(QHostAddress::AnyIPv4, 2333)) {
        qWarning() << "Failed to bind UDP port 2333:" << udpSocket->errorString();
    } else {
        qDebug() << "Listening for UDP messages on port 2333";
    }
}

void MainWindow::readPendingDatagrams()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(qsizetype(udpSocket->pendingDatagramSize()));

        QHostAddress sender;
        quint16 senderPort = 0;
        udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        qDebug().noquote() << "UDP datagram from"
                           << QString("%1:%2").arg(sender.toString()).arg(senderPort)
                           << "bytes"
                           << datagram.size();
        parseMessage(datagram);
    }
}

void MainWindow::parseMessage(QByteArray buffer)
{
    QDataStream stream(&buffer, QIODevice::ReadOnly);

    quint32 magic = 0;
    stream >> magic;
    if (magic != 0xadbccbda) {
        qWarning().noquote() << "UDP message magic number not correct:"
                             << QString("0x%1").arg(magic, 8, 16, QChar('0'));
        return;
    }

    quint32 schema = 0;
    stream >> schema;
    if (schema != 2) {
        qWarning() << "UDP message schema number not 2:" << schema;
        return;
    }

    quint32 messageType = 0;
    stream >> messageType;

    switch (messageType) {
    case Heartbeat: {
        const QString id = readUtf8String(stream);
        quint32 maximumSchemaNumber = 0;
        stream >> maximumSchemaNumber;
        const QString version = readUtf8String(stream);
        const QString revision = readUtf8String(stream);

        qDebug().noquote() << "Heartbeat:"
                           << "id" << id
                           << "maximumSchema" << maximumSchemaNumber
                           << "version" << version
                           << "revision" << revision;
        break;
    }
    case Status: {
        const QString id = readUtf8String(stream);
        quint64 dialFrequency = 0;
        stream >> dialFrequency;
        const QString mode = readUtf8String(stream);
        const QString dxCall = readUtf8String(stream);
        const QString report = readUtf8String(stream);
        const QString txMode = readUtf8String(stream);
        bool txEnabled = false;
        bool transmitting = false;
        bool decoding = false;
        stream >> txEnabled >> transmitting >> decoding;
        quint32 txDf = 0;
        quint32 rxDf = 0;
        stream >> txDf >> rxDf;
        const QString call = readUtf8String(stream);
        const QString grid = readUtf8String(stream);
        const QString dxGrid = readUtf8String(stream);
        bool txWatchdog = false;
        stream >> txWatchdog;
        const QString subMode = readUtf8String(stream);
        bool fastMode = false;
        stream >> fastMode;
        quint8 specialOperationMode = 0;
        quint32 frequencyTolerance = 0;
        quint32 trPeriod = 0;
        stream >> specialOperationMode >> frequencyTolerance >> trPeriod;
        const QString configurationName = readUtf8String(stream);
        const QString txMessage = readUtf8String(stream);

        qDebug().noquote() << "Status:"
                           << "id" << id
                           << "freq" << dialFrequency
                           << "mode" << mode
                           << "dxCall" << dxCall
                           << "report" << report
                           << "txMode" << txMode
                           << "txEnabled" << txEnabled
                           << "transmitting" << transmitting
                           << "decoding" << decoding
                           << "txDf" << txDf
                           << "rxDf" << rxDf
                           << "call" << call
                           << "grid" << grid
                           << "dxGrid" << dxGrid
                           << "txWatchdog" << txWatchdog
                           << "subMode" << subMode
                           << "fastMode" << fastMode
                           << "specialOperationMode" << specialOperationMode
                           << "frequencyTolerance" << frequencyTolerance
                           << "trPeriod" << trPeriod
                           << "configurationName" << configurationName
                           << "txMessage" << txMessage;
        break;
    }
    case Decode: {
        const QString id = readUtf8String(stream);
        bool isNew = false;
        QTime time;
        qint32 snr = 0;
        double deltaTime = 0.0;
        quint32 deltaFrequency = 0;
        stream >> isNew >> time >> snr >> deltaTime >> deltaFrequency;
        const QString mode = readUtf8String(stream);
        const QString message = readUtf8String(stream);

        qDebug().noquote() << "Decode:"
                           << "id" << id
                           << "new" << isNew
                           << "time" << time.toString("HH:mm:ss.zzz")
                           << "snr" << snr
                           << "dt" << deltaTime
                           << "df" << deltaFrequency
                           << "mode" << mode
                           << "message" << message;
        break;
    }
    case Clear: {
        const QString id = readUtf8String(stream);
        qDebug().noquote() << "Clear:" << "id" << id;
        break;
    }
    case QSOLogged: {
        const QString id = readUtf8String(stream);
        QDateTime dateTimeOff;
        stream >> dateTimeOff;
        const QString dxCall = readUtf8String(stream);
        const QString dxGrid = readUtf8String(stream);
        quint64 txFrequency = 0;
        stream >> txFrequency;
        const QString mode = readUtf8String(stream);
        const QString reportSent = readUtf8String(stream);
        const QString reportReceived = readUtf8String(stream);
        const QString txPower = readUtf8String(stream);
        const QString comments = readUtf8String(stream);
        const QString name = readUtf8String(stream);
        QDateTime dateTimeOn;
        stream >> dateTimeOn;
        const QString operatorCall = readUtf8String(stream);
        const QString myCall = readUtf8String(stream);
        const QString myGrid = readUtf8String(stream);
        const QString exchangeSent = readUtf8String(stream);
        const QString exchangeReceived = readUtf8String(stream);
        const QString adifPropagationMode = readUtf8String(stream);

        qDebug().noquote() << "QSOLogged:"
                           << "id" << id
                           << "dateTimeOff" << dateTimeOff.toString(Qt::ISODate)
                           << "dxCall" << dxCall
                           << "dxGrid" << dxGrid
                           << "txFrequency" << txFrequency
                           << "mode" << mode
                           << "reportSent" << reportSent
                           << "reportReceived" << reportReceived
                           << "txPower" << txPower
                           << "comments" << comments
                           << "name" << name
                           << "dateTimeOn" << dateTimeOn.toString(Qt::ISODate)
                           << "operatorCall" << operatorCall
                           << "myCall" << myCall
                           << "myGrid" << myGrid
                           << "exchangeSent" << exchangeSent
                           << "exchangeReceived" << exchangeReceived
                           << "adifPropagationMode" << adifPropagationMode;
        break;
    }
    case Close: {
        const QString id = readUtf8String(stream);
        qDebug().noquote() << "Close:" << "id" << id;
        break;
    }
    case LoggedADIF: {
        const QString id = readUtf8String(stream);
        const QString text = readUtf8String(stream);
        qDebug().noquote() << "LoggedADIF:" << "id" << id << "text" << text;
        break;
    }
    default:
        qDebug().noquote() << "UDP message type" << messageTypeName(messageType)
                           << "is not parsed yet";
        break;
    }
}
