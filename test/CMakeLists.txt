#
#	yaLIBCFG tests
#
set(YALIBCFG_K_CORPUS ${CMAKE_CURRENT_SOURCE_DIR}/corpus)

foreach(YALIBCFG_K_T t_roundtrip t_edit t_errors)
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

add_test(NAME roundtrip COMMAND t_roundtrip ${YALIBCFG_K_FILES} ${YALIBCFG_K_CORPUS}/ext.cfg)
add_test(NAME edit COMMAND t_edit)
target_include_directories(t_errors PRIVATE ${CMAKE_SOURCE_DIR}/src)
add_test(NAME errors COMMAND t_errors)

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
