# SPDX-License-Identifier: LicenseRef-Ezurio-Clause

cmake_minimum_required(VERSION 3.20.0)

if(CONFIG_APP_DEBUG)
  set(EXTRA_CONF_FILE ${CMAKE_CURRENT_LIST_DIR}/debug.conf)
  set(EXTRA_DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/debug.overlay)
endif()

find_package(Zephyr REQUIRED HINTS)

project(dvk_probe)

zephyr_include_directories(include)
FILE(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})
