#include "SatelliteRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace SatelliteTracker {

SatelliteRepository::SatelliteRepository(const QString &connectionName)
    : m_connectionName(connectionName)
{
}

bool SatelliteRepository::upsertSatellites(const QVector<Satellite> &satellites, const QString &group,
                                            QString *errorOut)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        if (errorOut) *errorOut = QStringLiteral("Database connection is not open");
        return false;
    }

    if (!db.transaction()) {
        if (errorOut) *errorOut = db.lastError().text();
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO satellites ("
        "  norad_id, name, intl_designator, tle_line1, tle_line2, epoch_utc,"
        "  source, sat_group, is_active, last_updated_utc, inclination_deg, eccentricity,"
        "  mean_motion, period_minutes, apogee_km, perigee_km"
        ") VALUES ("
        "  :norad_id, :name, :intl_designator, :tle_line1, :tle_line2, :epoch_utc,"
        "  :source, :sat_group, "
        "  COALESCE((SELECT is_active FROM satellites WHERE norad_id = :norad_id_lookup), 0),"
        "  :last_updated_utc, :inclination_deg, :eccentricity,"
        "  :mean_motion, :period_minutes, :apogee_km, :perigee_km"
        ")"
        " ON CONFLICT(norad_id) DO UPDATE SET"
        "  name=excluded.name, intl_designator=excluded.intl_designator,"
        "  tle_line1=excluded.tle_line1, tle_line2=excluded.tle_line2,"
        "  epoch_utc=excluded.epoch_utc, source=excluded.source,"
        "  sat_group=excluded.sat_group,"
        "  last_updated_utc=excluded.last_updated_utc,"
        "  inclination_deg=excluded.inclination_deg, eccentricity=excluded.eccentricity,"
        "  mean_motion=excluded.mean_motion, period_minutes=excluded.period_minutes,"
        "  apogee_km=excluded.apogee_km, perigee_km=excluded.perigee_km"
    ));

    for (const Satellite &s : satellites) {
        q.bindValue(QStringLiteral(":norad_id"), s.noradId);
        q.bindValue(QStringLiteral(":norad_id_lookup"), s.noradId);
        q.bindValue(QStringLiteral(":name"), s.name);
        q.bindValue(QStringLiteral(":intl_designator"), s.intlDesignator);
        q.bindValue(QStringLiteral(":tle_line1"), s.tleLine1);
        q.bindValue(QStringLiteral(":tle_line2"), s.tleLine2);
        q.bindValue(QStringLiteral(":epoch_utc"), s.epochUtc.toUTC().toString(Qt::ISODate));
        q.bindValue(QStringLiteral(":source"), s.source);
        q.bindValue(QStringLiteral(":sat_group"), group);
        q.bindValue(QStringLiteral(":last_updated_utc"), s.lastUpdatedUtc.toUTC().toString(Qt::ISODate));
        q.bindValue(QStringLiteral(":inclination_deg"), s.inclinationDeg);
        q.bindValue(QStringLiteral(":eccentricity"), s.eccentricity);
        q.bindValue(QStringLiteral(":mean_motion"), s.meanMotionRevPerDay);
        q.bindValue(QStringLiteral(":period_minutes"), s.periodMinutes);
        q.bindValue(QStringLiteral(":apogee_km"), s.apogeeKm);
        q.bindValue(QStringLiteral(":perigee_km"), s.perigeeKm);

        if (!q.exec()) {
            if (errorOut) *errorOut = q.lastError().text();
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        if (errorOut) *errorOut = db.lastError().text();
        db.rollback();
        return false;
    }

    return true;
}

QVector<Satellite> SatelliteRepository::getAllSatellites(const QString &group, QString *errorOut) const
{
    QVector<Satellite> result;

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        if (errorOut) *errorOut = QStringLiteral("Database connection is not open");
        return result;
    }

    QSqlQuery q(db);
    QString sql = QStringLiteral(
        "SELECT norad_id, name, intl_designator, tle_line1, tle_line2, epoch_utc,"
        "       source, is_active, last_updated_utc, inclination_deg, eccentricity,"
        "       mean_motion, period_minutes, apogee_km, perigee_km"
        " FROM satellites");
    if (!group.isEmpty()) {
        sql += QStringLiteral(" WHERE sat_group = :sat_group");
    }
    sql += QStringLiteral(" ORDER BY name COLLATE NOCASE ASC");

    q.prepare(sql);
    if (!group.isEmpty()) {
        q.bindValue(QStringLiteral(":sat_group"), group);
    }
    const bool ok = q.exec();

    if (!ok) {
        if (errorOut) *errorOut = q.lastError().text();
        return result;
    }

    while (q.next()) {
        Satellite s;
        s.noradId = q.value(0).toInt();
        s.name = q.value(1).toString();
        s.intlDesignator = q.value(2).toString();
        s.tleLine1 = q.value(3).toString();
        s.tleLine2 = q.value(4).toString();
        s.epochUtc = QDateTime::fromString(q.value(5).toString(), Qt::ISODate);
        s.source = q.value(6).toString();
        s.isActive = q.value(7).toBool();
        s.lastUpdatedUtc = QDateTime::fromString(q.value(8).toString(), Qt::ISODate);
        s.inclinationDeg = q.value(9).toDouble();
        s.eccentricity = q.value(10).toDouble();
        s.meanMotionRevPerDay = q.value(11).toDouble();
        s.periodMinutes = q.value(12).toDouble();
        s.apogeeKm = q.value(13).toDouble();
        s.perigeeKm = q.value(14).toDouble();
        result.push_back(s);
    }

    return result;
}

QDateTime SatelliteRepository::getLastCatalogUpdateUtc(QString *errorOut) const
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        if (errorOut) *errorOut = QStringLiteral("Database connection is not open");
        return QDateTime();
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "SELECT value FROM catalog_metadata WHERE key = :key"));
    q.bindValue(QStringLiteral(":key"), QStringLiteral("last_updated_utc"));

    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return QDateTime();
    }

    if (!q.next()) {
        return QDateTime();
    }

    return QDateTime::fromString(q.value(0).toString(), Qt::ISODate);
}

bool SatelliteRepository::setLastCatalogUpdateUtc(const QDateTime &utc, QString *errorOut)
{
    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    if (!db.isOpen()) {
        if (errorOut) *errorOut = QStringLiteral("Database connection is not open");
        return false;
    }

    QSqlQuery q(db);
    q.prepare(QStringLiteral(
        "INSERT INTO catalog_metadata (key, value) VALUES (:key, :value)"
        " ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    q.bindValue(QStringLiteral(":key"), QStringLiteral("last_updated_utc"));
    q.bindValue(QStringLiteral(":value"), utc.toUTC().toString(Qt::ISODate));

    if (!q.exec()) {
        if (errorOut) *errorOut = q.lastError().text();
        return false;
    }

    return true;
}

} // namespace SatelliteTracker
