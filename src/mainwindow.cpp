#include "mainwindow.h"
#include "udpmessagehandler.h"

#include <QAbstractItemView>
#include <QDebug>
#include <QHeaderView>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QTableView>
#include <QTime>

namespace {
constexpr auto kConnectionName = "AlertVibeConnection";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("AlertVibe");
    resize(900, 600);
    setUpDatabase();
    setUpTableView();

    udpMessageHandler = new UdpMessageHandler(this);
    connect(udpMessageHandler,
            &UdpMessageHandler::decodeRecordReceived,
            this,
            &MainWindow::addDecodeRecord);
}

MainWindow::~MainWindow()
{
    delete decodeModel;
    delete decodeTableView;

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
    database->setDatabaseName(":memory:");

    if (!database->open()) {
        qFatal("Failed to open SQLite database: %s", qPrintable(database->lastError().text()));
    }

    QSqlQuery query(*database);
    if (!query.exec("CREATE TABLE decodes ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                    "time TEXT NOT NULL,"
                    "callsign TEXT,"
                    "grid TEXT,"
                    "message TEXT NOT NULL)")) {
        qFatal("Failed to create decodes table: %s", qPrintable(query.lastError().text()));
    }
}

void MainWindow::setUpTableView()
{
    decodeModel = new QSqlTableModel(this, *database);
    decodeModel->setTable("decodes");
    decodeModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    decodeModel->setSort(decodeModel->fieldIndex("id"), Qt::DescendingOrder);
    decodeModel->select();
    decodeModel->removeColumn(decodeModel->fieldIndex("id"));
    decodeModel->setHeaderData(0, Qt::Horizontal, "Time");
    decodeModel->setHeaderData(1, Qt::Horizontal, "Call");
    decodeModel->setHeaderData(2, Qt::Horizontal, "Grid");
    decodeModel->setHeaderData(3, Qt::Horizontal, "Message");

    decodeTableView = new QTableView(this);
    decodeTableView->setModel(decodeModel);
    decodeTableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    decodeTableView->setSelectionMode(QAbstractItemView::SingleSelection);
    decodeTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    decodeTableView->setAlternatingRowColors(true);
    decodeTableView->setSortingEnabled(false);
    decodeTableView->verticalHeader()->setVisible(false);
    decodeTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    decodeTableView->horizontalHeader()->setStretchLastSection(true);
    setCentralWidget(decodeTableView);
}

void MainWindow::addDecodeRecord(const QTime &time,
                                 const QString &callsign,
                                 const QString &grid,
                                 const QString &message)
{
    QSqlRecord record = decodeModel->record();
    record.setValue("time", time.toString("HH:mm:ss"));
    record.setValue("callsign", callsign);
    record.setValue("grid", grid);
    record.setValue("message", message);

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
