cmake_minimum_required(VERSION 3.18)

project(YEDIT)

set(YORI_SRC_DIR ${CMAKE_CURRENT_BINARY_DIR}/YEDIT_BLD-prefix/src/YEDIT_BLD)

include(ExternalProject)
ExternalProject_Add(YEDIT_BLD
	URL ${CMAKE_CURRENT_SOURCE_DIR}/yori
	CONFIGURE_COMMAND ""
	BUILD_COMMAND ${CMAKE_COMMAND} -DYORI_SRC_DIR=${YORI_SRC_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/yedit_build.cmake
	INSTALL_COMMAND ${CMAKE_COMMAND} -E copy ${YORI_SRC_DIR}/edit/yedit.exe ${CMAKE_CURRENT_BINARY_DIR}/yedit.exe
)

# TODO - make install rule for yedit.exe
