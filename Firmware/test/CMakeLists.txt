set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
try_compile(UNSAFE_SIMULATION_COMPILES
    "${CMAKE_CURRENT_BINARY_DIR}/guard-check"
    SOURCES "${CMAKE_CURRENT_LIST_DIR}/../control_core/src/actuator.cpp"
    CMAKE_FLAGS
        "-DCMAKE_CXX_STANDARD=17"
        "-DINCLUDE_DIRECTORIES=${CMAKE_CURRENT_LIST_DIR}/../control_core/include"
    COMPILE_DEFINITIONS -DESP_PLATFORM -DARACHNE_HOST_SIMULATION=1
    OUTPUT_VARIABLE guard_output)
if(UNSAFE_SIMULATION_COMPILES OR NOT guard_output MATCHES "Host simulation enablement must never")
    message(FATAL_ERROR "ESP32 simulation compile guard failed: ${guard_output}")
endif()
message(STATUS "ESP32 simulation compile guard rejected unsafe flags as expected")

add_executable(control_tests test_control.cpp)
target_link_libraries(control_tests PRIVATE arachne_simulation)
add_executable(locked_tests test_locked.cpp ../esp32/main/main.cpp)
target_link_libraries(locked_tests PRIVATE arachne_locked)
foreach(target IN ITEMS control_tests locked_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME ${target} COMMAND ${target})
endforeach()
