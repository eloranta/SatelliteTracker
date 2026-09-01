add_executable(orbit_tests
    orbit_tests.cpp
    ../src/core/Maidenhead.cpp
    ../src/core/Orbit/Sgp4OrbitPropagator.cpp
    ../src/core/Orbit/PassFinder.cpp
)
target_link_libraries(orbit_tests PRIVATE Qt6::Core sgp4)

add_test(NAME OrbitTests COMMAND orbit_tests)
