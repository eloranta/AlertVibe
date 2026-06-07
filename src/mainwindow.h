#pragma once

#include <QMainWindow>

class QSqlDatabase;
class QSqlTableModel;
class QTableView;
class QModelIndex;
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
    void clearDecodeRecords();
    void addDecodeRecord(const QString &wsjtId,
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
    void handleTableClicked(const QModelIndex &index);

    QSqlDatabase *database = nullptr;
    QSqlTableModel *decodeModel = nullptr;
    QTableView *decodeTableView = nullptr;
    UdpMessageHandler *udpMessageHandler = nullptr;
    int currentPeriodTimeValue = -1;
};
