# Vendored from https://github.com/dnwrnr/sgp4 (Apache License 2.0), pinned at
# commit 661e057a5d369d5ee424676cf1d69cbead95ff2c. See LICENSE in this directory.
add_library(sgp4 STATIC
    libsgp4/CoordGeodetic.cc
    libsgp4/CoordTopocentric.cc
    libsgp4/DateTime.cc
    libsgp4/DecayedException.cc
    libsgp4/Eci.cc
    libsgp4/Globals.cc
    libsgp4/Observer.cc
    libsgp4/OrbitalElements.cc
    libsgp4/SGP4.cc
    libsgp4/SatelliteException.cc
    libsgp4/SolarPosition.cc
    libsgp4/TimeSpan.cc
    libsgp4/Tle.cc
    libsgp4/TleException.cc
    libsgp4/Util.cc
    libsgp4/Vector.cc
)
target_include_directories(sgp4 PUBLIC libsgp4)
