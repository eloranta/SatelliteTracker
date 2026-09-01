#pragma once

#include <QSqlDatabase>
#include <QString>

namespace SatelliteTracker {

// Opens (creating if necessary) the local SQLite database used to cache
// satellites, and applies schema migrations. M1 ships schema version 1
// (satellites table only); later milestones add passes/alerts/log tables.
class Database {
public:
    // Opens a connection named `connectionName` pointing at the app's
    // local-data SQLite file and ensures the schema exists.
    // Returns true on success; on failure, *errorOut (if non-null) is set.
    static bool openAndMigrate(const QString &connectionName, QString *errorOut = nullptr);

    // Full path to the SQLite file under the per-user local app data folder,
    // e.g. C:/Users/<user>/AppData/Local/SatelliteTracker/data.db
    static QString defaultDatabasePath();

private:
    static bool applySchema(QSqlDatabase &db, QString *errorOut);
};

} // namespace SatelliteTracker
