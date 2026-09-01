#pragma once

#include <QString>
#include <QVector>

#include "../data/Satellite.h"

namespace SatelliteTracker {

// Parses Celestrak-style "3-line element" text (name line + 2 TLE lines,
// repeated) into Satellite records, filling in both the raw TLE fields and
// derived orbital quantities (inclination, period, apogee/perigee).
//
// Malformed groups (wrong line count, unparsable numeric fields) are
// skipped rather than aborting the whole batch; skipped-group count and
// reasons are appended to *warningsOut if provided.
class TleParser {
public:
    static QVector<Satellite> parseThreeLineElementSet(
        const QString &rawText,
        const QString &source,
        QString *warningsOut = nullptr);

private:
    static bool parseLine1(const QString &line1, Satellite &out, QString *error);
    static bool parseLine2(const QString &line2, Satellite &out, QString *error);
    static QDateTime parseEpoch(const QString &epochField);
    static void computeDerivedQuantities(Satellite &s);
};

} // namespace SatelliteTracker
