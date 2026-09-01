#pragma once

#include <QDialog>
#include <QFutureWatcher>
#include <QVector>

#include "../core/ObserverLocation.h"
#include "../core/Orbit/PassFinder.h"
#include "../data/Satellite.h"

class QLabel;
class QTableWidget;

namespace SatelliteTracker {

// Opened by double-clicking a PassCard: a longer-range table of the
// satellite's next several passes, per SatelliteTracker.md Tab 1 spec.
// Computed off the UI thread via QtConcurrent::run, since finding several
// passes for a rarely-visible satellite can scan multiple days.
class PassDetailDialog : public QDialog {
    Q_OBJECT
public:
    PassDetailDialog(const Satellite &satellite, const ObserverLocation &location,
                      QWidget *parent = nullptr);

private:
    void onPassesComputed();

    Satellite m_satellite;
    ObserverLocation m_location;
    QLabel *m_statusLabel = nullptr;
    QTableWidget *m_table = nullptr;
    QFutureWatcher<QVector<PassResult>> *m_watcher = nullptr;
};

} // namespace SatelliteTracker
