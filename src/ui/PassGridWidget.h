#pragma once

#include <QFutureWatcher>
#include <QHash>
#include <QScrollArea>
#include <QVector>

#include "../core/ObserverLocation.h"
#include "../core/Orbit/PassFinder.h"
#include "../data/Satellite.h"

class QGridLayout;
class QTimer;

namespace SatelliteTracker {

class PassCard;

// Tab 1: a QScrollArea of PassCards, per SatelliteTracker.md §2. Shows
// "what's happening soonest across the whole active watchlist": for every
// active satellite, finds every pass (including one already in progress)
// starting within the next few hours, pools all of those (satellite, pass)
// pairs together, sorts the pool chronologically by AOS, and builds cards
// for only the soonest 12 -- so a satellite with several near-term passes
// can occupy multiple grid slots while another with none that soon gets
// none, rather than every active satellite getting a fixed quota of slots.
//
// Owns its own 30s off-thread recompute cycle (mirrors
// ActiveSatelliteTracker's QTimer + QtConcurrent::run + QFutureWatcher
// pattern) independent of ActiveSatelliteTracker, since gathering several
// passes per satellite is a grid-filling concern specific to this widget --
// Tab 2's Next AOS column only ever needs the single soonest pass, which
// ActiveSatelliteTracker still provides unchanged. Each cycle fully rebuilds
// the PassCard set from scratch rather than diffing, since
// findPassesInWindow() already returns complete, standalone PassResults.
class PassGridWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit PassGridWidget(QWidget *parent = nullptr);

    void setActiveSatellites(const QVector<Satellite> &activeSatellites);
    void setObserverLocation(const ObserverLocation &location);

private:
    void requestRecompute();
    void startRecompute();
    void onRecomputeFinished();
    void rebuildCards(const QHash<int, QVector<PassResult>> &passesByNoradId);
    void reflow();

    QVector<PassCard *> m_cards;
    QVector<Satellite> m_activeSatellites;
    QWidget *m_content = nullptr;
    QGridLayout *m_gridLayout = nullptr;
    ObserverLocation m_location;
    QTimer *m_tickTimer = nullptr;
    QTimer *m_recomputeTimer = nullptr;
    QFutureWatcher<QHash<int, QVector<PassResult>>> *m_watcher = nullptr;
    bool m_recomputePending = false;
};

} // namespace SatelliteTracker
