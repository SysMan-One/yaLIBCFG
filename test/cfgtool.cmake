#
#	CFGTOOL: every command on copies of the sample files, the output and the exit status
#	checked.  Run by ctest as cmake -DTOOL=... -DSRCDIR=... -DWORK=... -P cfgtool.cmake
#
#	A cross build: the emulator to start the program with ('|'-joined, see CMakeLists.txt)
string(REPLACE "|" ";" EMU "${EMU}")

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})
file(COPY ${SRCDIR}/router.cfg ${SRCDIR}/router-new.cfg ${SRCDIR}/router-bad.cfg DESTINATION ${WORK})

#	RUN(<want exit> <want stdout or IGNORE> <args...>)
function(RUN WANTRC WANTOUT)
	execute_process(COMMAND ${EMU} ${TOOL} ${ARGN} WORKING_DIRECTORY ${WORK}
		OUTPUT_VARIABLE OUT ERROR_VARIABLE ERR RESULT_VARIABLE RC)

	if(NOT RC EQUAL WANTRC)
		message(FATAL_ERROR "cfgtool ${ARGN}: exit ${RC}, want ${WANTRC}\n--- stdout\n${OUT}--- stderr\n${ERR}")
	endif()

	if(NOT WANTOUT STREQUAL "IGNORE" AND NOT OUT STREQUAL WANTOUT)
		message(FATAL_ERROR "cfgtool ${ARGN}: stdout differs\n--- want\n${WANTOUT}--- got\n${OUT}---")
	endif()

	set(LASTERR "${ERR}" PARENT_SCOPE)
endfunction()

#	CHECK
RUN(0 "" check router.cfg)
RUN(0 "a0f0b155-8348-5765-97a7-4dfb5164faba\n" check router.cfg /fingerprint)
RUN(0 "a0f0b155-8348-5765-97a7-4dfb5164faba\n" check router.cfg --fingerprint)
file(WRITE ${WORK}/broken.cfg "a = 1;\nb = ;\n")
RUN(2 "" check broken.cfg)
if(NOT LASTERR MATCHES "%CFG-E-SYNTAX, syntax error: expected a value, found ';', broken.cfg line 2 column 5")
	message(FATAL_ERROR "check broken.cfg: the diagnostic is wrong:\n${LASTERR}")
endif()
RUN(2 "" check no-such-file.cfg)

#	GET
RUN(0 "9216\n" get router.cfg ports.[id=9].mtu)
RUN(0 "edge-1\n" get router.cfg system.hostname)
RUN(0 "[ 1, 100, 200 ]\n" get router.cfg vlans)
RUN(0 "{\n\thostname = \"edge-1\";\n\tlocation = \"Lab, rack 3\";\n}\n" get router.cfg system)
RUN(0 "{ hostname = \"edge-1\"; location = \"Lab, rack 3\"; }\n" get router.cfg system /flat)
RUN(0 "edge-1\n" get router.cfg SYSTEM.HOSTNAME /nocase)
RUN(2 "" get router.cfg no.such)

#	DIFF
RUN(1 IGNORE diff router.cfg router-new.cfg /notime)
file(READ ${SRCDIR}/expected/ex5_diff.out WANT)
RUN(1 "${WANT}" diff router.cfg router-new.cfg /notime)
RUN(0 IGNORE diff router.cfg router.cfg /notime)
RUN(1 "" diff router.cfg router-new.cfg /notime /output=listing.txt)
file(READ ${WORK}/listing.txt GOT)
if(NOT GOT STREQUAL WANT)
	message(FATAL_ERROR "diff /output: the file differs")
endif()

#	FORMAT: the canonical text reads back to the same settings
RUN(0 "" format router.cfg /output=canon.cfg)
RUN(0 "a0f0b155-8348-5765-97a7-4dfb5164faba\n" check canon.cfg /fingerprint)

#	SET, DELETE: in place, the comments kept
RUN(0 "" set router.cfg ports.[id=9].mtu 1500)
RUN(0 "1500\n" get router.cfg ports.[id=9].mtu)
RUN(0 "" set router.cfg system.snmp.community public)
RUN(0 "public\n" get router.cfg system.snmp.community)
RUN(0 "" set router.cfg system.snmp.port 0x00A1)
RUN(0 "0xA1\n" get router.cfg system.snmp.port)
RUN(0 "" set router.cfg system.location 1234 /type=string)
RUN(0 "{ hostname = \"edge-1\"; location = \"1234\"; snmp = { community = \"public\"; port = 0xA1; }; }\n" get router.cfg system /flat)
RUN(2 "" set router.cfg ports.[id=9].mtu not-a-number)
RUN(0 "" delete router.cfg ports.[id=10])
RUN(2 "" delete router.cfg ports.[id=10])
file(READ ${WORK}/router.cfg GOT)
if(NOT GOT MATCHES "# The ports of the switch" OR NOT GOT MATCHES "# shown in the prompt" OR GOT MATCHES "id = 10")
	message(FATAL_ERROR "set/delete: comments lost or port 10 still there:\n${GOT}")
endif()

#	SET /OUTPUT leaves the file as it was
file(COPY ${SRCDIR}/router.cfg DESTINATION ${WORK}/orig)
RUN(0 "" set orig/router.cfg system.hostname edge-2 /output=out.cfg)
RUN(0 "edge-1\n" get orig/router.cfg system.hostname)
RUN(0 "edge-2\n" get out.cfg system.hostname)

#	HELP, errors of the command line
RUN(0 IGNORE help)
RUN(2 "" bogus)
RUN(2 "" get router.cfg)
