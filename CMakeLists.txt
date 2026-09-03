cmake_minimum_required(VERSION 3.25)
project(SatelliteTracker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql Concurrent Charts)

add_subdirectory(third_party/sgp4)

add_executable(SatelliteTracker
    src/main.cpp
    src/data/Satellite.h
    src/data/Database.h
    src/data/Database.cpp
    src/data/SatelliteRepository.h
    src/data/SatelliteRepository.cpp
    src/data/SatelliteModel.h
    src/data/SatelliteModel.cpp
    src/core/TleParser.h
    src/core/TleParser.cpp
    src/core/CelestrakClient.h
    src/core/CelestrakClient.cpp
    src/core/Maidenhead.h
    src/core/Maidenhead.cpp
    src/core/SatelliteNaming.h
    src/core/SatelliteNaming.cpp
    src/core/Orbit/IOrbitPropagator.h
    src/core/Orbit/Sgp4OrbitPropagator.h
    src/core/Orbit/Sgp4OrbitPropagator.cpp
    src/core/Orbit/PassFinder.h
    src/core/Orbit/PassFinder.cpp
    src/core/ObserverLocation.h
    src/core/AppSettings.h
    src/core/AppSettings.cpp
    src/core/ActiveSatelliteTracker.h
    src/core/ActiveSatelliteTracker.cpp
    src/ui/MainWindow.h
    src/ui/MainWindow.cpp
    src/ui/ObserverLocationDialog.h
    src/ui/ObserverLocationDialog.cpp
    src/ui/PassCard.h
    src/ui/PassCard.cpp
    src/ui/PassGridWidget.h
    src/ui/PassGridWidget.cpp
    src/ui/PassRadarTab.h
    src/ui/PassRadarTab.cpp
)

target_include_directories(SatelliteTracker PRIVATE src)

target_link_libraries(SatelliteTracker PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    Qt6::Concurrent
    Qt6::Charts
    sgp4
)

if(WIN32)
    set_target_properties(SatelliteTracker PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

enable_testing()
add_subdirectory(tests)
