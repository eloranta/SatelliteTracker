#pragma once

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
    // transaction. Returns true on success.
    bool upsertSatellites(const QVector<Satellite> &satellites, QString *errorOut = nullptr);

    // Returns every cached satellite, ordered by name.
    QVector<Satellite> getAllSatellites(QString *errorOut = nullptr) const;

private:
    QString m_connectionName;
};

} // namespace SatelliteTracker
