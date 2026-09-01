#pragma once

#include <QAbstractTableModel>
#include <QHash>
#include <QVector>

#include "../core/Orbit/PassFinder.h"
#include "Satellite.h"

namespace SatelliteTracker {

// Table model backing the Tab 2 catalog view. ColActive is a checkbox the
// user toggles to build the M2 "active satellites" watchlist; ColNextAos
// shows the next-pass time computed for active satellites only (see
// applyPassResults) -- unchecked rows show "—" rather than paying to
// propagate the whole catalog.
class SatelliteModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColActive = 0,
        ColName,
        ColNoradId,
        ColIntlDesignator,
        ColSource,
        ColEpoch,
        ColInclination,
        ColPeriod,
        ColApogee,
        ColPerigee,
        ColNextAos,
        ColumnCount
    };

    explicit SatelliteModel(QObject *parent = nullptr);

    void setSatellites(const QVector<Satellite> &satellites);
    const QVector<Satellite> &satellites() const { return m_satellites; }

    // Updates the next-pass results for active satellites (keyed by NORAD
    // ID, independent of row order) and refreshes just the affected
    // ColNextAos cells -- no full model reset.
    void applyPassResults(const QHash<int, PassResult> &resultsByNoradId);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

signals:
    void activeChanged(int noradId, bool active);

private:
    QVector<Satellite> m_satellites;
    QHash<int, PassResult> m_passResultsByNoradId;
};

} // namespace SatelliteTracker
