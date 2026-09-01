#include "ActiveSatelliteTracker.h"

#include <QDateTime>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include "Orbit/Sgp4OrbitPropagator.h"

namespace SatelliteTracker {

namespace {
constexpr int kRecomputeIntervalMs = 30 * 1000;
}

ActiveSatelliteTracker::ActiveSatelliteTracker(QObject *parent)
    : QObject(parent)
{
    m_watcher = new QFutureWatcher<QHash<int, PassResult>>(this);
    connect(m_watcher, &QFutureWatcher<QHash<int, PassResult>>::finished,
            this, &ActiveSatelliteTracker::onRecomputeFinished);

    m_recomputeTimer = new QTimer(this);
    m_recomputeTimer->setInterval(kRecomputeIntervalMs);
    connect(m_recomputeTimer, &QTimer::timeout, this, &ActiveSatelliteTracker::requestRecompute);
    m_recomputeTimer->start();
}

void ActiveSatelliteTracker::setActiveSatellites(const QVector<Satellite> &activeSatellites)
{
    m_activeSatellites = activeSatellites;
}

void ActiveSatelliteTracker::setObserverLocation(const ObserverLocation &location)
{
    m_location = location;
}

void ActiveSatelliteTracker::requestRecompute()
{
    if (m_watcher->isRunning()) {
        m_recomputePending = true;
        return;
    }
    startRecompute();
}

void ActiveSatelliteTracker::startRecompute()
{
    if (!m_location.isConfigured || m_activeSatellites.isEmpty()) {
        emit passesUpdated({});
        return;
    }

    const QVector<Satellite> satellites = m_activeSatellites;
    const ObserverLocation location = m_location;
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();

    QFuture<QHash<int, PassResult>> future = QtConcurrent::run([satellites, location, nowUtc]() {
        QHash<int, PassResult> results;
        for (const Satellite &s : satellites) {
            Sgp4OrbitPropagator propagator;
            if (!propagator.loadTle(s.tleLine1, s.tleLine2)) {
                continue; // unparsable TLE; leave absent, displays as "no pass" rather than crashing
            }
            results.insert(s.noradId,
                            findNextPass(propagator, nowUtc,
                                         location.latitudeDeg, location.longitudeDeg, location.altitudeMeters));
        }
        return results;
    });

    m_watcher->setFuture(future);
}

void ActiveSatelliteTracker::onRecomputeFinished()
{
    emit passesUpdated(m_watcher->result());

    if (m_recomputePending) {
        m_recomputePending = false;
        startRecompute();
    }
}

} // namespace SatelliteTracker
