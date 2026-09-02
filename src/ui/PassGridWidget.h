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

// Tab 1: a QScrollArea of PassCards, per SatelliteTracker.md §2. Always
// tries to fill a 12-card grid (4 wide): each active satellite gets a share
// of the 12 slots -- its own next several upcoming passes -- split as
// evenly as possible, so e.g. one active satellite fills the grid with its
// next 12 passes while four active satellites get 3 passes (cards) each.
// Once active-satellite count reaches 12, everyone just gets 1 (today's
// single-pass-per-satellite behavior), and the grid grows/scrolls as usual.
//
// Owns its own 30s off-thread recompute cycle (mirrors
// ActiveSatelliteTracker's QTimer + QtConcurrent::run + QFutureWatcher
// pattern) independent of ActiveSatelliteTracker, since "several passes per
// satellite" is a grid-filling concern specific to this widget -- Tab 2's
// Next AOS column only ever needs the single soonest pass, which
// ActiveSatelliteTracker still provides unchanged. Each cycle fully rebuilds
// the PassCard set from scratch rather than diffing, since
// findUpcomingPasses() already returns complete, standalone PassResults.
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
