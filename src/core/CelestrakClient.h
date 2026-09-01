#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QString>

namespace SatelliteTracker {

// Thin async wrapper around Celestrak's GP TLE endpoint. No authentication
// required. One request in flight at a time is enough for M1; if multiple
// groups need merging later, queue calls to fetchGroup() from the caller.
class CelestrakClient : public QObject {
    Q_OBJECT
public:
    explicit CelestrakClient(QObject *parent = nullptr);

    // Kicks off an async GET for the given Celestrak group (e.g. "active",
    // "stations", "visual", "weather", "starlink", "last-30-days").
    void fetchGroup(const QString &group);

signals:
    void fetchSucceeded(const QString &group, const QByteArray &rawTleText);
    void fetchFailed(const QString &group, const QString &errorMessage);

private:
    QNetworkAccessManager m_networkManager;
};

} // namespace SatelliteTracker
