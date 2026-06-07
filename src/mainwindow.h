#pragma once

#include <QMainWindow>

class UdpMessageHandler;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    UdpMessageHandler *udpMessageHandler = nullptr;
};
