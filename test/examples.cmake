#
#	Run the examples of the manual on a copy of examples/router.cfg and compare what they
#	print and what they write with examples/expected/*.
#
file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})

foreach(EX ex1_read ex3_walk ex4_list ex2_edit)
	file(COPY ${SRCDIR}/router.cfg DESTINATION ${WORK})
	execute_process(COMMAND ${EXDIR}/${EX} ${WORK}/router.cfg OUTPUT_VARIABLE OUT ERROR_VARIABLE ERR RESULT_VARIABLE RC)

	if(NOT RC EQUAL 0)
		message(FATAL_ERROR "${EX}: exit ${RC}\n${ERR}")
	endif()

	if(EX STREQUAL "ex2_edit")
		file(READ ${WORK}/router.cfg OUT)
	endif()

	file(READ ${SRCDIR}/expected/${EX}.out WANT)

	if(NOT OUT STREQUAL WANT)
		message(FATAL_ERROR "${EX}: output differs\n--- want\n${WANT}--- got\n${OUT}---")
	endif()
endforeach()

#	ex5_diff: run in the directory of the examples, with relative names, without times
execute_process(COMMAND ${EXDIR}/ex5_diff router.cfg router-new.cfg notime WORKING_DIRECTORY ${SRCDIR}
	OUTPUT_VARIABLE OUT ERROR_VARIABLE ERR RESULT_VARIABLE RC)

if(NOT RC EQUAL 1)
	message(FATAL_ERROR "ex5_diff: exit ${RC}, want 1 (the files differ)\n${ERR}")
endif()

file(READ ${SRCDIR}/expected/ex5_diff.out WANT)

if(NOT OUT STREQUAL WANT)
	message(FATAL_ERROR "ex5_diff: output differs\n--- want\n${WANT}--- got\n${OUT}---")
endif()

#	ex6_schema: a valid file and one with mistakes; the lines of the messages carry the time and
#	are not compared - only the lines the example prints itself
foreach(PAIR "router.cfg;0" "router-bad.cfg;1")
	list(GET PAIR 0 F)
	list(GET PAIR 1 WANTRC)
	execute_process(COMMAND ${EXDIR}/ex6_schema ${F} WORKING_DIRECTORY ${SRCDIR}
		OUTPUT_VARIABLE OUT ERROR_VARIABLE ERR RESULT_VARIABLE RC)

	if(NOT RC EQUAL WANTRC)
		message(FATAL_ERROR "ex6_schema ${F}: exit ${RC}, want ${WANTRC}\n${ERR}")
	endif()

	string(REGEX REPLACE "[^\n]*%CFG-[^\n]*\n" "" OUT "${OUT}")
	string(APPEND OUT6 "${OUT}")
endforeach()

file(READ ${SRCDIR}/expected/ex6_schema.out WANT)

if(NOT OUT6 STREQUAL WANT)
	message(FATAL_ERROR "ex6_schema: output differs\n--- want\n${WANT}--- got\n${OUT6}---")
endif()
