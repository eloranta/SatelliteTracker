#include "PassGridWidget.h"

#include <algorithm>

#include <QDateTime>
#include <QGridLayout>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

#include "../core/Orbit/Sgp4OrbitPropagator.h"
#include "PassCard.h"

namespace SatelliteTracker {

namespace {
constexpr int kColumns = 4;
constexpr int kGridCapacity = 12;
constexpr int kTickIntervalMs = 1000;
constexpr int kRecomputeIntervalMs = 30 * 1000;
constexpr int kFindUpcomingLookaheadHoursPerCall = 72;

// Splits kGridCapacity slots across `satellites` as evenly as possible --
// extra slots (the remainder) go to the satellites sorted first by NORAD
// ID, for a stable, deterministic split. Once there are at least as many
// satellites as capacity, everyone just gets 1 and the grid grows past
// capacity (today's original behavior).
QHash<int, int> passCountsFor(const QVector<Satellite> &satellites)
{
    QHash<int, int> counts;
    const int n = satellites.size();
    if (n == 0) {
        return counts;
    }

    QVector<int> noradIds;
    noradIds.reserve(n);
    for (const Satellite &s : satellites) {
        noradIds.append(s.noradId);
    }
    std::sort(noradIds.begin(), noradIds.end());

    if (n >= kGridCapacity) {
        for (int id : noradIds) counts.insert(id, 1);
        return counts;
    }

    const int base = kGridCapacity / n;
    const int remainder = kGridCapacity % n;
    for (int i = 0; i < n; ++i) {
        counts.insert(noradIds.at(i), base + (i < remainder ? 1 : 0));
    }
    return counts;
}
}

PassGridWidget::PassGridWidget(QWidget *parent)
    : QScrollArea(parent)
{
    setWidgetResizable(true);

    m_content = new QWidget(this);
    m_gridLayout = new QGridLayout(m_content);
    setWidget(m_content);

    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(kTickIntervalMs);
    connect(m_tickTimer, &QTimer::timeout, this, [this]() {
        const QDateTime nowUtc = QDateTime::currentDateTimeUtc();
        for (PassCard *card : std::as_const(m_cards)) {
            card->tick(nowUtc);
        }
    });
    m_tickTimer->start();

    m_watcher = new QFutureWatcher<QHash<int, QVector<PassResult>>>(this);
    connect(m_watcher, &QFutureWatcher<QHash<int, QVector<PassResult>>>::finished,
            this, &PassGridWidget::onRecomputeFinished);

    m_recomputeTimer = new QTimer(this);
    m_recomputeTimer->setInterval(kRecomputeIntervalMs);
    connect(m_recomputeTimer, &QTimer::timeout, this, &PassGridWidget::requestRecompute);
    m_recomputeTimer->start();
}

void PassGridWidget::setActiveSatellites(const QVector<Satellite> &activeSatellites)
{
    m_activeSatellites = activeSatellites;
    requestRecompute();
}

void PassGridWidget::setObserverLocation(const ObserverLocation &location)
{
    m_location = location;
    requestRecompute();
}

void PassGridWidget::requestRecompute()
{
    if (m_watcher->isRunning()) {
        m_recomputePending = true;
        return;
    }
    startRecompute();
}

void PassGridWidget::startRecompute()
{
    if (!m_location.isConfigured || m_activeSatellites.isEmpty()) {
        rebuildCards({});
        return;
    }

    const QVector<Satellite> satellites = m_activeSatellites;
    const ObserverLocation location = m_location;
    const QDateTime nowUtc = QDateTime::currentDateTimeUtc();

    QFuture<QHash<int, QVector<PassResult>>> future = QtConcurrent::run([satellites, location, nowUtc]() {
        const QHash<int, int> counts = passCountsFor(satellites);
        QHash<int, QVector<PassResult>> results;
        for (const Satellite &s : satellites) {
            Sgp4OrbitPropagator propagator;
            if (!propagator.loadTle(s.tleLine1, s.tleLine2)) {
                continue; // unparsable TLE; leave absent
            }
            results.insert(s.noradId,
                            findUpcomingPasses(propagator, nowUtc,
                                               location.latitudeDeg, location.longitudeDeg, location.altitudeMeters,
                                               counts.value(s.noradId, 1),
                                               kFindUpcomingLookaheadHoursPerCall));
        }
        return results;
    });

    m_watcher->setFuture(future);
}

void PassGridWidget::onRecomputeFinished()
{
    rebuildCards(m_watcher->result());

    if (m_recomputePending) {
        m_recomputePending = false;
        startRecompute();
    }
}

void PassGridWidget::rebuildCards(const QHash<int, QVector<PassResult>> &passesByNoradId)
{
    qDeleteAll(m_cards);
    m_cards.clear();

    for (const Satellite &s : std::as_const(m_activeSatellites)) {
        const QVector<PassResult> passes = passesByNoradId.value(s.noradId);
        for (const PassResult &pass : passes) {
            auto *card = new PassCard(s, m_content);
            card->setObserverLocation(m_location);
            card->setPassResult(pass);
            connect(card, &PassCard::visibilityMaybeChanged, this, &PassGridWidget::reflow);
            m_cards.append(card);
        }
    }

    reflow();
}

void PassGridWidget::reflow()
{
    // Chronological by AOS (soonest first), left to right then top to
    // bottom; NORAD ID only breaks ties for a stable, deterministic order.
    QVector<PassCard *> ordered = m_cards;
    std::sort(ordered.begin(), ordered.end(), [](PassCard *a, PassCard *b) {
        const QDateTime aosA = a->aosSortKey();
        const QDateTime aosB = b->aosSortKey();
        if (aosA != aosB) return aosA < aosB;
        return a->noradId() < b->noradId();
    });

    // Cards past their own LOS hide themselves (see PassCard::tick) and are
    // excluded here -- removeWidget so a hidden card doesn't keep reserving
    // its old grid cell -- until the next recompute cycle replaces them.
    int visibleIndex = 0;
    for (PassCard *card : std::as_const(ordered)) {
        if (card->isHidden()) {
            m_gridLayout->removeWidget(card);
            continue;
        }
        m_gridLayout->addWidget(card, visibleIndex / kColumns, visibleIndex % kColumns);
        ++visibleIndex;
    }
}

} // namespace SatelliteTracker
