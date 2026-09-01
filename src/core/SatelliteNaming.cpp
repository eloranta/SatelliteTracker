#include "SatelliteNaming.h"

#include <QHash>
#include <QRegularExpression>

namespace SatelliteTracker::SatelliteNaming {

QString shortName(const QString &fullName)
{
    if (fullName.startsWith(QLatin1String("ISS "))) {
        return QStringLiteral("ISS");
    }

    // A handful of satellites don't follow Celestrak's usual "FULL NAME
    // (SHORT-NAME)" convention at all -- either the common designator is
    // outside the parens (like ISS above), or there's no parenthetical
    // suffix, or none at all matching the familiar name -- so the regex
    // below would otherwise return the wrong thing (or the whole ugly full
    // name) and the satellite would look absent under its familiar name.
    // Each entry here was confirmed directly against a live Celestrak
    // amateur-group fetch, not guessed from memory.
    static const QHash<QString, QString> knownAliases = {
        {QStringLiteral("RADFXSAT (FOX-1B)"), QStringLiteral("AO-91")},
        {QStringLiteral("DIWATA-2B"), QStringLiteral("PO-101")},
        {QStringLiteral("LILACSAT-2"), QStringLiteral("CAS-3H")},
        {QStringLiteral("RS-44 & BREEZE-KM R/B"), QStringLiteral("RS-44")},
    };
    const auto aliasIt = knownAliases.find(fullName);
    if (aliasIt != knownAliases.end()) {
        return aliasIt.value();
    }

    static const QRegularExpression pattern(QStringLiteral("\\(([^()]+)\\)\\s*$"));
    const QRegularExpressionMatch match = pattern.match(fullName);
    return match.hasMatch() ? match.captured(1) : fullName;
}

QString mode(const QString &shortName)
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

} // namespace SatelliteTracker::SatelliteNaming
