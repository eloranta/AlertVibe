#include "mainwindow.h"
#include "udpmessagehandler.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDebug>
#include <QHeaderView>
#include <QModelIndex>
#include <QPainter>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QTime>

namespace {
constexpr auto kConnectionName = "AlertVibeConnection";

bool isCqMessage(const QString &message)
{
    const QString normalized = message.simplified().toUpper();
    return normalized == "CQ" || normalized.startsWith("CQ ");
}

bool containsMyCall(const QString &message)
{
    return message.contains("OG3Z", Qt::CaseInsensitive);
}

class CqHighlightDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem paintedOption(option);
        initStyleOption(&paintedOption, index);
        const QModelIndex messageIndex = index.model()->index(index.row(), 4, index.parent());
        const QString message = messageIndex.data().toString();
        if (!(paintedOption.state & QStyle::State_Selected) && containsMyCall(message)) {
            painter->fillRect(paintedOption.rect, QColor(255, 199, 206));
            paintedOption.backgroundBrush = Qt::NoBrush;
        } else if (isCqMessage(message) && !(paintedOption.state & QStyle::State_Selected)) {
            painter->fillRect(paintedOption.rect, QColor(198, 239, 206));
            paintedOption.backgroundBrush = Qt::NoBrush;
        }

        QStyledItemDelegate::paint(painter, paintedOption, index);
    }
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("AlertVibe");
    resize(900, 600);
    setUpDatabase();
    setUpTableViews();

    udpMessageHandler = new UdpMessageHandler(this);
    connect(udpMessageHandler,
            &UdpMessageHandler::decodeRecordReceived,
            this,
            &MainWindow::addDecodeRecord);
    connect(udpMessageHandler,
            &UdpMessageHandler::qsoLoggedReceived,
            this,
            &MainWindow::addLoggedQsoRecord);
    connect(decodeTableView, &QTableView::clicked, this, &MainWindow::handleTableClicked);
}

MainWindow::~MainWindow()
{
    if (database != nullptr) {
        const QString connectionName = database->connectionName();
        if (database->isOpen()) {
            database->close();
        }
        delete database;
        QSqlDatabase::removeDatabase(connectionName);
    }
}

void MainWindow::setUpDatabase()
{
    database = new QSqlDatabase(QSqlDatabase::addDatabase("QSQLITE", kConnectionName));
    database->setDatabaseName("C:/ZOWN/AlertVibe/alertvibe.db");

    if (!database->open()) {
        qFatal("Failed to open SQLite database: %s", qPrintable(database->lastError().text()));
    }

    QSqlQuery query(*database);
    if (!query.exec("CREATE TABLE IF NOT EXISTS decodes ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "time TEXT NOT NULL,"
                    "band TEXT,"
                    "callsign TEXT,"
                    "grid TEXT,"
                    "message TEXT NOT NULL,"
                    "time_value INTEGER NOT NULL,"
                    "wsjt_id TEXT NOT NULL,"
                    "snr INTEGER NOT NULL,"
                    "delta_time REAL NOT NULL,"
                    "delta_frequency INTEGER NOT NULL,"
                    "mode TEXT NOT NULL,"
                    "low_confidence INTEGER NOT NULL)")) {
        qFatal("Failed to create decodes table: %s", qPrintable(query.lastError().text()));
    }

    if (!query.exec("CREATE TABLE IF NOT EXISTS logged_qsos ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "band TEXT,"
                    "frequency INTEGER NOT NULL,"
                    "mode TEXT NOT NULL,"
                    "date TEXT NOT NULL,"
                    "time TEXT NOT NULL,"
                    "callsign TEXT NOT NULL,"
                    "sent_grid TEXT,"
                    "received_grid TEXT)")) {
        qFatal("Failed to create logged_qsos table: %s", qPrintable(query.lastError().text()));
    }
}

