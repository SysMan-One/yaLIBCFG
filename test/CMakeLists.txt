#
#	yaLIBCFG tests
#
set(YALIBCFG_K_CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus)

foreach(YALIBCFG_K_T t_roundtrip t_edit t_errors t_diff t_schema)
	add_executable(${YALIBCFG_K_T} ${YALIBCFG_K_T}.c)
	target_link_libraries(${YALIBCFG_K_T} PRIVATE yalibcfg)
	target_compile_options(${YALIBCFG_K_T} PRIVATE -Wall -Wno-unused-parameter)
endforeach()

set(YALIBCFG_K_FILES
	${YALIBCFG_K_CORPUS}/basic.cfg
	${YALIBCFG_K_CORPUS}/comments.cfg
	${YALIBCFG_K_CORPUS}/nested.cfg
	${YALIBCFG_K_CORPUS}/strings.cfg
	${YALIBCFG_K_CORPUS}/lists.cfg
	${YALIBCFG_K_CORPUS}/noeol.cfg
	${YALIBCFG_K_CORPUS}/crlf.cfg
	${YALIBCFG_K_CORPUS}/empty.cfg
	${YALIBCFG_K_CORPUS}/blank.cfg
	${YALIBCFG_K_CORPUS}/main.cfg)

add_test(NAME roundtrip COMMAND t_roundtrip ${YALIBCFG_K_FILES} ${YALIBCFG_K_CORPUS}/ext.cfg ${YALIBCFG_K_CORPUS}/inlist.cfg)
add_test(NAME edit COMMAND t_edit)
target_include_directories(t_errors PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_definitions(t_errors PRIVATE CORPUS="${YALIBCFG_K_CORPUS}")
add_test(NAME errors COMMAND t_errors)
add_test(NAME diff COMMAND t_diff)
target_include_directories(t_schema PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME schema COMMAND t_schema)

#
#	The same files through libconfig, when it is there
#
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
	pkg_check_modules(LIBCONFIG QUIET libconfig)
endif()

if(LIBCONFIG_FOUND)
	add_executable(t_libconfig t_libconfig.c)
	target_include_directories(t_libconfig PRIVATE ${LIBCONFIG_INCLUDE_DIRS})
	target_link_libraries(t_libconfig PRIVATE yalibcfg ${LIBCONFIG_LIBRARIES})
	add_test(NAME libconfig COMMAND t_libconfig ${YALIBCFG_K_FILES})
	message(STATUS "yaLIBCFG: libconfig ${LIBCONFIG_VERSION} found - the comparison test is on")
else()
	message(STATUS "yaLIBCFG: libconfig not found - the comparison test is off")
endif()

#
#	The scripts below start programs themselves: a cross build hands them its emulator
#	(qemu-<arch>-static and its arguments), joined by '|' - a ';' would split the -D
#
string(REPLACE ";" "|" YALIBCFG_K_EMU "${CMAKE_CROSSCOMPILING_EMULATOR}")

#
#	The examples of the manual, on a copy of the sample file, against the expected output
#
if(YALIBCFG_M_EXAMPLES)
	add_test(NAME examples COMMAND ${CMAKE_COMMAND}
		-DEXDIR=${CMAKE_BINARY_DIR} -DSRCDIR=${CMAKE_SOURCE_DIR}/examples -DWORK=${CMAKE_CURRENT_BINARY_DIR}/ex-work
		"-DEMU=${YALIBCFG_K_EMU}"
		-P ${CMAKE_CURRENT_SOURCE_DIR}/examples.cmake)
endif()

#
#	CFGTOOL on the sample files of the examples
#
add_test(NAME cfgtool COMMAND ${CMAKE_COMMAND}
	-DTOOL=$<TARGET_FILE:cfgtool> -DSRCDIR=${CMAKE_SOURCE_DIR}/examples -DWORK=${CMAKE_CURRENT_BINARY_DIR}/cfgtool-work
	"-DEMU=${YALIBCFG_K_EMU}"
	-P ${CMAKE_CURRENT_SOURCE_DIR}/cfgtool.cmake)
