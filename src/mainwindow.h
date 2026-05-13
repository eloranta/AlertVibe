#pragma once

#include <QMainWindow>

class QUdpSocket;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void readPendingDatagrams();

private:
    void parseMessage(QByteArray buffer);

    QUdpSocket *udpSocket = nullptr;
};
