#pragma once

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

// Tab 1: a QScrollArea of PassCards, one per active satellite, per
// SatelliteTracker.md §2. Reflows into however many columns fit the current
// width; ticks every card once a second for the live "now" marker.
class PassGridWidget : public QScrollArea {
    Q_OBJECT
public:
    explicit PassGridWidget(QWidget *parent = nullptr);

    // Adds/removes/updates cards to match; existing cards keep their chart
    // state, only their satellite data (TLE/name) refreshes.
    void setActiveSatellites(const QVector<Satellite> &activeSatellites);
    void setObserverLocation(const ObserverLocation &location);

public slots:
    void applyPassResults(const QHash<int, PassResult> &resultsByNoradId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void reflow();

    QHash<int, PassCard *> m_cardsByNoradId;
    QWidget *m_content = nullptr;
    QGridLayout *m_gridLayout = nullptr;
    ObserverLocation m_location;
    QTimer *m_tickTimer = nullptr;
};

} // namespace SatelliteTracker
