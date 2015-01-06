SET( PRG_NAME "scl" )
SET(MODULES_PATH "/etc/scl/modulefiles" )
SET(MODULE_CMD "/usr/bin/modulecmd" )
SET(CONF_DIR "/etc/scl/conf/" )
CONFIGURE_FILE( config.h.cmake config.h )

SET( CMAKE_C_FLAGS "-Wall -pedantic --std=gnu99 -D_GNU_SOURCE -g" )
INCLUDE_DIRECTORIES ("${PROJECT_BINARY_DIR}/src")
list(APPEND SOURCES scl.c debug.c scllib.c lib_common.c args.c sclmalloc.c fallback.c)
ADD_EXECUTABLE (scl ${SOURCES})
INSTALL(TARGETS scl RUNTIME DESTINATION bin LIBRARY DESTINATION lib ARCHIVE
    DESTINATION lib)
FILE(GLOB helpers "helpers/*")
INSTALL(PROGRAMS ${helpers} DESTINATION bin)

link_directories (/lib64/)
target_link_libraries (scl librpm.so)
target_link_libraries (scl librpmio.so)
