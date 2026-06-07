#include "mainwindow.h"
#include "udpmessagehandler.h"

#include <QAbstractItemView>
#include <QColor>
#include <QDebug>
#include <QHeaderView>
#include <QModelIndex>
#include <QPainter>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QSortFilterProxyModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTabWidget>
#include <QTableView>
#include <QTime>
#include <QtMath>

namespace {
constexpr auto kConnectionName = "AlertVibeConnection";
constexpr auto kMyGrid = "KP11";
constexpr double kEarthRadiusKm = 6371.0;

bool ensureColumnExists(QSqlDatabase &database,
                        const QString &tableName,
                        const QString &columnName,
                        const QString &columnDefinition)
{
    QSqlQuery query(database);
    if (!query.exec(QString("PRAGMA table_info(%1)").arg(tableName))) {
        qWarning() << "Failed to inspect table" << tableName << ":" << query.lastError().text();
        return false;
    }

    while (query.next()) {
        if (query.value("name").toString().compare(columnName, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    QSqlQuery alterQuery(database);
    if (!alterQuery.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3")
                             .arg(tableName, columnName, columnDefinition))) {
        qWarning() << "Failed to add column" << columnName << "to" << tableName << ":"
                   << alterQuery.lastError().text();
        return false;
    }

    return true;
}

bool maidenheadToLatLon(const QString &grid, double &latitude, double &longitude)
{
    const QString normalized = grid.trimmed().toUpper();
    static const QRegularExpression gridPattern("^[A-R]{2}[0-9]{2}(?:[A-X]{2})?$");
    if (!gridPattern.match(normalized).hasMatch()) {
        return false;
    }

    longitude = (normalized[0].unicode() - 'A') * 20.0 - 180.0;
    latitude = (normalized[1].unicode() - 'A') * 10.0 - 90.0;
    longitude += (normalized[2].unicode() - '0') * 2.0;
    latitude += (normalized[3].unicode() - '0') * 1.0;

    double lonWidth = 2.0;
    double latHeight = 1.0;
    if (normalized.size() >= 6) {
        longitude += (normalized[4].unicode() - 'A') * (5.0 / 60.0);
        latitude += (normalized[5].unicode() - 'A') * (2.5 / 60.0);
        lonWidth = 5.0 / 60.0;
        latHeight = 2.5 / 60.0;
    }

    longitude += lonWidth / 2.0;
    latitude += latHeight / 2.0;
    return true;
}

int distanceKmBetweenGrids(const QString &fromGrid, const QString &toGrid)
{
    double fromLat = 0.0;
    double fromLon = 0.0;
    double toLat = 0.0;
    double toLon = 0.0;
    if (!maidenheadToLatLon(fromGrid, fromLat, fromLon)
        || !maidenheadToLatLon(toGrid, toLat, toLon)) {
        return -1;
    }

    const double fromLatRad = qDegreesToRadians(fromLat);
    const double fromLonRad = qDegreesToRadians(fromLon);
    const double toLatRad = qDegreesToRadians(toLat);
    const double toLonRad = qDegreesToRadians(toLon);
    const double dLat = toLatRad - fromLatRad;
    const double dLon = toLonRad - fromLonRad;
    const double a = qPow(qSin(dLat / 2.0), 2)
        + qCos(fromLatRad) * qCos(toLatRad) * qPow(qSin(dLon / 2.0), 2);
    const double c = 2.0 * qAtan2(qSqrt(a), qSqrt(1.0 - a));
    return qRound(kEarthRadiusKm * c);
}

bool isCqMessage(const QString &message)
{
    const QString normalized = message.simplified().toUpper();
    return normalized == "CQ" || normalized.startsWith("CQ ");
}

bool containsMyCall(const QString &message)
{
    return message.contains("OG3Z", Qt::CaseInsensitive);
}

bool isLoggedQsoInModel(QSqlTableModel *logModel, const QString &band, const QString &callsign)
{
    if (logModel == nullptr || band.isEmpty() || callsign.isEmpty()) {
        return false;
    }

    for (int row = 0; row < logModel->rowCount(); ++row) {
        const QSqlRecord record = logModel->record(row);
        if (record.value("band").toString().compare(band, Qt::CaseInsensitive) == 0
            && record.value("callsign").toString().compare(callsign, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }

    return false;
}

class DecodeRowDelegate final : public QStyledItemDelegate
{
public:
    explicit DecodeRowDelegate(QSqlTableModel *logModel, QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
        , m_logModel(logModel)
    {
    }

    void paint(QPainter *painter,
               const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        QStyleOptionViewItem paintedOption(option);
        initStyleOption(&paintedOption, index);
        const auto *proxyModel = qobject_cast<const QSortFilterProxyModel *>(index.model());
        const auto *sqlModel = proxyModel != nullptr
            ? qobject_cast<const QSqlTableModel *>(proxyModel->sourceModel())
            : qobject_cast<const QSqlTableModel *>(index.model());
        const QModelIndex sourceIndex = proxyModel != nullptr ? proxyModel->mapToSource(index) : index;

        QString band;
        QString callsign;
        QString message;
        if (sqlModel != nullptr) {
            const QSqlRecord record = sqlModel->record(sourceIndex.row());
            band = record.value("band").toString();
            callsign = record.value("callsign").toString();
            message = record.value("message").toString();
        }

        if (!(paintedOption.state & QStyle::State_Selected) && isLoggedQsoInModel(m_logModel, band, callsign)) {
            painter->fillRect(paintedOption.rect, QColor(217, 217, 217));
            paintedOption.backgroundBrush = Qt::NoBrush;
            paintedOption.palette.setColor(QPalette::Text, QColor(96, 96, 96));
        } else if (!(paintedOption.state & QStyle::State_Selected) && containsMyCall(message)) {
            painter->fillRect(paintedOption.rect, QColor(255, 199, 206));
            paintedOption.backgroundBrush = Qt::NoBrush;
        } else if (isCqMessage(message) && !(paintedOption.state & QStyle::State_Selected)) {
            painter->fillRect(paintedOption.rect, QColor(198, 239, 206));
            paintedOption.backgroundBrush = Qt::NoBrush;
        }

        QStyledItemDelegate::paint(painter, paintedOption, index);
    }

private:
    QSqlTableModel *m_logModel = nullptr;
};

class DecodeSortProxyModel final : public QSortFilterProxyModel
{
public:
    explicit DecodeSortProxyModel(QSqlTableModel *logModel, QObject *parent = nullptr)
        : QSortFilterProxyModel(parent)
        , m_logModel(logModel)
    {
    }

protected:
    bool lessThan(const QModelIndex &sourceLeft, const QModelIndex &sourceRight) const override
    {
        const auto *sqlModel = qobject_cast<QSqlTableModel *>(sourceModel());
        if (sqlModel == nullptr) {
            return sourceLeft.row() < sourceRight.row();
        }

        const QSqlRecord leftRecord = sqlModel->record(sourceLeft.row());
        const QSqlRecord rightRecord = sqlModel->record(sourceRight.row());
        const int leftPriority = rowPriority(leftRecord);
        const int rightPriority = rowPriority(rightRecord);
        if (leftPriority != rightPriority) {
            return leftPriority < rightPriority;
        }

        const int leftDistance = leftRecord.value("distance_km").toInt();
        const int rightDistance = rightRecord.value("distance_km").toInt();
        if (leftDistance != rightDistance) {
            return leftDistance > rightDistance;
        }

        return sourceLeft.row() < sourceRight.row();
    }

private:
    int rowPriority(const QSqlRecord &record) const
    {
        const QString band = record.value("band").toString();
        const QString callsign = record.value("callsign").toString();
        const QString message = record.value("message").toString();

        if (isLoggedQsoInModel(m_logModel, band, callsign)) {
            return 3;
        }
        if (containsMyCall(message)) {
            return 0;
        }
        if (isCqMessage(message)) {
            return 1;
        }

        return 2;
    }

    QSqlTableModel *m_logModel = nullptr;
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
                    "distance_km INTEGER,"
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

    if (!ensureColumnExists(*database, "decodes", "distance_km", "INTEGER")) {
        qFatal("Failed to ensure distance_km column exists");
    }
}

void MainWindow::setUpTableViews()
{
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

    decodeModel = new QSqlTableModel(this, *database);
    decodeModel->setTable("decodes");
    decodeModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    decodeModel->setSort(decodeModel->fieldIndex("id"), Qt::DescendingOrder);
    decodeModel->select();
    decodeModel->removeColumn(decodeModel->fieldIndex("id"));
    decodeModel->setHeaderData(decodeModel->fieldIndex("time"), Qt::Horizontal, "Time");
    decodeModel->setHeaderData(decodeModel->fieldIndex("band"), Qt::Horizontal, "Band");
    decodeModel->setHeaderData(decodeModel->fieldIndex("callsign"), Qt::Horizontal, "Call");
    decodeModel->setHeaderData(decodeModel->fieldIndex("grid"), Qt::Horizontal, "Grid");
    decodeModel->setHeaderData(decodeModel->fieldIndex("distance_km"), Qt::Horizontal, "Distance");
    decodeModel->setHeaderData(decodeModel->fieldIndex("message"), Qt::Horizontal, "Message");

    decodeProxyModel = new DecodeSortProxyModel(logModel, this);
    decodeProxyModel->setSourceModel(decodeModel);
    decodeProxyModel->setDynamicSortFilter(true);
    decodeProxyModel->sort(0);

    decodeTableView = new QTableView(this);
    decodeTableView->setModel(decodeProxyModel);
    decodeTableView->setItemDelegate(new DecodeRowDelegate(logModel, decodeTableView));
    decodeTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    decodeTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    decodeTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    decodeTableView->setSortingEnabled(false);
    decodeTableView->verticalHeader()->setVisible(false);
    decodeTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    decodeTableView->horizontalHeader()->setStretchLastSection(true);
    decodeTableView->horizontalHeader()->setSectionsMovable(true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("wsjt_id"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("snr"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("delta_time"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("delta_frequency"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("mode"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("low_confidence"), true);
    decodeTableView->setColumnHidden(decodeModel->fieldIndex("time_value"), true);
    decodeTableView->horizontalHeader()->moveSection(
        decodeTableView->horizontalHeader()->visualIndex(decodeModel->fieldIndex("distance_km")),
        decodeTableView->horizontalHeader()->visualIndex(decodeModel->fieldIndex("message")));

    logTableView = new QTableView(this);
    logTableView->setModel(logModel);
    logTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    logTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    logTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    logTableView->setSortingEnabled(false);
    logTableView->verticalHeader()->setVisible(false);
    logTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    logTableView->horizontalHeader()->setStretchLastSection(true);

    tabWidget = new QTabWidget(this);
    tabWidget->addTab(decodeTableView, "Decodes");
    tabWidget->addTab(logTableView, "Log");
    setCentralWidget(tabWidget);
}

bool MainWindow::isLoggedQso(const QString &band, const QString &callsign) const
{
    return isLoggedQsoInModel(logModel, band, callsign);
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
    decodeTableView->viewport()->update();
    decodeProxyModel->sort(0);
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
    const int distanceKm = distanceKmBetweenGrids(kMyGrid, grid);
    record.setValue("time", time.toString("HH:mm:ss"));
    record.setValue("band", band);
    record.setValue("time_value", timeValue);
    record.setValue("callsign", callsign);
    record.setValue("grid", grid);
    if (distanceKm >= 0) {
        record.setValue("distance_km", distanceKm);
    } else {
        record.setNull("distance_km");
    }
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
    decodeProxyModel->sort(0);
}

void MainWindow::handleTableClicked(const QModelIndex &index)
{
    if (!index.isValid() || index.column() != 2) {
        return;
    }

    const QModelIndex sourceIndex = decodeProxyModel->mapToSource(index);
    const QSqlRecord record = decodeModel->record(sourceIndex.row());
    const QString band = record.value("band").toString();
    const QString callsign = record.value("callsign").toString();
    if (isLoggedQso(band, callsign)) {
        qWarning() << "QSO already logged for" << callsign << "on" << band;
        return;
    }

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
        qWarning() << "Failed to start QSO for" << callsign;
    }
}
