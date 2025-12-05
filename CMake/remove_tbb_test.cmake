# Read the file
file(READ "${SRC}" CONTENTS)

# Replace the TBB_BUILD_TESTS option line with OFF
string(REGEX REPLACE
       "option\\(TBB_BUILD_TESTS[^\n]*\\)"
       ""
       NEW_CONTENTS
       "${CONTENTS}")

# Write the patched file back
file(WRITE "${SRC}" "${NEW_CONTENTS}")

message(STATUS "Patched TBB CMakeLists.txt: disabled TBB_BUILD_TESTS")