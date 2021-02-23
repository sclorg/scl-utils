ENABLE_TESTING()
INCLUDE_DIRECTORIES(. ../src ${PROJECT_BINARY_DIR}/src )
SET( CMAKE_C_FLAGS "-Wall -pedantic --std=gnu99 -D_GNU_SOURCE -g -Wl,--wrap=get_command_output -Wl,--wrap=system,--wrap=putenv,--wrap=getenv" )


SET(tested_sources ../src/scllib.c ../src/sclmalloc.c ../src/lib_common.c ../src/debug.c ../src/fallback.c)
SET(testing_sources test_scllib.c test_common.c dict.c)
ADD_EXECUTABLE(test_scllib ${testing_sources} ${tested_sources})
TARGET_LINK_LIBRARIES(test_scllib libcmocka.so)
TARGET_LINK_LIBRARIES(test_scllib librpm.so librpmio.so)
ADD_TEST(test_scllib ${CMAKE_CURRENT_BINARY_DIR}/test_scllib)

SET(tested_sources ../src/args.c ../src/sclmalloc.c ../src/lib_common.c ../src/debug.c)
SET(testing_sources test_args.c test_common.c)
ADD_EXECUTABLE(test_args ${testing_sources} ${tested_sources})
TARGET_LINK_LIBRARIES(test_args libcmocka.so)
ADD_TEST(test_args ${CMAKE_CURRENT_BINARY_DIR}/test_args)

# FILE(INSTALL test_build.sh DESTINATION . FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
# FILE(INSTALL SRPMS RPMS DESTINATION .)
# ADD_TEST(test_build ${CMAKE_CURRENT_BINARY_DIR}/test_build.sh)
