#include "Database.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace SatelliteTracker {

QString Database::defaultDatabasePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/data.db");
}

bool Database::openAndMigrate(const QString &connectionName, QString *errorOut)
{
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
    db.setDatabaseName(defaultDatabasePath());

    if (!db.open()) {
        if (errorOut) *errorOut = db.lastError().text();
        return false;
    }

    return applySchema(db, errorOut);
}

bool Database::applySchema(QSqlDatabase &db, QString *errorOut)
{
    QSqlQuery q(db);

    // NOTE: is_active is included now (defaulting to 0) so the column
    // survives forward into M2, when Tab 2 gains checkboxes and Tab 1
    // starts reading it. It is not surfaced in the M1 UI.
    const bool ok = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS satellites ("
        "  norad_id INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  intl_designator TEXT,"
        "  tle_line1 TEXT NOT NULL,"
        "  tle_line2 TEXT NOT NULL,"
        "  epoch_utc TEXT,"
        "  source TEXT,"
        "  is_active INTEGER NOT NULL DEFAULT 0,"
        "  last_updated_utc TEXT,"
        "  inclination_deg REAL,"
        "  eccentricity REAL,"
        "  mean_motion REAL,"
        "  period_minutes REAL,"
        "  apogee_km REAL,"
        "  perigee_km REAL"
        ")"));

    if (!ok) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }

    // sat_group tags each row with the Celestrak group it was last fetched
    // as part of (e.g. "amateur", "active"), so the catalog view can be
    // scoped to one group instead of always showing everything ever cached.
    // Added after the initial release, so existing installs need it added
    // via ALTER TABLE rather than picking it up from CREATE TABLE IF NOT
    // EXISTS above.
    bool hasGroupColumn = false;
    if (q.exec(QStringLiteral("PRAGMA table_info(satellites)"))) {
        while (q.next()) {
            if (q.value(QStringLiteral("name")).toString() == QLatin1String("sat_group")) {
                hasGroupColumn = true;
                break;
            }
        }
    }
    if (!hasGroupColumn && !q.exec(QStringLiteral("ALTER TABLE satellites ADD COLUMN sat_group TEXT"))) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }

    // Single-row-per-key store for catalog-level facts that don't belong to
    // any one satellite, e.g. when the catalog as a whole was last refreshed
    // from Celestrak (as opposed to satellites.last_updated_utc, which is
    // per-row).
    const bool metaOk = q.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS catalog_metadata ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT"
        ")"));

    if (!metaOk) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }

    return true;
}

} // namespace SatelliteTracker
