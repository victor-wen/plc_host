# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "")
  file(REMOVE_RECURSE
  "src/CMakeFiles/plc_host_autogen.dir/AutogenUsed.txt"
  "src/CMakeFiles/plc_host_autogen.dir/ParseCache.txt"
  "src/plc_host_autogen"
  "tests/CMakeFiles/tst_DatabaseMigrations_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/tst_DatabaseMigrations_autogen.dir/ParseCache.txt"
  "tests/CMakeFiles/tst_DomainModels_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/tst_DomainModels_autogen.dir/ParseCache.txt"
  "tests/CMakeFiles/tst_TagCache_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/tst_TagCache_autogen.dir/ParseCache.txt"
  "tests/CMakeFiles/tst_WriteQueue_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/tst_WriteQueue_autogen.dir/ParseCache.txt"
  "tests/CMakeFiles/tst_smoke_autogen.dir/AutogenUsed.txt"
  "tests/CMakeFiles/tst_smoke_autogen.dir/ParseCache.txt"
  "tests/tst_DatabaseMigrations_autogen"
  "tests/tst_DomainModels_autogen"
  "tests/tst_TagCache_autogen"
  "tests/tst_WriteQueue_autogen"
  "tests/tst_smoke_autogen"
  )
endif()
