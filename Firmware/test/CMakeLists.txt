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
add_executable(protocol_tests test_protocol.cpp)
target_link_libraries(protocol_tests PRIVATE protocol_simulation)
target_compile_definitions(protocol_tests PRIVATE
    ARACHNE_GOLDEN_PATH="${CMAKE_CURRENT_SOURCE_DIR}/../../protocol/golden_vectors.txt")
add_executable(protocol_locked_tests test_protocol_locked.cpp)
target_link_libraries(protocol_locked_tests PRIVATE protocol_locked)
add_executable(protocol_bridge protocol_bridge.cpp)
target_link_libraries(protocol_bridge PRIVATE protocol_simulation)
add_executable(protocol_bridge_locked protocol_bridge.cpp)
target_link_libraries(protocol_bridge_locked PRIVATE protocol_locked)
add_executable(calibration_tests test_calibration.cpp)
target_link_libraries(calibration_tests PRIVATE calibration_simulation)
add_executable(calibration_locked_tests test_calibration_locked.cpp)
target_link_libraries(calibration_locked_tests PRIVATE calibration_locked)
foreach(target IN ITEMS control_tests locked_tests protocol_tests protocol_locked_tests protocol_bridge protocol_bridge_locked calibration_tests calibration_locked_tests)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endforeach()
foreach(target IN ITEMS control_tests locked_tests protocol_tests protocol_locked_tests calibration_tests calibration_locked_tests)
    add_test(NAME ${target} COMMAND ${target})
    set_tests_properties(${target} PROPERTIES TIMEOUT 60)
endforeach()
find_package(Python3 3.11 COMPONENTS Interpreter REQUIRED)
add_test(NAME python_protocol_tests COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Raspberry_Pi/tests/test_protocol.py"
    --bridge $<TARGET_FILE:protocol_bridge> --locked-bridge $<TARGET_FILE:protocol_bridge_locked>)
set_tests_properties(python_protocol_tests PROPERTIES TIMEOUT 120)
