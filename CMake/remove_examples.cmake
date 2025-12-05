# Read the file
file(READ "${SRC}" CONTENTS)

# Remove the line containing add_subdirectory(examples)
string(REGEX REPLACE "[^\n]*add_subdirectory[ \t]*\\(examples\\)[^\n]*\n" "" NEW_CONTENTS "${CONTENTS}")

# Write the patched file back
file(WRITE "${SRC}" "${NEW_CONTENTS}")
