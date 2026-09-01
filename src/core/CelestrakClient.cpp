#include "CelestrakClient.h"

#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>

namespace SatelliteTracker {

CelestrakClient::CelestrakClient(QObject *parent)
    : QObject(parent)
{
}

void CelestrakClient::fetchGroup(const QString &group)
{
    QUrl url(QStringLiteral("https://celestrak.org/NORAD/elements/gp.php"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("GROUP"), group);
    query.addQueryItem(QStringLiteral("FORMAT"), QStringLiteral("tle"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("SatelliteTracker/0.1"));

    QNetworkReply *reply = m_networkManager.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, group]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            emit fetchFailed(group, reply->errorString());
            return;
        }

        const QByteArray body = reply->readAll();
        if (body.trimmed().isEmpty()) {
            emit fetchFailed(group, QStringLiteral("Empty response from Celestrak"));
            return;
        }

        emit fetchSucceeded(group, body);
    });
}

} // namespace SatelliteTracker
