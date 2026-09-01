#include "SatelliteModel.h"

#include <QRegularExpression>

namespace SatelliteTracker {

namespace {

// Celestrak names familiar satellites as "FULL NAME (SHORT-NAME)", e.g.
// "OSCAR 7 (AO-7)" -- pull out the parenthesized part for the Short Name
// column. ISS is the opposite: "ISS (ZARYA)" names the module in
// parentheses, not the familiar name, so it's special-cased. Falls back to
// the full name when there's no parenthetical suffix at all.
QString extractShortName(const QString &name)
{
    if (name.startsWith(QLatin1String("ISS "))) {
        return QStringLiteral("ISS");
    }

    static const QRegularExpression pattern(QStringLiteral("\\(([^()]+)\\)\\s*$"));
    const QRegularExpressionMatch match = pattern.match(name);
    return match.hasMatch() ? match.captured(1) : name;
}

// Not derivable from TLE data -- a satellite's transponder type isn't
// orbital data. Covers only the satellites whose current FM/linear mode was
// confirmed against AMSAT's live status categorization; anything else (or
// any satellite that goes silent/decommissioned) shows blank rather than a
// guessed or stale mode.
QString lookupMode(const QString &shortName)
{
    static const QHash<QString, QString> modeByShortName = {
        {QStringLiteral("AO-7"), QStringLiteral("Linear")},
        {QStringLiteral("AO-73"), QStringLiteral("Linear")},
        {QStringLiteral("FO-29"), QStringLiteral("Linear")},
        {QStringLiteral("JO-97"), QStringLiteral("Linear")},
        {QStringLiteral("QO-100"), QStringLiteral("Linear")},
        {QStringLiteral("RS-44"), QStringLiteral("Linear")},
        {QStringLiteral("AO-91"), QStringLiteral("FM")},
        {QStringLiteral("AO-123"), QStringLiteral("FM")},
        {QStringLiteral("CAS-3H"), QStringLiteral("FM")},
        {QStringLiteral("IO-86"), QStringLiteral("FM")},
        {QStringLiteral("ISS"), QStringLiteral("FM")},
        {QStringLiteral("PO-101"), QStringLiteral("FM")},
        {QStringLiteral("RS95S"), QStringLiteral("FM")},
        {QStringLiteral("SO-50"), QStringLiteral("FM")},
    };
    return modeByShortName.value(shortName.toUpper());
}

QString formatNextAos(const PassResult &pass)
{
    switch (pass.state) {
    case PassState::CurrentlyInView:
        return QStringLiteral("In view");
    case PassState::UpcomingPass:
        return pass.aosUtc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) + QStringLiteral(" UTC");
    case PassState::NoPassInWindow:
    default:
        return QStringLiteral("No pass in 24h");
    }
}
} // namespace

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

void SatelliteModel::applyPassResults(const QHash<int, PassResult> &resultsByNoradId)
{
    m_passResultsByNoradId = resultsByNoradId;

    for (int row = 0; row < m_satellites.size(); ++row) {
        if (!m_satellites.at(row).isActive) continue;
        const QModelIndex idx = index(row, ColNextAos);
        emit dataChanged(idx, idx, {Qt::DisplayRole});
    }
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

    if (role == Qt::CheckStateRole && index.column() == ColActive) {
        return s.isActive ? Qt::Checked : Qt::Unchecked;
    }

    if (role == Qt::TextAlignmentRole) {
        switch (index.column()) {
        case ColActive:
        case ColMode:
            return int(Qt::AlignCenter);
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
    case ColActive:          return QVariant();
    case ColName:            return s.name;
    case ColShortName:       return extractShortName(s.name);
    case ColMode:            return lookupMode(extractShortName(s.name));
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
    case ColNextAos:         return s.isActive
                                     ? formatNextAos(m_passResultsByNoradId.value(s.noradId))
                                     : QStringLiteral("—");
    default:                 return QVariant();
    }
}

bool SatelliteModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_satellites.size())
        return false;

    if (index.column() == ColActive && role == Qt::CheckStateRole) {
        Satellite &s = m_satellites[index.row()];
        const bool active = (value.toInt() == Qt::Checked);
        if (s.isActive == active) return true;

        s.isActive = active;
        emit dataChanged(index, index, {Qt::CheckStateRole});
        emit activeChanged(s.noradId, active);
        return true;
    }

    return false;
}

Qt::ItemFlags SatelliteModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (!index.isValid()) return f;

    if (index.column() == ColActive) {
        f |= Qt::ItemIsUserCheckable;
    }
    return f;
}

QVariant SatelliteModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section) {
    case ColActive:         return QStringLiteral("Active");
    case ColName:           return QStringLiteral("Full Name");
    case ColShortName:      return QStringLiteral("Name");
    case ColMode:           return QStringLiteral("Mode");
    case ColNoradId:        return QStringLiteral("NORAD ID");
    case ColIntlDesignator: return QStringLiteral("Int'l Designator");
    case ColSource:         return QStringLiteral("Source");
    case ColEpoch:          return QStringLiteral("Epoch");
    case ColInclination:    return QStringLiteral("Inclination");
    case ColPeriod:         return QStringLiteral("Period");
    case ColApogee:         return QStringLiteral("Apogee");
    case ColPerigee:        return QStringLiteral("Perigee");
    case ColNextAos:        return QStringLiteral("Next AOS");
    default:                return QVariant();
    }
}

} // namespace SatelliteTracker