void MainWindow::setUpTableViews()
{
    decodeModel = new QSqlTableModel(this, *database);
    decodeModel->setTable("decodes");
    decodeModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    decodeModel->setSort(decodeModel->fieldIndex("id"), Qt::DescendingOrder);
    decodeModel->select();
    decodeModel->removeColumn(decodeModel->fieldIndex("id"));
    decodeModel->setHeaderData(0, Qt::Horizontal, "Time");
    decodeModel->setHeaderData(1, Qt::Horizontal, "Band");
    decodeModel->setHeaderData(2, Qt::Horizontal, "Call");
    decodeModel->setHeaderData(3, Qt::Horizontal, "Grid");
    decodeModel->setHeaderData(4, Qt::Horizontal, "Message");

    decodeTableView = new QTableView(this);
    decodeTableView->setModel(decodeModel);
    decodeTableView->setItemDelegate(new CqHighlightDelegate(decodeTableView));
    decodeTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    decodeTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    decodeTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    decodeTableView->setAlternatingRowColors(true);
    decodeTableView->setSortingEnabled(false);
    decodeTableView->verticalHeader()->setVisible(false);
    decodeTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    decodeTableView->horizontalHeader()->setStretchLastSection(true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("wsjt_id"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("snr"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("delta_time"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("delta_frequency"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("mode"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("low_confidence"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("time_value"), true);

    logModel = new QSqlTableModel(this, *database);
    logModel->setTable("logged_qsos");
    logModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    logModel->setSort(logModel->fieldIndex("id"), Qt::DescendingOrder);
    logModel->select();
    logModel->removeColumn(logModel->fieldIndex("id"));
    logModel->setHeaderData(0, Qt::Horizontal, "Band");
    logModel->setHeaderData(1, Qt::Horizontal, "Freq");
    logModel->setHeaderData(2, Qt::Horizontal, "Mode");
    logModel->setHeaderData(3, Qt::Horizontal, "Date");
    logModel->setHeaderData(4, Qt::Horizontal, "Time");
    logModel->setHeaderData(5, Qt::Horizontal, "Call");
    logModel->setHeaderData(6, Qt::Horizontal, "Sent Grid");
    logModel->setHeaderData(7, Qt::Horizontal, "Rcvd Grid");

    logTableView = new QTableView(this);
    logTableView->setModel(logModel);
    logTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    logTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    logTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logTableView->setAlternatingRowColors(true);
    logTableView->setSortingEnabled(false);
    logTableView->verticalHeader()->setVisible(false);
    logTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    logTableView->horizontalHeader()->setStretchLastSection(true);

    tabWidget = new QTabWidget(this);
    tabWidget->addTab(decodeTableView, "Decodes");
    tabWidget->addTab(logTableView, "Log");
    setCentralWidget(tabWidget);
}

void MainWindow::clearDecodeRecords()
{
    QSqlQuery query(*database);
    if (!query.exec("DELETE FROM decodes")) {
        qWarning() << "Failed to clear decode records:" << query.lastError().text();
        return;
    }

    currentPeriodTimeValue = -1;
    decodeModel->select();
}

void MainWindow::addLoggedQsoRecord(const QString &band,
                                    quint64 frequency,
                                    const QString &mode,
                                    const QString &date,
                                    const QString &time,
                                    const QString &callsign,
                                    const QString &sentGrid,
                                    const QString &receivedGrid)
{
    QSqlRecord record = logModel->record();
    record.setValue("band", band);
    record.setValue("frequency", qulonglong(frequency));
    record.setValue("mode", mode);
    record.setValue("date", date);
    record.setValue("time", time);
    record.setValue("callsign", callsign);
    record.setValue("sent_grid", sentGrid);
    record.setValue("received_grid", receivedGrid);

    if (!logModel->insertRecord(0, record)) {
        qWarning() << "Failed to insert logged QSO:" << logModel->lastError().text();
        return;
    }

    if (!logModel->submitAll()) {
        qWarning() << "Failed to submit logged QSO:" << logModel->lastError().text();
        logModel->revertAll();
        return;
    }

    logModel->select();
}

void MainWindow::addDecodeRecord(const QString &wsjtId,
                                 const QTime &time,
                                 const QString &band,
                                 const QString &callsign,
                                 const QString &grid,
                                 const QString &message,
                                 qint32 snr,
                                 double deltaTime,
                                 quint32 deltaFrequency,
                                 const QString &mode,
                                 bool lowConfidence)
{
    const int timeValue = time.msecsSinceStartOfDay();
    if (currentPeriodTimeValue != -1 && currentPeriodTimeValue != timeValue) {
        clearDecodeRecords();
    }
    currentPeriodTimeValue = timeValue;

    QSqlRecord record = decodeModel->record();
    record.setValue("time", time.toString("HH:mm:ss"));
    record.setValue("band", band);
    record.setValue("time_value", timeValue);
    record.setValue("callsign", callsign);
    record.setValue("grid", grid);
    record.setValue("message", message);
    record.setValue("wsjt_id", wsjtId);
    record.setValue("snr", snr);
    record.setValue("delta_time", deltaTime);
    record.setValue("delta_frequency", deltaFrequency);
    record.setValue("mode", mode);
    record.setValue("low_confidence", lowConfidence ? 1 : 0);

    if (!decodeModel->insertRecord(0, record)) {
        qWarning() << "Failed to insert decode record:" << decodeModel->lastError().text();
        return;
    }

    if (!decodeModel->submitAll()) {
        qWarning() << "Failed to submit decode record:" << decodeModel->lastError().text();
        decodeModel->revertAll();
        return;
    }

    decodeModel->select();
}

void MainWindow::handleTableClicked(const QModelIndex &index)
{
    if (!index.isValid() || index.column() != 2) {
        return;
    }

    const QSqlRecord record = decodeModel->record(index.row());
    const QString wsjtId = record.value("wsjt_id").toString();
    const QTime time = QTime::fromMSecsSinceStartOfDay(record.value("time_value").toInt());
    const qint32 snr = record.value("snr").toInt();
    const double deltaTime = record.value("delta_time").toDouble();
    const quint32 deltaFrequency = record.value("delta_frequency").toUInt();
    const QString mode = record.value("mode").toString();
    const QString message = record.value("message").toString();
    const bool lowConfidence = record.value("low_confidence").toBool();

    if (!udpMessageHandler->startQso(wsjtId,
                                     time,
                                     snr,
                                     deltaTime,
                                     deltaFrequency,
                                     mode,
                                     message,
                                     lowConfidence)) {
        qWarning() << "Failed to start QSO for" << record.value("callsign").toString();
    }
}
