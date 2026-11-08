# ADVENYA setup · simulador de host (pruebas SIMULADAS, no sustituyen a la placa)
#
# Compila la MISMA interfaz que el firmware (components/advenya_ui), el nucleo
# de persistencia, el punto de integracion y los assets, con LVGL 9.2.2 en el
# PC: pantalla virtual 480x800 RGB565, tactil guionizado y reloj virtual.
#
#   cmake -S sim -B sim/build -DLVGL_DIR=<ruta a lvgl v9.2.2> && cmake --build sim/build
#   python3 sim/run_tests.py
cmake_minimum_required(VERSION 3.16)
project(advenya_sim C)
set(CMAKE_C_STANDARD 11)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(LVGL_DIR "${ROOT}/managed_components/lvgl__lvgl" CACHE PATH "Ruta a LVGL v9.2.2")
if(NOT EXISTS "${LVGL_DIR}/lvgl.h")
    include(FetchContent)
    FetchContent_Declare(lvgl GIT_REPOSITORY https://github.com/lvgl/lvgl.git
                         GIT_TAG 7f07a129e8d77f4984fff8e623fd5be18ff42e74)   # v9.2.2
    FetchContent_Populate(lvgl)
    set(LVGL_DIR ${lvgl_SOURCE_DIR})
endif()

option(ADV_SIM_ASAN "AddressSanitizer + UBSan" ON)

file(GLOB_RECURSE LVGL_SRCS ${LVGL_DIR}/src/*.c)
add_library(lvgl_host STATIC ${LVGL_SRCS})
target_include_directories(lvgl_host PUBLIC ${LVGL_DIR} ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(lvgl_host PUBLIC LV_CONF_INCLUDE_SIMPLE=1)
target_compile_options(lvgl_host PRIVATE -O2 -w)

file(GLOB UI_SRCS ${ROOT}/components/advenya_ui/src/*.c
                  ${ROOT}/components/advenya_ui/src/i18n/*.c
                  ${ROOT}/components/advenya_ui/src/legal/*.c)
list(FILTER UI_SRCS EXCLUDE REGEX "ui_platform_esp\\.c$")
file(GLOB ASSET_SRCS ${ROOT}/components/advenya_assets/src/generated/*.c
                     ${ROOT}/components/advenya_assets/src/fonts/*.c)

add_executable(advenya_sim
    sim_main.c platform_sim.c settings_file.c
    ${UI_SRCS} ${ASSET_SRCS}
    ${ROOT}/components/advenya_settings/src/settings_core.c
    ${ROOT}/components/advenya_handoff/src/advenya_handoff.c)
target_include_directories(advenya_sim PRIVATE
    ${ROOT}/components/advenya_ui/include ${ROOT}/components/advenya_ui/src
    ${ROOT}/components/advenya_assets/include ${ROOT}/components/advenya_settings/include
    ${ROOT}/components/advenya_handoff/include)
target_compile_options(advenya_sim PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare -O1 -g)
target_link_libraries(advenya_sim lvgl_host m)
if(ADV_SIM_ASAN)
    target_compile_options(advenya_sim PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(advenya_sim PRIVATE -fsanitize=address,undefined)
endif()
