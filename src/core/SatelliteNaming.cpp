#include "SatelliteNaming.h"

#include <QHash>
#include <QRegularExpression>

namespace SatelliteTracker::SatelliteNaming {

QString shortName(const QString &fullName)
{
    if (fullName.startsWith(QLatin1String("ISS "))) {
        return QStringLiteral("ISS");
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
