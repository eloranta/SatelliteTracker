#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include "Satellite.h"

namespace SatelliteTracker {

// DAO layer between the parsed TLE data and SQLite. Upserts are keyed by
// NORAD ID so re-fetching a catalog updates existing rows in place.
class SatelliteRepository {
public:
    explicit SatelliteRepository(const QString &connectionName);

    // Inserts or updates every satellite in `satellites` in a single
    // transaction, tagging each row with `group` (the Celestrak group it was
    // fetched as part of). Returns true on success.
    bool upsertSatellites(const QVector<Satellite> &satellites, const QString &group,
                           QString *errorOut = nullptr);

    // Returns cached satellites ordered by name. When `group` is non-empty,
    // only satellites last fetched as part of that Celestrak group are
    // returned; an empty `group` returns everything ever cached.
    QVector<Satellite> getAllSatellites(const QString &group = QString(),
                                         QString *errorOut = nullptr) const;

    // UTC timestamp of the last successful catalog refresh from Celestrak.
    // Returns an invalid QDateTime if the catalog has never been refreshed.
    QDateTime getLastCatalogUpdateUtc(QString *errorOut = nullptr) const;

    // Records `utc` as the moment the catalog was last refreshed.
    bool setLastCatalogUpdateUtc(const QDateTime &utc, QString *errorOut = nullptr);

    // Persists whether a single satellite is on the M2 "active" watchlist.
    bool setSatelliteActive(int noradId, bool active, QString *errorOut = nullptr);

private:
    QString m_connectionName;
};

} // namespace SatelliteTracker
