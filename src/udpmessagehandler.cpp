#include "udpmessagehandler.h"

#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QDebug>
#include <QHostAddress>
#include <QIODevice>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QUdpSocket>

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

struct ParsedDecodeMessage
{
    QString callsign;
    QString grid;
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

bool isGridToken(const QString &token)
{
    static const QRegularExpression gridPattern(
        QStringLiteral("^[A-R]{2}[0-9]{2}(?:[A-X]{2})?$"),
        QRegularExpression::CaseInsensitiveOption);
    return gridPattern.match(token).hasMatch();
}

bool isCallsignToken(const QString &token)
{
    if (token.isEmpty()) {
        return false;
    }

    static const QStringList excludedTokens = {
        QStringLiteral("CQ"),
        QStringLiteral("QRZ"),
        QStringLiteral("DE"),
        QStringLiteral("RRR"),
        QStringLiteral("RR73"),
        QStringLiteral("73"),
    };
    if (excludedTokens.contains(token, Qt::CaseInsensitive)) {
        return false;
    }

    if (!token.contains('/')) {
        const int digitIndex = token.indexOf(QRegularExpression(QStringLiteral("[0-9]")));
        if (digitIndex <= 0 || digitIndex == token.size() - 1) {
            return false;
        }
    }

    static const QRegularExpression callsignPattern(
        QStringLiteral("^(?:<[A-Z0-9/]+>|[A-Z0-9/]+)$"),
        QRegularExpression::CaseInsensitiveOption);
    return callsignPattern.match(token).hasMatch();
}

ParsedDecodeMessage parseDecodedText(const QString &message)
{
    ParsedDecodeMessage parsed;
    const QStringList tokens = message.simplified().split(' ', Qt::SkipEmptyParts);
    QStringList callsigns;

    for (const QString &rawToken : tokens) {
        QString token = rawToken.trimmed();
        token.remove('<');
        token.remove('>');

        if (parsed.grid.isEmpty() && isGridToken(token)) {
            parsed.grid = token.toUpper();
            continue;
        }

        if (isCallsignToken(token)) {
            callsigns.append(token.toUpper());
        }
    }

    if (callsigns.size() >= 2) {
        parsed.callsign = callsigns.at(1);
    } else if (!callsigns.isEmpty()) {
        parsed.callsign = callsigns.first();
    }

    return parsed;
}
}

UdpMessageHandler::UdpMessageHandler(QObject *parent)
    : QObject(parent)
{
    udpSocket = new QUdpSocket(this);
    connect(udpSocket, &QUdpSocket::readyRead, this, &UdpMessageHandler::readPendingDatagrams);

    if (!udpSocket->bind(QHostAddress::AnyIPv4, 2237)) {
        qWarning() << "Failed to bind UDP port 2237:" << udpSocket->errorString();
    } else {
        // qDebug() << "Listening for UDP messages on port 2237";
    }
}

void UdpMessageHandler::readPendingDatagrams()
{
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(qsizetype(udpSocket->pendingDatagramSize()));

        QHostAddress sender;
        quint16 senderPort = 0;
        udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        // qDebug().noquote() << "UDP datagram from"
        //                    << QString("%1:%2").arg(sender.toString()).arg(senderPort)
        //                    << "bytes"
        //                    << datagram.size();
        parseMessage(datagram);
    }
}

void UdpMessageHandler::parseMessage(QByteArray buffer)
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

        Q_UNUSED(id);
        Q_UNUSED(maximumSchemaNumber);
        Q_UNUSED(version);
        Q_UNUSED(revision);
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

        Q_UNUSED(id);
        Q_UNUSED(dialFrequency);
        Q_UNUSED(mode);
        Q_UNUSED(dxCall);
        Q_UNUSED(report);
        Q_UNUSED(txMode);
        Q_UNUSED(txEnabled);
        Q_UNUSED(transmitting);
        Q_UNUSED(decoding);
        Q_UNUSED(txDf);
        Q_UNUSED(rxDf);
        Q_UNUSED(call);
        Q_UNUSED(grid);
        Q_UNUSED(dxGrid);
        Q_UNUSED(txWatchdog);
        Q_UNUSED(subMode);
        Q_UNUSED(fastMode);
        Q_UNUSED(specialOperationMode);
        Q_UNUSED(frequencyTolerance);
        Q_UNUSED(trPeriod);
        Q_UNUSED(configurationName);
        Q_UNUSED(txMessage);
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
        const ParsedDecodeMessage parsed = parseDecodedText(message);

        Q_UNUSED(id);
        Q_UNUSED(isNew);
        Q_UNUSED(snr);
        Q_UNUSED(deltaTime);
        Q_UNUSED(deltaFrequency);
        qDebug().noquote() << time
                           << mode
                           << parsed.callsign
                           << parsed.grid
                           << message;
        break;
    }
    case Clear: {
        const QString id = readUtf8String(stream);
        Q_UNUSED(id);
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

        Q_UNUSED(id);
        Q_UNUSED(dateTimeOff);
        Q_UNUSED(dxCall);
        Q_UNUSED(dxGrid);
        Q_UNUSED(txFrequency);
        Q_UNUSED(mode);
        Q_UNUSED(reportSent);
        Q_UNUSED(reportReceived);
        Q_UNUSED(txPower);
        Q_UNUSED(comments);
        Q_UNUSED(name);
        Q_UNUSED(dateTimeOn);
        Q_UNUSED(operatorCall);
        Q_UNUSED(myCall);
        Q_UNUSED(myGrid);
        Q_UNUSED(exchangeSent);
        Q_UNUSED(exchangeReceived);
        Q_UNUSED(adifPropagationMode);
        break;
    }
    case Close: {
        const QString id = readUtf8String(stream);
        Q_UNUSED(id);
        break;
    }
    case LoggedADIF: {
        const QString id = readUtf8String(stream);
        const QString text = readUtf8String(stream);
        Q_UNUSED(id);
        Q_UNUSED(text);
        break;
    }
    default:
        // qDebug().noquote() << "UDP message type" << messageType
        //                    << "is not parsed yet";
        Q_UNUSED(messageType);
        break;
    }
}
