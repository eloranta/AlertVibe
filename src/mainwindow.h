#pragma once

#include <QMainWindow>

class QSqlDatabase;
class QSqlTableModel;
class QTableView;
class QTabWidget;
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
    bool isLoggedQso(const QString &band, const QString &callsign) const;
    void setUpDatabase();
    void setUpTableViews();
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
    void addLoggedQsoRecord(const QString &band,
                            quint64 frequency,
                            const QString &mode,
                            const QString &date,
                            const QString &time,
                            const QString &callsign,
                            const QString &sentGrid,
                            const QString &receivedGrid);
    void handleTableClicked(const QModelIndex &index);

    QSqlDatabase *database = nullptr;
    QSqlTableModel *decodeModel = nullptr;
    QSqlTableModel *logModel = nullptr;
    QTableView *decodeTableView = nullptr;
    QTableView *logTableView = nullptr;
    QTabWidget *tabWidget = nullptr;
    UdpMessageHandler *udpMessageHandler = nullptr;
    int currentPeriodTimeValue = -1;
};
