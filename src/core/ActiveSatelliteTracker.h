#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QObject>
#include <QVector>

#include "ObserverLocation.h"
#include "Orbit/PassFinder.h"
#include "../data/Satellite.h"

class QTimer;

namespace SatelliteTracker {

// Periodically (every 30s) recomputes the next pass for every satellite on
// the M2 "active" watchlist, off the UI thread -- the same QtConcurrent::run
// + QFutureWatcher pattern MainWindow::onFetchSucceeded already uses for TLE
// parsing. Recompute requests that arrive while one is already running are
// coalesced into a single follow-up run instead of overlapping.
class ActiveSatelliteTracker : public QObject {
    Q_OBJECT
public:
    explicit ActiveSatelliteTracker(QObject *parent = nullptr);

    void setActiveSatellites(const QVector<Satellite> &activeSatellites);
    void setObserverLocation(const ObserverLocation &location);

    void requestRecompute();

signals:
    void passesUpdated(const QHash<int, PassResult> &resultsByNoradId);

private:
    void startRecompute();
    void onRecomputeFinished();

    QVector<Satellite> m_activeSatellites;
    ObserverLocation m_location;
    QTimer *m_recomputeTimer = nullptr;
    QFutureWatcher<QHash<int, PassResult>> *m_watcher = nullptr;
    bool m_recomputePending = false;
};

} // namespace SatelliteTracker
