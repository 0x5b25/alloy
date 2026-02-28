# A small utility function which generates a C-header from an input file
#This intends to be a script called in add_custom_command
function(FILE_TO_C_STRING FILE_IN FILE_OUT VARIABLE_NAME ZERO_TERMINATED)
    FILE(READ "${FILE_IN}" HEX_INPUT HEX)
    if (${ZERO_TERMINATED})
        string(APPEND HEX_INPUT "00")
    endif()

    string(REGEX REPLACE "(....)" "\\1\n" HEX_OUTPUT ${HEX_INPUT})
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," HEX_OUTPUT ${HEX_OUTPUT})

    set(HEX_OUTPUT "static constexpr uint8_t ${VARIABLE_NAME}[] = {\n${HEX_OUTPUT}\n};\n")

    file(WRITE "${FILE_OUT}" "${HEX_OUTPUT}")
endfunction()

# message("Create header file for ${FILE_IN}")
# example usage:
#file_to_c_string(binaryFile.bin embedded.h binaryFileContent ZERO_TERMINATED)


file_to_c_string(${FILE_IN} ${FILE_OUT} ${VARIABLE_NAME} FALSE)
