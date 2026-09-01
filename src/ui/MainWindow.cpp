#include "MainWindow.h"

#include <QComboBox>
#include <QDateTime>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTabWidget>
#include <QTableView>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include "../core/CelestrakClient.h"
#include "../core/TleParser.h"
#include "../data/SatelliteModel.h"
#include "../data/SatelliteRepository.h"

namespace SatelliteTracker {

MainWindow::MainWindow(const QString &dbConnectionName, QWidget *parent)
    : QMainWindow(parent)
    , m_dbConnectionName(dbConnectionName)
{
    m_repository = new SatelliteRepository(m_dbConnectionName);
    m_celestrakClient = new CelestrakClient(this);

    connect(m_celestrakClient, &CelestrakClient::fetchSucceeded,
            this, &MainWindow::onFetchSucceeded);
    connect(m_celestrakClient, &CelestrakClient::fetchFailed,
            this, &MainWindow::onFetchFailed);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildPassGridTabPlaceholder(), QStringLiteral("Pass Grid"));
    tabs->addTab(buildCatalogTab(), QStringLiteral("Satellite Catalog"));
    setCentralWidget(tabs);

    setWindowTitle(QStringLiteral("SatelliteTracker"));
    resize(1100, 700);

    // Show cached data immediately, then kick off a background refresh so
    // the window never opens empty even before the network reply lands.
    reloadModelFromCache();
    onRefreshClicked();
}

QWidget *MainWindow::buildPassGridTabPlaceholder()
{
    auto *placeholder = new QWidget(this);
    auto *layout = new QVBoxLayout(placeholder);
    auto *label = new QLabel(
        QStringLiteral("Pass Grid dashboard arrives in M3.\n\n"
                        "Check satellites active in the Satellite Catalog tab once M2 adds "
                        "the watchlist checkboxes — their next-pass elevation charts will "
                        "appear here."),
        placeholder);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("color: #888; font-size: 14px;"));
    layout->addStretch();
    layout->addWidget(label);
    layout->addStretch();
    return placeholder;
}

QWidget *MainWindow::buildCatalogTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);

    // --- Toolbar row ---
    auto *toolbar = new QHBoxLayout();

    m_searchEdit = new QLineEdit(tab);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search name or NORAD ID..."));
    m_searchEdit->setClearButtonEnabled(true);
    toolbar->addWidget(m_searchEdit, 1);

    m_groupCombo = new QComboBox(tab);
    m_groupCombo->addItem(QStringLiteral("Active satellites"), QStringLiteral("active"));
    m_groupCombo->addItem(QStringLiteral("Space stations"), QStringLiteral("stations"));
    m_groupCombo->addItem(QStringLiteral("Visible (brightest)"), QStringLiteral("visual"));
    m_groupCombo->addItem(QStringLiteral("Weather"), QStringLiteral("weather"));
    m_groupCombo->addItem(QStringLiteral("Starlink"), QStringLiteral("starlink"));
    m_groupCombo->addItem(QStringLiteral("Launched last 30 days"), QStringLiteral("last-30-days"));
    toolbar->addWidget(m_groupCombo);

    m_refreshButton = new QPushButton(QStringLiteral("Refresh"), tab);
    toolbar->addWidget(m_refreshButton);

    layout->addLayout(toolbar);

    // --- Status row ---
    auto *statusRow = new QHBoxLayout();
    m_lastUpdatedLabel = new QLabel(QStringLiteral("Never updated"), tab);
    m_lastUpdatedLabel->setStyleSheet(QStringLiteral("color: #888;"));
    m_statusLabel = new QLabel(tab);
    m_statusLabel->setStyleSheet(QStringLiteral("color: #888;"));
    m_rowCountLabel = new QLabel(tab);
    m_rowCountLabel->setStyleSheet(QStringLiteral("color: #888;"));
    statusRow->addWidget(m_lastUpdatedLabel);
    statusRow->addWidget(m_statusLabel, 1);
    statusRow->addWidget(m_rowCountLabel);
    layout->addLayout(statusRow);

    // --- Table ---
    m_satelliteModel = new SatelliteModel(this);
    m_proxyModel = new QSortFilterProxyModel(this);
    m_proxyModel->setSourceModel(m_satelliteModel);
    m_proxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxyModel->setFilterKeyColumn(-1); // search across all columns

    m_catalogTable = new QTableView(tab);
    m_catalogTable->setModel(m_proxyModel);
    m_catalogTable->setSortingEnabled(true);
    m_catalogTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_catalogTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_catalogTable->horizontalHeader()->setStretchLastSection(true);
    m_catalogTable->horizontalHeader()->setSectionResizeMode(SatelliteModel::ColName,
                                                              QHeaderView::Stretch);
    m_catalogTable->verticalHeader()->setVisible(false);
    layout->addWidget(m_catalogTable, 1);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxyModel->setFilterFixedString(text);
    });
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);

    return tab;
}

