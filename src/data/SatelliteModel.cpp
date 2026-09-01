#include "SatelliteModel.h"

namespace SatelliteTracker {

SatelliteModel::SatelliteModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void SatelliteModel::setSatellites(const QVector<Satellite> &satellites)
{
    beginResetModel();
    m_satellites = satellites;
    endResetModel();
}

int SatelliteModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_satellites.size();
}

int SatelliteModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return ColumnCount;
}

QVariant SatelliteModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_satellites.size())
        return QVariant();

    const Satellite &s = m_satellites.at(index.row());

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColNoradId:
        case ColInclination:
        case ColPeriod:
        case ColApogee:
        case ColPerigee:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    if (role != Qt::DisplayRole) return QVariant();

    switch (index.column()) {
    case ColName:            return s.name;
    case ColNoradId:         return s.noradId;
    case ColIntlDesignator:  return s.intlDesignator;
    case ColSource:          return s.source;
    case ColEpoch:           return s.epochUtc.isValid()
                                     ? s.epochUtc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) + " UTC"
                                     : QStringLiteral("—");
    case ColInclination:     return QString::number(s.inclinationDeg, 'f', 2) + QStringLiteral("°");
    case ColPeriod:          return QString::number(s.periodMinutes, 'f', 1) + QStringLiteral(" min");
    case ColApogee:          return QString::number(s.apogeeKm, 'f', 0) + QStringLiteral(" km");
    case ColPerigee:         return QString::number(s.perigeeKm, 'f', 0) + QStringLiteral(" km");
    default:                 return QVariant();
    }
}

QVariant SatelliteModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case ColName:           return QStringLiteral("Name");
    case ColNoradId:        return QStringLiteral("NORAD ID");
    case ColIntlDesignator: return QStringLiteral("Int'l Designator");
    case ColSource:         return QStringLiteral("Source");
    case ColEpoch:          return QStringLiteral("Epoch");
    case ColInclination:    return QStringLiteral("Inclination");
    case ColPeriod:         return QStringLiteral("Period");
    case ColApogee:         return QStringLiteral("Apogee");
    case ColPerigee:        return QStringLiteral("Perigee");
    default:                return QVariant();
    }
}

} // namespace SatelliteTracker
