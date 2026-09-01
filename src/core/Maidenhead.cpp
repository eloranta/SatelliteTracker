#include "Maidenhead.h"

namespace SatelliteTracker::Maidenhead {

bool isValidLocator(const QString &locator, QString *errorOut)
{
    const QString trimmed = locator.trimmed();

    if (trimmed.length() != 4 && trimmed.length() != 6) {
        if (errorOut) *errorOut = QStringLiteral("Locator must be 4 or 6 characters");
        return false;
    }

    const QChar field0 = trimmed.at(0).toUpper();
    const QChar field1 = trimmed.at(1).toUpper();
    if (field0 < QLatin1Char('A') || field0 > QLatin1Char('R')
        || field1 < QLatin1Char('A') || field1 > QLatin1Char('R')) {
        if (errorOut) *errorOut = QStringLiteral("First pair must be letters A-R");
        return false;
    }

    if (!trimmed.at(2).isDigit() || !trimmed.at(3).isDigit()) {
        if (errorOut) *errorOut = QStringLiteral("Second pair must be digits 0-9");
        return false;
    }

    if (trimmed.length() == 6) {
        const QChar sub0 = trimmed.at(4).toLower();
        const QChar sub1 = trimmed.at(5).toLower();
        if (sub0 < QLatin1Char('a') || sub0 > QLatin1Char('x')
            || sub1 < QLatin1Char('a') || sub1 > QLatin1Char('x')) {
            if (errorOut) *errorOut = QStringLiteral("Third pair must be letters A-X");
            return false;
        }
    }

    return true;
}

bool locatorToLatLon(const QString &locator, GeoCoordinate *outCoord, QString *errorOut)
{
    if (!isValidLocator(locator, errorOut)) {
        return false;
    }

    const QString trimmed = locator.trimmed();
    const QChar field0 = trimmed.at(0).toUpper();
    const QChar field1 = trimmed.at(1).toUpper();
    const QChar square0 = trimmed.at(2);
    const QChar square1 = trimmed.at(3);

    double lon = (field0.unicode() - QLatin1Char('A').unicode()) * 20.0 - 180.0
               + (square0.unicode() - QLatin1Char('0').unicode()) * 2.0;
    double lat = (field1.unicode() - QLatin1Char('A').unicode()) * 10.0 - 90.0
               + (square1.unicode() - QLatin1Char('0').unicode()) * 1.0;

    if (trimmed.length() == 4) {
        lon += 1.0; // center of a 2°-wide field square
        lat += 0.5; // center of a 1°-tall field square
    } else {
        const QChar sub0 = trimmed.at(4).toLower();
        const QChar sub1 = trimmed.at(5).toLower();
        lon += (sub0.unicode() - QLatin1Char('a').unicode()) * (2.0 / 24.0) + (2.0 / 24.0) / 2.0;
        lat += (sub1.unicode() - QLatin1Char('a').unicode()) * (1.0 / 24.0) + (1.0 / 24.0) / 2.0;
    }

    if (outCoord) {
        outCoord->latitudeDeg = lat;
        outCoord->longitudeDeg = lon;
    }
    return true;
}

} // namespace SatelliteTracker::Maidenhead
