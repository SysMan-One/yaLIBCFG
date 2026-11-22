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

message(STATUS "yaLIBCFG: ${YALIBCFG_K_IDENT} -> ${PROJECT_VERSION}")

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
	src/cfg_write.c)

target_include_directories(yalibcfg
	PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>
	PRIVATE	${CMAKE_CURRENT_SOURCE_DIR}/src)

target_compile_options(yalibcfg PRIVATE -Wall -Wextra -Wno-unused-parameter)
target_link_libraries(yalibcfg PUBLIC StarLet::starlet Threads::Threads m)

install(TARGETS yalibcfg ARCHIVE DESTINATION lib)
install(FILES include/yalibcfg.h include/yalibcfgdef.h DESTINATION include)

#
#	Tests
#
option(YALIBCFG_M_TESTS "Build the tests" ON)

if(YALIBCFG_M_TESTS)
	enable_testing()
	add_subdirectory(test)
endif()
