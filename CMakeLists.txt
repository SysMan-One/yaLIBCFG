#+++
#
#	FACILITY:	yaLIBCFG - configuration trees in the libconfig syntax
#
#	MODULE:		CMakeLists.txt
#
#	ABSTRACT:	Build of the library libyalibcfg.a, its tests.
#
#	AUTHOR:		StarLet Squad and Ruslan R. Laishev (AKA: BadAss SysMan)
#
#	CREATION DATE:	22-NOV-2026
#
#	MODIFICATION HISTORY:
#
#		22-NOV-2026	RRL	X.01-01 : First edition - stage 1: parse, tree, paths,
#					lossless write; round-trip, edit, error tests.
#		22-NOV-2026	RRL	X.01-02 : CFG$M_SKIPERRS - skip bad settings; CFGTOOL /SKIPERRORS.
#		22-NOV-2026	RRL	X.01-03 : a leading 0 is octal (as libconfig); %X %D %B %O; ! comments.
#		22-NOV-2026	RRL	X.01-03ECO01 : CFGTOOL tells the IDENT of the head module.
#		22-NOV-2026	RRL	X.01-03ECO02 : CFGTOOL help: the comments and the integers of a file.
#
#---
cmake_minimum_required(VERSION 3.16)

#
#	The version is that of the head module, include/yalibcfgdef.h:
#	__YALIBCFGDEF_IDENT__ X.aa-bb[ECOcc] gives aa.bb.cc
#
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/yalibcfgdef.h" YALIBCFG_K_IDENTLINE REGEX "define[ \t]+__YALIBCFGDEF_IDENT__")
string(REGEX MATCH "\"[XV]\\.([0-9][0-9])-([0-9][0-9])(ECO([0-9][0-9]))?\"" YALIBCFG_K_IDENT "${YALIBCFG_K_IDENTLINE}")

if(NOT YALIBCFG_K_IDENT)
	message(FATAL_ERROR "yaLIBCFG: no __YALIBCFGDEF_IDENT__ of the form X.aa-bb[ECOcc] in include/yalibcfgdef.h")
endif()

math(EXPR YALIBCFG_K_MAJOR "${CMAKE_MATCH_1}")
math(EXPR YALIBCFG_K_MINOR "${CMAKE_MATCH_2}")
if(CMAKE_MATCH_4)
	math(EXPR YALIBCFG_K_PATCH "${CMAKE_MATCH_4}")
else()
	set(YALIBCFG_K_PATCH 0)
endif()

project(yaLIBCFG VERSION ${YALIBCFG_K_MAJOR}.${YALIBCFG_K_MINOR}.${YALIBCFG_K_PATCH} LANGUAGES C)

string(REPLACE "\"" "" YALIBCFG_K_IDENT "${YALIBCFG_K_IDENT}")
message(STATUS "yaLIBCFG: ${YALIBCFG_K_IDENT} -> ${PROJECT_VERSION}")

#
#	Release by default: the messages of StarLet carry the place in the code only in a
#	build without NDEBUG, which is for the developer
#
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

#
#	StarLet - the installed one (CMAKE_PREFIX_PATH=/opt/starlet when it is not
#	in a default place); never a vendored copy.
#
find_package(StarLet 1.7 REQUIRED CONFIG)
message(STATUS "yaLIBCFG: StarLet ${StarLet_VERSION} from ${StarLet_DIR}")

find_package(Threads REQUIRED)

add_library(yalibcfg STATIC
	src/cfg_msg.c
	src/cfg_lex.c
	src/cfg_parse.c
	src/cfg_tree.c
	src/cfg_write.c
	src/cfg_diff.c
	src/cfg_schema.c
	src/cfg_sha1.c)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

