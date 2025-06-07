set(TARGET_NAME ysflight_graphics_common)
set(LIB_DEPENDENCY ysflight_common ysflight_filename fsguilib geblkernel geblgl ystexturemanager)

# Don't link fssimplewindow. It can be fssimplenowindow. Just add include directory

# --- GLEW Integration ---
# 1. Find GLEW package
find_package(GLEW REQUIRED) # Corrected: find_package, not findpackage

set(SRCS
    fswirefont.cpp
    fsvisual.cpp
    fsblackout.cpp
    fsfontrenderer.cpp
    fsconsole.cpp
    fsopengl.cpp
    fstexturemanager.cpp
    fswirefont_makebuf.cpp
    fsculling.cpp
)

set(HEADERS
    fsopengl.h
    fswirefont.h
    fsvisual.h
    fsblackout.h
    fsfontrenderer.h
    fsconsole.h
    fstexturemanager.h
    fsculling.h
)

add_library(${TARGET_NAME} ${SRCS} ${HEADERS}) # Headers here help IDEs list them

# --- Link Dependencies ---
target_link_libraries(${TARGET_NAME}
    PUBLIC # Or PRIVATE/INTERFACE depending on usage
        ${LIB_DEPENDENCY}
    PRIVATE # GLEW is often an implementation detail, but if its types appear in your public headers, use PUBLIC
        GLEW::glew # Modern CMake way to link GLEW. Handles includes too.
                   # If using GLEW static, this target should be GLEW::glew_s or similar,
                   # or GLEW::glew will adapt if GLEW_STATIC is defined.
)

# --- Include Directories ---
target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR} # So users of this lib can find its headers
        ${FSSIMPLEWINDOW_INCLUDE_DIRS} # If fssimplewindow headers are needed by users of this lib
    #PRIVATE # If fssimplewindow is only an internal detail
    #   ${FSSIMPLEWINDOW_INCLUDE_DIRS}
)

# --- GLEW_STATIC preprocessor definition ---
# The vcpkg toolchain and -DGLEW_STATIC=ON in your GH Actions CMake call
# should make GLEW::glew link statically and define GLEW_STATIC.
# However, if you find that GLEW_STATIC is not being defined for your target's sources,
# you might need to add it explicitly, especially if not using GLEW::glew target.
# Check if GLEW::glew target exists and is correctly configured by vcpkg.
# If GLEW::glew correctly sets the define, this is not needed.
if(WIN32 AND GLEW_STATIC) # GLEW_STATIC is the CMake variable you set in GH Actions
    target_compile_definitions(${TARGET_NAME} PRIVATE GLEW_STATIC)
endif()

# For older CMake versions or if GLEW::glew target isn't available/working as expected:
# if(GLEW_FOUND)
#   target_include_directories(${TARGET_NAME} PRIVATE ${GLEW_INCLUDE_DIRS})
#   target_link_libraries(${TARGET_NAME} PRIVATE ${GLEW_LIBRARIES})
#   if(WIN32 AND GLEW_STATIC) # GLEW_STATIC is the CMake variable
#       target_compile_definitions(${TARGET_NAME} PRIVATE GLEW_STATIC)
#   endif()
# endif()
