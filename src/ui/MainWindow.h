#pragma once

#include <QHash>
#include <QMainWindow>
#include <QString>

#include "../core/Orbit/PassFinder.h"

class QTableView;
class QSortFilterProxyModel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QTabWidget;
class QTimer;

namespace SatelliteTracker {

class ActiveSatelliteTracker;
class CelestrakClient;
class PassGridWidget;
class PassRadarTab;
class SatelliteModel;
class SatelliteRepository;
struct Satellite;

// Top-level window: a QTabWidget with Tab 1 (Pass Grid, M3) and Tab 2
// (Satellite Catalog, M1/M2).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &dbConnectionName, QWidget *parent = nullptr);

private slots:
    void onRefreshClicked();
    void onFetchSucceeded(const QString &group, const QByteArray &rawTleText);
    void onFetchFailed(const QString &group, const QString &errorMessage);
    void onActiveToggled(int noradId, bool active);
    void onPassesUpdated(const QHash<int, PassResult> &resultsByNoradId);
    void onObserverLocationTriggered();
    void onRadarTabRequested(const Satellite &satellite, const PassResult &pass);
    void onTabCloseRequested(int index);

private:
    QWidget *buildPassGridTab();
    QWidget *buildCatalogTab();
    void reloadModelFromCache();
    void setBusy(bool busy);

    // Refreshes now if the catalog has never been updated or is more than
    // 24h stale, then arms the recurring 24h auto-refresh timer.
    void startAutoRefreshSchedule();

    // Active satellites can belong to a different Celestrak group than the
    // one currently selected for display, so this re-queries the repository
    // unfiltered and filters client-side by isActive, rather than reusing
    // reloadModelFromCache()'s group-scoped query.
    void pushActiveSatellitesAndRecompute();

    QString m_dbConnectionName;
    SatelliteRepository *m_repository = nullptr;
    CelestrakClient *m_celestrakClient = nullptr;
    ActiveSatelliteTracker *m_activeTracker = nullptr;
    PassGridWidget *m_passGridWidget = nullptr;
    SatelliteModel *m_satelliteModel = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;
    QTimer *m_autoRefreshTimer = nullptr;
    QTimer *m_retryTimer = nullptr;

    QTabWidget *m_tabs = nullptr;
    QHash<int, PassRadarTab *> m_radarTabsByNoradId;
    QTimer *m_radarTickTimer = nullptr;

    QTableView *m_catalogTable = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_groupCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QLabel *m_lastUpdatedLabel = nullptr;
    QLabel *m_rowCountLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
};

} // namespace SatelliteTracker
