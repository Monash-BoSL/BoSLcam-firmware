# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(native_camera)

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/ov7675.c)
target_sources(app PRIVATE src/sram.c)
target_sources(app PRIVATE src/ftp.c)
target_sources(app PRIVATE src/sd.c)
target_sources(app PRIVATE src/util.c)
target_sources(app PRIVATE src/jpg.c)
target_sources(app PRIVATE src/common.c)
