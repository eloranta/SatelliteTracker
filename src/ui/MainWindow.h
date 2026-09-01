#pragma once

#include <QMainWindow>
#include <QString>

class QTableView;
class QSortFilterProxyModel;
class QLineEdit;
class QComboBox;
class QPushButton;
class QLabel;

namespace SatelliteTracker {

class CelestrakClient;
class SatelliteModel;
class SatelliteRepository;

// Top-level window: a QTabWidget with Tab 1 (Pass Grid — placeholder until
// M3) and Tab 2 (Satellite Catalog — implemented in M1).
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString &dbConnectionName, QWidget *parent = nullptr);

private slots:
    void onRefreshClicked();
    void onFetchSucceeded(const QString &group, const QByteArray &rawTleText);
    void onFetchFailed(const QString &group, const QString &errorMessage);

private:
    QWidget *buildPassGridTabPlaceholder();
    QWidget *buildCatalogTab();
    void reloadModelFromCache();
    void setBusy(bool busy);

    QString m_dbConnectionName;
    SatelliteRepository *m_repository = nullptr;
    CelestrakClient *m_celestrakClient = nullptr;
    SatelliteModel *m_satelliteModel = nullptr;
    QSortFilterProxyModel *m_proxyModel = nullptr;

    QTableView *m_catalogTable = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QComboBox *m_groupCombo = nullptr;
    QPushButton *m_refreshButton = nullptr;
    QLabel *m_lastUpdatedLabel = nullptr;
    QLabel *m_rowCountLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
};

} // namespace SatelliteTracker
