#include "SatelliteNaming.h"

#include <QHash>
#include <QRegularExpression>

namespace SatelliteTracker::SatelliteNaming {

QString shortName(const QString &fullName)
{
    if (fullName.startsWith(QLatin1String("ISS "))) {
        return QStringLiteral("ISS");
    }

    // A handful of satellites have their common designator OUTSIDE the
    // parentheses on Celestrak, the reverse of the usual "FULL NAME
    // (SHORT-NAME)" convention (like ISS above) -- e.g. AO-91 is listed as
    // "RADFXSAT (FOX-1B)", so the regex below would otherwise return
    // "FOX-1B" and the satellite would look absent under its familiar name.
    static const QHash<QString, QString> knownAliases = {
        {QStringLiteral("RADFXSAT (FOX-1B)"), QStringLiteral("AO-91")},
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