void MainWindow::reloadModelFromCache()
{
    QString error;
    const QVector<Satellite> cached = m_repository->getAllSatellites(&error);
    if (!error.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Cache read error: %1").arg(error));
        return;
    }

    m_satelliteModel->setSatellites(cached);
    m_rowCountLabel->setText(QStringLiteral("%1 satellites").arg(cached.size()));

    if (!cached.isEmpty()) {
        QDateTime newest;
        for (const Satellite &s : cached) {
            if (!newest.isValid() || s.lastUpdatedUtc > newest) newest = s.lastUpdatedUtc;
        }
        if (newest.isValid()) {
            m_lastUpdatedLabel->setText(
                QStringLiteral("Cache last updated: %1 UTC")
                    .arg(newest.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
        }
    }
}

void MainWindow::setBusy(bool busy)
{
    m_refreshButton->setEnabled(!busy);
    m_groupCombo->setEnabled(!busy);
    m_refreshButton->setText(busy ? QStringLiteral("Refreshing…") : QStringLiteral("Refresh"));
}

void MainWindow::onRefreshClicked()
{
    setBusy(true);
    m_statusLabel->setText(QStringLiteral("Fetching from Celestrak…"));
    const QString group = m_groupCombo->currentData().toString();
    m_celestrakClient->fetchGroup(group);
}

void MainWindow::onFetchSucceeded(const QString &group, const QByteArray &rawTleText)
{
    Q_UNUSED(group);

    // Parsing a few thousand TLE records is cheap but not free — do it off
    // the UI thread so a large group (e.g. "active", ~8000 satellites)
    // never causes a visible stall.
    auto *watcher = new QFutureWatcher<QVector<Satellite>>(this);
    connect(watcher, &QFutureWatcher<QVector<Satellite>>::finished, this, [this, watcher]() {
        const QVector<Satellite> parsed = watcher->result();
        watcher->deleteLater();

        QString dbError;
        if (!m_repository->upsertSatellites(parsed, &dbError)) {
            setBusy(false);
            m_statusLabel->setText(QStringLiteral("Cache write failed: %1").arg(dbError));
            QMessageBox::warning(this, QStringLiteral("SatelliteTracker"),
                                  QStringLiteral("Failed to save fetched satellites to the "
                                                  "local cache:\n%1").arg(dbError));
            return;
        }

        reloadModelFromCache();
        setBusy(false);
        m_statusLabel->setText(QStringLiteral("Fetched %1 satellites").arg(parsed.size()));
    });

    const QString text = QString::fromUtf8(rawTleText);
    QFuture<QVector<Satellite>> future = QtConcurrent::run([text]() {
        return TleParser::parseThreeLineElementSet(text, QStringLiteral("celestrak"));
    });
    watcher->setFuture(future);
}

void MainWindow::onFetchFailed(const QString &group, const QString &errorMessage)
{
    Q_UNUSED(group);
    setBusy(false);
    m_statusLabel->setText(QStringLiteral("Fetch failed: %1 (showing cached data)").arg(errorMessage));
}

} // namespace SatelliteTracker
