#pragma once

#include <QString>

namespace SatelliteTracker::SatelliteNaming {

// Celestrak names familiar satellites as "FULL NAME (SHORT-NAME)", e.g.
// "OSCAR 7 (AO-7)" -- pulls out the parenthesized part. ISS is the
// opposite: "ISS (ZARYA)" names the module in parentheses, not the
// familiar name, so it's special-cased. Falls back to the full name when
// there's no parenthetical suffix at all.
QString shortName(const QString &fullName);

// Not derivable from TLE data -- a satellite's transponder type isn't
// orbital data. Covers only the satellites whose current FM/linear mode was
// confirmed against AMSAT's live status categorization; anything else (or
// any satellite that goes silent/decommissioned) returns an empty string.
QString mode(const QString &shortName);

} // namespace SatelliteTracker::SatelliteNaming
