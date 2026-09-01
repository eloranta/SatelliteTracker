#pragma once

#include <QAbstractTableModel>
#include <QVector>

#include "Satellite.h"

namespace SatelliteTracker {

// Read-only table model backing the Tab 2 catalog view. No checkbox column
// yet (that arrives in M2, once Tab 1 needs an "active" watchlist to read
// from); this is a straightforward flat display of cached satellite data.
class SatelliteModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column {
        ColName = 0,
        ColNoradId,
        ColIntlDesignator,
        ColSource,
        ColEpoch,
        ColInclination,
        ColPeriod,
        ColApogee,
        ColPerigee,
        ColumnCount
    };

    explicit SatelliteModel(QObject *parent = nullptr);

    void setSatellites(const QVector<Satellite> &satellites);
    const QVector<Satellite> &satellites() const { return m_satellites; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QVector<Satellite> m_satellites;
};

} // namespace SatelliteTracker
