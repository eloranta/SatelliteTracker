#include "TleParser.h"

#include <QDate>
#include <QDateTime>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>

namespace SatelliteTracker {

namespace {
constexpr double kEarthRadiusKm = 6378.137;
constexpr double kMuEarth = 398600.4418; // km^3/s^2, standard gravitational parameter

// TLE fields sometimes omit the leading '+' and use an implied decimal
// point for eccentricity ("1234567" means "0.1234567"). This also handles
// the occasional leading space or sign character safely.
double parseImpliedDecimal(const QString &field)
{
    QString trimmed = field.trimmed();
    if (trimmed.isEmpty()) return 0.0;
    bool ok = false;
    double value = trimmed.toDouble(&ok);
    if (!ok) return 0.0;
    return value / std::pow(10.0, trimmed.length());
}

QString safeSubstring(const QString &line, int start, int length)
{
    if (start >= line.length()) return QString();
    return line.mid(start, length).trimmed();
}
} // namespace

QDateTime TleParser::parseEpoch(const QString &epochField)
{
    // Format: YYDDD.DDDDDDDD  (2-digit year, day-of-year with fractional day)
    if (epochField.length() < 5) return QDateTime();

    bool ok = false;
    const int yy = epochField.left(2).toInt(&ok);
    if (!ok) return QDateTime();

    const double dayOfYearFrac = epochField.mid(2).toDouble(&ok);
    if (!ok) return QDateTime();

    const int year = (yy < 57) ? (2000 + yy) : (1900 + yy);
    const int dayOfYear = static_cast<int>(dayOfYearFrac);
    const double fractionalDay = dayOfYearFrac - dayOfYear;

    QDateTime dt(QDate(year, 1, 1).addDays(dayOfYear - 1), QTime(0, 0, 0), Qt::UTC);
    const qint64 msIntoDay = static_cast<qint64>(fractionalDay * 86400.0 * 1000.0);
    return dt.addMSecs(msIntoDay);
}

bool TleParser::parseLine1(const QString &line1, Satellite &out, QString *error)
{
    if (line1.length() < 32 || line1.at(0) != QLatin1Char('1')) {
        if (error) *error = QStringLiteral("line 1 malformed or too short");
        return false;
    }

    bool ok = false;
    const int noradId = safeSubstring(line1, 2, 5).toInt(&ok);
    if (!ok) {
        if (error) *error = QStringLiteral("could not parse NORAD id from line 1");
        return false;
    }

    out.noradId = noradId;
    out.intlDesignator = safeSubstring(line1, 9, 8);
    out.epochUtc = parseEpoch(safeSubstring(line1, 18, 14));
    out.tleLine1 = line1;
    return true;
}

bool TleParser::parseLine2(const QString &line2, Satellite &out, QString *error)
{
    if (line2.length() < 63 || line2.at(0) != QLatin1Char('2')) {
        if (error) *error = QStringLiteral("line 2 malformed or too short");
        return false;
    }

    bool ok = false;

    const double inclination = safeSubstring(line2, 8, 8).toDouble(&ok);
    if (!ok) { if (error) *error = QStringLiteral("bad inclination field"); return false; }

    const double eccentricity = parseImpliedDecimal(safeSubstring(line2, 26, 7));

    const double meanMotion = safeSubstring(line2, 52, 11).toDouble(&ok);
    if (!ok) { if (error) *error = QStringLiteral("bad mean motion field"); return false; }

    out.inclinationDeg = inclination;
    out.eccentricity = eccentricity;
    out.meanMotionRevPerDay = meanMotion;
    out.tleLine2 = line2;
    return true;
}

void TleParser::computeDerivedQuantities(Satellite &s)
{
    if (s.meanMotionRevPerDay <= 0.0) return;

    s.periodMinutes = 1440.0 / s.meanMotionRevPerDay;

    const double nRadPerSec = s.meanMotionRevPerDay * 2.0 * M_PI / 86400.0;
    const double semiMajorAxisKm = std::cbrt(kMuEarth / (nRadPerSec * nRadPerSec));

    s.apogeeKm = semiMajorAxisKm * (1.0 + s.eccentricity) - kEarthRadiusKm;
    s.perigeeKm = semiMajorAxisKm * (1.0 - s.eccentricity) - kEarthRadiusKm;
}

QVector<Satellite> TleParser::parseThreeLineElementSet(
    const QString &rawText, const QString &source, QString *warningsOut)
{
    QVector<Satellite> result;

    const QStringList lines = rawText.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")),
                                              Qt::SkipEmptyParts);

    int skipped = 0;
    const QDateTime now = QDateTime::currentDateTimeUtc();

    // Each satellite occupies exactly 3 lines: name, line 1, line 2.
    for (int i = 0; i + 2 < lines.size(); i += 3) {
        const QString nameLine = lines[i].trimmed();
        const QString line1 = lines[i + 1];
        const QString line2 = lines[i + 2];

        // Sanity check we're actually at a name/1/2 triple; if not, the feed
        // is malformed and we stop rather than misaligning every subsequent
        // record.
        if (!line1.startsWith(QLatin1Char('1')) || !line2.startsWith(QLatin1Char('2'))) {
            skipped = lines.size() - i;
            break;
        }

        Satellite s;
        s.name = nameLine;
        s.source = source;
        s.lastUpdatedUtc = now;

        QString err;
        if (!parseLine1(line1, s, &err) || !parseLine2(line2, s, &err)) {
            ++skipped;
            continue;
        }

        computeDerivedQuantities(s);
        result.push_back(s);
    }

    if (warningsOut && skipped > 0) {
        *warningsOut = QStringLiteral("Skipped %1 malformed line(s)/record(s) while parsing")
                           .arg(skipped);
    }

    return result;
}

} // namespace SatelliteTracker
