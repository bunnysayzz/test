cmake_minimum_required(VERSION 3.20)
project(CrowmanStudy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /I${CMAKE_SOURCE_DIR}/res")

add_executable(crowman_study WIN32
    src/main.cpp
    src/config.cpp
    src/capture.cpp
    src/windowpick.cpp
    src/http.cpp
    src/netutil.cpp
    src/json.cpp
    src/ai.cpp
    src/license.cpp
    src/secret.cpp
    src/secrets.cpp
    src/redact.cpp
    src/log.cpp
    src/strings.cpp
    src/history.cpp
    src/history_window.cpp
    src/dragdrop.cpp
    src/settings.cpp
    src/hotkey.cpp
    src/util.cpp
    res/app.rc
)

target_compile_definitions(crowman_study PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
)
target_include_directories(crowman_study PRIVATE res src)
target_compile_options(crowman_study PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

add_custom_command(TARGET crowman_study PRE_BUILD
    COMMAND python ${CMAKE_SOURCE_DIR}/tools/make_icon.py)

if(MINGW)
    target_link_options(crowman_study PRIVATE -static -municode)
endif()

target_link_libraries(crowman_study PRIVATE
    winhttp gdiplus bcrypt crypt32 dbghelp shcore dwmapi
    comctl32 shell32 ole32 user32 gdi32 advapi32 ws2_32
)

enable_testing()

foreach(test json netutil config)
    add_executable(${test}_test tests/${test}_test.cpp)
    if(test STREQUAL "json")
        target_sources(${test}_test PRIVATE src/json.cpp)
    elseif(test STREQUAL "netutil")
        target_sources(${test}_test PRIVATE src/netutil.cpp)
    else()
        target_sources(${test}_test PRIVATE src/hotkey.cpp)
        target_include_directories(${test}_test PRIVATE src)
    endif()
    add_test(NAME ${test} COMMAND ${test}_test)
endforeach()

if(WIN32)
    foreach(test capture config_win license http)
        # config_win is tests/config_test_win.cpp in this tree.
        if(test STREQUAL "config_win")
            set(test_source tests/config_test_win.cpp)
        else()
            set(test_source tests/${test}_test.cpp)
        endif()
        add_executable(${test}_test ${test_source})
        target_sources(${test}_test PRIVATE
            src/capture.cpp src/config.cpp src/license.cpp src/http.cpp
            src/netutil.cpp src/json.cpp src/util.cpp src/secret.cpp
            src/secrets.cpp src/redact.cpp src/log.cpp src/history.cpp
            src/windowpick.cpp src/hotkey.cpp src/strings.cpp)
        target_include_directories(${test}_test PRIVATE src res)
        target_compile_definitions(${test}_test PRIVATE UNICODE _UNICODE NOMINMAX)
        target_link_libraries(${test}_test PRIVATE
            winhttp gdiplus bcrypt crypt32 dbghelp shcore dwmapi
            comctl32 shell32 ole32 user32 gdi32 advapi32 ws2_32)
        add_test(NAME ${test} COMMAND ${test}_test)
    endforeach()
endif()
