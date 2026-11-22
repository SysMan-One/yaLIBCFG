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

#	SET /TYPE=AUTO: what is not exactly one scalar is a string; a setting keeps its type
RUN(0 "" set router.cfg syslog 10.0.0.5)
RUN(0 "10.0.0.5\n" get router.cfg syslog)
RUN(0 "" set router.cfg note "12 # not a comment")
RUN(0 "12 # not a comment\n" get router.cfg note)
RUN(0 "" set router.cfg big 12L)
RUN(0 "12L\n" get router.cfg big)
file(WRITE ${WORK}/t.cfg "f = 1.5;\ns = \"x\";\np = ( { id = 9; } );\n")
RUN(0 "" set t.cfg f 2)
RUN(0 "2.0\n" get t.cfg f)
RUN(0 "" set t.cfg s 12)
RUN(0 "12\n" get t.cfg s)
RUN(0 "" set t.cfg p.[id=9].id 0x20)
RUN(0 "0x20\n" get t.cfg p.[0].id)

#	/OUTPUT refuses a change in an included file - it would be lost
file(WRITE ${WORK}/m.cfg "x = 1;\n@include \"inc.cfg\"\n")
file(WRITE ${WORK}/inc.cfg "inc_a = 5;\n")
RUN(2 "" set m.cfg inc_a 6 /output=m-out.cfg)
if(EXISTS ${WORK}/m-out.cfg)
	message(FATAL_ERROR "set /output wrote a file that lost the change of inc.cfg")
endif()
RUN(0 "" set m.cfg inc_a 6)
RUN(0 "6\n" get m.cfg inc_a)

#	In place through a symbolic link: the link stays, its target changes
file(CREATE_LINK ${WORK}/t.cfg ${WORK}/link.cfg SYMBOLIC)
RUN(0 "" set link.cfg z 1)
RUN(0 "1\n" get t.cfg z)
if(NOT IS_SYMLINK ${WORK}/link.cfg)
	message(FATAL_ERROR "set through a symbolic link replaced the link")
endif()

#	DELETE of the root is refused with its reason; an argument over 255 octets is refused
RUN(2 "" delete t.cfg "")
string(REPEAT "a" 300 LONG)
RUN(2 "" set t.cfg q ${LONG})
RUN(2 "" format t.cfg /output=)

#	/SKIPERRORS: CHECK tells every error and fails; GET and SET work around the bad settings
file(WRITE ${WORK}/bad.cfg "a = 1;\nb = ;\nc = 3;\nd = $;\n")
RUN(2 "" check bad.cfg)
RUN(2 "" check bad.cfg /skiperrors)
if(NOT LASTERR MATCHES "line 2 column 5" OR NOT LASTERR MATCHES "line 4 column 5" OR NOT LASTERR MATCHES "SKIPPED, 2 bad settings skipped")
	message(FATAL_ERROR "check /skiperrors did not tell both errors:\n${LASTERR}")
endif()
RUN(0 "3\n" get bad.cfg c /skiperrors)
RUN(0 "" set bad.cfg c 4 /skiperrors)
file(READ ${WORK}/bad.cfg GOT)
if(NOT GOT STREQUAL "a = 1;\nb = ;\nc = 4;\nd = $;\n")
	message(FATAL_ERROR "set /skiperrors did not keep the bad settings:\n${GOT}")
endif()

#	The radix of DCL and a leading 0: read, set; SET takes the form of the new text
file(WRITE ${WORK}/rdx.cfg "mask = %X1F;\numask = 0027;\n")
RUN(0 "%X1F\n" get rdx.cfg mask)
RUN(0 "027\n" get rdx.cfg umask)
RUN(0 "" set rdx.cfg mask %XFF)
RUN(0 "" set rdx.cfg umask 022)
file(READ ${WORK}/rdx.cfg GOT)
if(NOT GOT STREQUAL "mask = %XFF;\numask = 022;\n")
	message(FATAL_ERROR "the radix forms were not kept:\n${GOT}")
endif()

#	HELP, errors of the command line
RUN(0 IGNORE help)
RUN(2 "" bogus)
RUN(2 "" get router.cfg)
