# the FindSubversion.cmake module is part of the standard distribution
include(FindSubversion)

# extract working copy information for SOURCE_DIR into MY_XXX variables
Subversion_WC_INFO(${SOURCE_DIR} MY)


execute_process(COMMAND  "date" "+%Y%m%d.%H%M" OUTPUT_VARIABLE output OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND uname OUTPUT_VARIABLE _output OUTPUT_STRIP_TRAILING_WHITESPACE)

# write a file with the SVNVERSION define
#file(WRITE svnversion.h.txt "#define SVNVERSION ${MY_WC_REVISION}\n#define COMPILETIME \"${output}\"\n")
file(WRITE svnversion.h "#define SVNVERSION \"${MY_WC_REVISION}.${output}.${_output}\" \n")


# copy the file to the final header only if the version changes
# reduces needless rebuilds
#execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
#                        svnversion.h.txt svnversion.h)