#include "MainWindow.h"

#include <QComboBox>
#include <QDateTime>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include "../core/ActiveSatelliteTracker.h"
#include "../core/AppSettings.h"
#include "../core/CelestrakClient.h"
#include "../core/SatelliteNaming.h"
#include "../core/TleParser.h"
#include "../data/SatelliteModel.h"
#include "../data/SatelliteRepository.h"
#include "ObserverLocationDialog.h"
#include "PassGridWidget.h"
#include "PassRadarTab.h"

namespace SatelliteTracker {

namespace {
constexpr qint64 kAutoRefreshIntervalMs = qint64(24) * 60 * 60 * 1000;
constexpr qint64 kRetryIntervalMs = qint64(5) * 60 * 1000;
}

MainWindow::MainWindow(const QString &dbConnectionName, QWidget *parent)
    : QMainWindow(parent)
    , m_dbConnectionName(dbConnectionName)
{
    m_repository = new SatelliteRepository(m_dbConnectionName);
    m_celestrakClient = new CelestrakClient(this);

    m_activeTracker = new ActiveSatelliteTracker(this);
    connect(m_activeTracker, &ActiveSatelliteTracker::passesUpdated,
            this, &MainWindow::onPassesUpdated);
    m_activeTracker->setObserverLocation(AppSettings::loadObserverLocation());

    connect(m_celestrakClient, &CelestrakClient::fetchSucceeded,
            this, &MainWindow::onFetchSucceeded);
    connect(m_celestrakClient, &CelestrakClient::fetchFailed,
            this, &MainWindow::onFetchFailed);

    // Armed only after a failed fetch, to retry sooner than the 24h
    // schedule would otherwise allow; cancelled whenever a fetch starts.
    m_retryTimer = new QTimer(this);
    m_retryTimer->setSingleShot(true);
    connect(m_retryTimer, &QTimer::timeout, this, &MainWindow::onRefreshClicked);

    QMenu *settingsMenu = menuBar()->addMenu(QStringLiteral("&Settings"));
    settingsMenu->addAction(QStringLiteral("Observer Location…"),
                             this, &MainWindow::onObserverLocationTriggered);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(buildPassGridTab(), QStringLiteral("Pass Grid"));
    m_tabs->addTab(buildCatalogTab(), QStringLiteral("Satellite Catalog"));
    setCentralWidget(m_tabs);

    // Only satellite radar tabs (added later, via onRadarTabRequested) are
    // closable -- these two fixed tabs never are.
    m_tabs->setTabsClosable(true);
    m_tabs->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    m_tabs->tabBar()->setTabButton(1, QTabBar::RightSide, nullptr);
    connect(m_tabs, &QTabWidget::tabCloseRequested, this, &MainWindow::onTabCloseRequested);

    connect(m_satelliteModel, &SatelliteModel::activeChanged, this, &MainWindow::onActiveToggled);
    connect(m_passGridWidget, &PassGridWidget::radarTabRequested, this, &MainWindow::onRadarTabRequested);
    m_passGridWidget->setObserverLocation(AppSettings::loadObserverLocation());

    m_radarTickTimer = new QTimer(this);
    m_radarTickTimer->setInterval(1000);
    connect(m_radarTickTimer, &QTimer::timeout, this, [this]() {
        const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
        for (PassRadarTab *tab : std::as_const(m_radarTabsByNoradId)) {
            tab->tick(nowUtc);
        }
    });
    m_radarTickTimer->start();

    setWindowTitle(QStringLiteral("SatelliteTracker"));
    resize(1100, 700);

    // Show cached data immediately, then refresh only if the cache is
    // missing or stale; either way, arm the 24h auto-refresh timer.
    reloadModelFromCache();
    if (!AppSettings::loadObserverLocation().isConfigured) {
        m_statusLabel->setText(
            QStringLiteral("Set observer location to compute Next AOS — Settings → Observer Location…"));
    }
    startAutoRefreshSchedule();
}

QWidget *MainWindow::buildPassGridTab()
{
    m_passGridWidget = new PassGridWidget(this);
    return m_passGridWidget;
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
    m_groupCombo->addItem(QStringLiteral("Amateur Radio"), QStringLiteral("amateur"));
    m_groupCombo->addItem(QStringLiteral("Starlink"), QStringLiteral("starlink"));
    m_groupCombo->addItem(QStringLiteral("Launched last 30 days"), QStringLiteral("last-30-days"));
    m_groupCombo->setCurrentIndex(m_groupCombo->findData(QStringLiteral("amateur")));
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
    m_catalogTable->horizontalHeader()->setSectionResizeMode(SatelliteModel::ColShortName,
                                                              QHeaderView::Stretch);
    m_catalogTable->horizontalHeader()->setSectionResizeMode(SatelliteModel::ColActive,
                                                              QHeaderView::ResizeToContents);
    // Only these columns are shown by default; the rest (full name, NORAD ID,
    // designator, source, epoch, and derived orbital elements) stay in the
    // model -- still searchable, just not displayed -- to keep the table
    // focused on what a ham-satellite user checks at a glance.
    static const QSet<int> kVisibleColumns = {
        SatelliteModel::ColActive, SatelliteModel::ColShortName,
        SatelliteModel::ColMode, SatelliteModel::ColNextAos,
    };
    for (int col = 0; col < SatelliteModel::ColumnCount; ++col) {
        m_catalogTable->setColumnHidden(col, !kVisibleColumns.contains(col));
    }
    m_catalogTable->verticalHeader()->setVisible(false);
    m_catalogTable->sortByColumn(SatelliteModel::ColShortName, Qt::AscendingOrder);
    layout->addWidget(m_catalogTable, 1);

    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_proxyModel->setFilterFixedString(text);
    });
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(m_groupCombo, &QComboBox::currentIndexChanged, this, &MainWindow::reloadModelFromCache);

    return tab;
}

