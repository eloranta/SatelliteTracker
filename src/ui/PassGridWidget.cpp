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
constexpr int kLookaheadHours = 6;
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
    const QDateTime windowEnd = nowUtc.addSecs(qint64(kLookaheadHours) * 3600);

    QFuture<QHash<int, QVector<PassResult>>> future = QtConcurrent::run([satellites, location, nowUtc, windowEnd]() {
        QHash<int, QVector<PassResult>> results;
        for (const Satellite &s : satellites) {
            Sgp4OrbitPropagator propagator;
            if (!propagator.loadTle(s.tleLine1, s.tleLine2)) {
                continue; // unparsable TLE; leave absent
            }
            results.insert(s.noradId,
                            findPassesInWindow(propagator, nowUtc, windowEnd,
                                               location.latitudeDeg, location.longitudeDeg, location.altitudeMeters));
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

    // Flatten every (satellite, pass) found across the whole active
    // watchlist into one list, sort it chronologically by AOS, and only
    // build cards for the soonest kGridCapacity -- so which satellites get
    // shown is purely "what's happening soonest across everything active",
    // not a per-satellite fair-share quota.
    struct Entry { const Satellite *satellite; PassResult pass; };
    QVector<Entry> entries;
    for (const Satellite &s : std::as_const(m_activeSatellites)) {
        for (const PassResult &pass : passesByNoradId.value(s.noradId)) {
            entries.append({&s, pass});
        }
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.pass.aosUtc < b.pass.aosUtc;
    });

    const int count = qMin(entries.size(), kGridCapacity);
    for (int i = 0; i < count; ++i) {
        const Entry &entry = entries.at(i);
        auto *card = new PassCard(*entry.satellite, m_content);
        card->setObserverLocation(m_location);
        card->setPassResult(entry.pass);
        connect(card, &PassCard::visibilityMaybeChanged, this, &PassGridWidget::reflow);
        m_cards.append(card);
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
