#pragma once

#include <QMainWindow>

class QSqlDatabase;
class QSqlTableModel;
class QTableView;
class QString;
class QTime;
class UdpMessageHandler;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private:
    void setUpDatabase();
    void setUpTableView();
    void addDecodeRecord(const QTime &time,
                         const QString &callsign,
                         const QString &grid,
                         const QString &message);

    QSqlDatabase *database = nullptr;
    QSqlTableModel *decodeModel = nullptr;
    QTableView *decodeTableView = nullptr;
    UdpMessageHandler *udpMessageHandler = nullptr;
};