void MainWindow::reloadModelFromCache()
{
    QString error;
    const QString group = m_groupCombo->currentData().toString();
    const QVector<Satellite> cached = m_repository->getAllSatellites(group, &error);
    if (!error.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("Cache read error: %1").arg(error));
        return;
    }

    m_satelliteModel->setSatellites(cached);
    m_rowCountLabel->setText(QStringLiteral("%1 satellites").arg(cached.size()));

    const QDateTime lastUpdated = m_repository->getLastCatalogUpdateUtc();
    if (lastUpdated.isValid()) {
        m_lastUpdatedLabel->setText(
            QStringLiteral("Cache last updated: %1 UTC")
                .arg(lastUpdated.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
    }

    pushActiveSatellitesAndRecompute();
}

void MainWindow::pushActiveSatellitesAndRecompute()
{
    QString error;
    const QVector<Satellite> all = m_repository->getAllSatellites(QString(), &error);
    if (!error.isEmpty()) return;

    QVector<Satellite> active;
    for (const Satellite &s : all) {
        if (s.isActive) active.push_back(s);
    }

    m_activeTracker->setActiveSatellites(active);
    m_activeTracker->requestRecompute();
    m_passGridWidget->setActiveSatellites(active);
}

void MainWindow::startAutoRefreshSchedule()
{
    const QDateTime lastUpdated = m_repository->getLastCatalogUpdateUtc();
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
    const bool isStale = !lastUpdated.isValid() || lastUpdated.secsTo(nowUtc) >= 24 * 60 * 60;

    if (isStale) {
        onRefreshClicked();
    }

    // Recurring timer catches the case where the app is left running for
    // more than 24h straight; it never fires sooner than 24h after launch,
    // and the startup check above already covers the "app was closed and
    // reopened after >24h" case.
    m_autoRefreshTimer = new QTimer(this);
    m_autoRefreshTimer->setInterval(kAutoRefreshIntervalMs);
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &MainWindow::onRefreshClicked);
    m_autoRefreshTimer->start();
}

void MainWindow::setBusy(bool busy)
{
    m_refreshButton->setEnabled(!busy);
    m_groupCombo->setEnabled(!busy);
    m_refreshButton->setText(busy ? QStringLiteral("Refreshing…") : QStringLiteral("Refresh"));
}