target_include_directories(yalibcfg
	PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
	PRIVATE	${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_options(yalibcfg PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(yalibcfg PUBLIC StarLet::starlet Threads::Threads m)

#
#	CFGTOOL - the command-line utility
#
add_executable(cfgtool src/cfgtool.c)
target_compile_options(cfgtool PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(cfgtool PRIVATE yalibcfg)

#
#	The help library of CFGTOOL (HELP CFGTOOL), when the HELP package is there
#
option(YALIBCFG_M_HELPLIB "Build the help libraries of CFGTOOL (needs the HELP package)" ON)

if(YALIBCFG_M_HELPLIB)
	find_package(HELP 1.0 QUIET)
endif()

if(HELP_FOUND)
	message(STATUS "yaLIBCFG: HELP ${HELP_VERSION} found - the help libraries of CFGTOOL are built")

	# HELP_ADD_LIBRARY installs with no COMPONENT: the default one is set for it - "tools"
	set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME tools)

	help_add_library(NAME cfgtoollib ALL NOFACILITY
		SOURCE		${CMAKE_CURRENT_SOURCE_DIR}/doc/cfgtoollib.md
		OUTPUT		${CMAKE_CURRENT_BINARY_DIR}/cfgtool.hlb
		IDENT		${YALIBCFG_K_IDENT}
		DESTINATION	${CMAKE_INSTALL_DATADIR}/help)

	help_add_library(NAME cfgtoollib_ru ALL NOFACILITY
		SOURCE		${CMAKE_CURRENT_SOURCE_DIR}/doc/cfgtoollib_ru.md
		OUTPUT		${CMAKE_CURRENT_BINARY_DIR}/cfgtool_ru.hlb
		IDENT		${YALIBCFG_K_IDENT}RU
		DESTINATION	${CMAKE_INSTALL_DATADIR}/help/ru)

	unset(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME)
else()
	message(STATUS "yaLIBCFG: no HELP package - the help libraries of CFGTOOL are not built")
endif()

#
#	Installation: the library as a CMake package (find_package(yaLIBCFG)), the headers, the
#	manuals - the component "dev"; CFGTOOL and its help libraries - the component "tools".
#	Only for a top-level build: a project that add_subdirectory()es this one wants the
#	target, not our installation and packaging.
#
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)

install(TARGETS yalibcfg EXPORT yaLIBCFGTargets
	ARCHIVE	DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT dev
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(FILES include/yalibcfg.h include/yalibcfgdef.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} COMPONENT dev)

install(FILES doc/yalibcfg_manual.md doc/yalibcfg_manual_ru.md README.md LICENSE NOTICE
	DESTINATION ${CMAKE_INSTALL_DOCDIR} COMPONENT dev)

install(EXPORT yaLIBCFGTargets FILE yaLIBCFGTargets.cmake NAMESPACE yaLIBCFG::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yaLIBCFG COMPONENT dev)

configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/yaLIBCFGConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/yaLIBCFGConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yaLIBCFG)

write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/yaLIBCFGConfigVersion.cmake
	VERSION ${PROJECT_VERSION} COMPATIBILITY SameMajorVersion)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/yaLIBCFGConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/yaLIBCFGConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/yaLIBCFG COMPONENT dev)

install(TARGETS cfgtool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT tools)

#
#	The Debian packages (cpack -G DEB in the build directory, configured with
#	-DCMAKE_INSTALL_PREFIX=/usr):
#
#	  libyalibcfg-dev_<version>_<arch>.deb	the static library, the headers, the CMake package, the
#						manuals; depends on libstarlet-dev
#	  cfgtool_<version>_<arch>.deb		CFGTOOL (StarLet and the library linked in statically -
#						it needs only the C library) and its help libraries
#
#	The version is that of the head module: X.01-01 gives 1.1.0.
#
set(CPACK_PACKAGE_NAME			"yalibcfg")
set(CPACK_PACKAGE_VERSION		${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT		"StarLet Squad and Ruslan R. Laishev <polylab240@gmail.com>")
set(CPACK_PACKAGE_VENDOR		"StarLet Squad")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY	"yaLIBCFG - configuration trees in the libconfig syntax")
set(CPACK_GENERATOR			"DEB")
set(CPACK_DEB_COMPONENT_INSTALL		ON)
set(CPACK_COMPONENTS_ALL		dev tools)
set(CPACK_DEBIAN_FILE_NAME		DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE	"https://github.com/SysMan-One/yaLIBCFG")
set(CPACK_DEBIAN_PACKAGE_PRIORITY	optional)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION ON)

set(CPACK_DEBIAN_DEV_PACKAGE_NAME	"libyalibcfg-dev")
set(CPACK_DEBIAN_DEV_PACKAGE_SECTION	libdevel)
set(CPACK_DEBIAN_DEV_PACKAGE_DEPENDS	"libstarlet-dev (>= 1.7)")
set(CPACK_COMPONENT_DEV_DESCRIPTION	"yaLIBCFG - configuration trees in the libconfig syntax (development files)
 A C library that reads libconfig-syntax files into a tree, writes them back
 losslessly (comments and layout kept), validates them by a schema and compares
 two configurations, with an OpenVMS-style listing.")

set(CPACK_DEBIAN_TOOLS_PACKAGE_NAME	"cfgtool")
set(CPACK_DEBIAN_TOOLS_PACKAGE_SECTION	utils)
set(CPACK_DEBIAN_TOOLS_PACKAGE_SHLIBDEPS ON)
set(CPACK_COMPONENT_TOOLS_DESCRIPTION	"CFGTOOL - check, format, compare, read and change libconfig-syntax files
 The command-line utility of yaLIBCFG, with a DCL-style command line:
 CFGTOOL CHECK, FORMAT, DIFF, GET, SET, DELETE.")

include(CPack)

endif()

#
#	The examples of the manual - built and run with the tests, so the code of the
#	manual is code that works
#
option(YALIBCFG_M_EXAMPLES "Build the examples of the manual" ON)

if(YALIBCFG_M_EXAMPLES)
	foreach(YALIBCFG_K_EX ex1_read ex2_edit ex3_walk ex4_list ex5_diff ex6_schema)
		add_executable(${YALIBCFG_K_EX} examples/${YALIBCFG_K_EX}.c)
		target_link_libraries(${YALIBCFG_K_EX} PRIVATE yalibcfg)
	endforeach()
endif()

#
#	Tests
#
option(YALIBCFG_M_TESTS "Build the tests" ON)

if(YALIBCFG_M_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()