void MainWindow::onRefreshClicked()
{
    m_retryTimer->stop();
    setBusy(true);
    m_statusLabel->setText(QStringLiteral("Fetching from Celestrak…"));
    const QString group = m_groupCombo->currentData().toString();
    m_celestrakClient->fetchGroup(group);
}

void MainWindow::onFetchSucceeded(const QString &group, const QByteArray &rawTleText)
{
    // Parsing a few thousand TLE records is cheap but not free — do it off
    // the UI thread so a large group (e.g. "active", ~8000 satellites)
    // never causes a visible stall.
    auto *watcher = new QFutureWatcher<QVector<Satellite>>(this);
    connect(watcher, &QFutureWatcher<QVector<Satellite>>::finished, this, [this, watcher, group]() {
        const QVector<Satellite> parsed = watcher->result();
        watcher->deleteLater();

        QString dbError;
        if (!m_repository->upsertSatellites(parsed, group, &dbError)) {
            setBusy(false);
            m_statusLabel->setText(QStringLiteral("Cache write failed: %1").arg(dbError));
            QMessageBox::warning(this, QStringLiteral("SatelliteTracker"),
                                  QStringLiteral("Failed to save fetched satellites to the "
                                                  "local cache:\n%1").arg(dbError));
            return;
        }

        m_repository->setLastCatalogUpdateUtc(QDateTime::currentDateTimeUtc());

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
    m_statusLabel->setText(
        QStringLiteral("Fetch failed: %1 (showing cached data, retrying in 5 min)")
            .arg(errorMessage));
    m_retryTimer->start(kRetryIntervalMs);
}

void MainWindow::onActiveToggled(int noradId, bool active)
{
    QString error;
    if (!m_repository->setSatelliteActive(noradId, active, &error)) {
        m_statusLabel->setText(QStringLiteral("Failed to save active state: %1").arg(error));
    }
    pushActiveSatellitesAndRecompute();
}

void MainWindow::onPassesUpdated(const QHash<int, PassResult> &resultsByNoradId)
{
    m_satelliteModel->applyPassResults(resultsByNoradId);
}

void MainWindow::onObserverLocationTriggered()
{
    ObserverLocationDialog dialog(AppSettings::loadObserverLocation(), this);
    if (dialog.exec() != QDialog::Accepted) return;

    QString error;
    if (!AppSettings::saveObserverLocation(dialog.result(), &error)) {
        QMessageBox::warning(this, QStringLiteral("SatelliteTracker"),
                              QStringLiteral("Could not save observer location:\n%1").arg(error));
        return;
    }

    const ObserverLocation newLocation = AppSettings::loadObserverLocation();
    m_activeTracker->setObserverLocation(newLocation);
    m_activeTracker->requestRecompute();
    m_passGridWidget->setObserverLocation(newLocation);
    for (PassRadarTab *tab : std::as_const(m_radarTabsByNoradId)) {
        tab->setObserverLocation(newLocation);
    }
}

void MainWindow::onRadarTabRequested(const Satellite &satellite, const PassResult &pass)
{
    auto it = m_radarTabsByNoradId.find(satellite.noradId);
    if (it != m_radarTabsByNoradId.end()) {
        it.value()->setPassResult(satellite, pass);
        m_tabs->setCurrentWidget(it.value());
        return;
    }

    auto *tab = new PassRadarTab(satellite, m_tabs);
    tab->setObserverLocation(AppSettings::loadObserverLocation());
    tab->setPassResult(satellite, pass);
    m_tabs->addTab(tab, SatelliteNaming::shortName(satellite.name));
    m_radarTabsByNoradId.insert(satellite.noradId, tab);
    m_tabs->setCurrentWidget(tab);
}

void MainWindow::onTabCloseRequested(int index)
{
    QWidget *widget = m_tabs->widget(index);
    for (auto it = m_radarTabsByNoradId.begin(); it != m_radarTabsByNoradId.end(); ++it) {
        if (it.value() == widget) {
            m_radarTabsByNoradId.erase(it);
            break;
        }
    }
    m_tabs->removeTab(index);
    widget->deleteLater();
}

} // namespace SatelliteTracker
