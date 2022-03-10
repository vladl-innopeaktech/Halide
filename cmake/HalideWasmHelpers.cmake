function(add_wasm_executable TARGET)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs SRCS DEPS INCLUDES ENABLE_IF)
    cmake_parse_arguments(args "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (args_ENABLE_IF AND NOT (${args_ENABLE_IF}))
        return()
    endif ()

    # Conceptually, we want something like this:
    # add_executable(${TARGET} ${args_SRCS})
    # if (args_INCLUDES)
    #     target_include_directories("${TARGET}" PRIVATE ${args_INCLUDES})
    # endif()
    # if (args_DEPS)
    #     target_link_libraries(${TARGET} PRIVATE ${args_DEPS})
    # endif ()

    find_program(EMCC emcc HINTS "$ENV{EMSDK}/upstream/emscripten")

    if (NOT EMCC)
        message(FATAL_ERROR "Building tests or apps for WASM requires that EMSDK point to a valid Emscripten install.")
    endif ()

    # TODO: this is currently hardcoded to settings that are sensible for most of Halide's
    # internal purposes. Consider adding ways to customize this as appropriate.
    set(EMCC_FLAGS
        -O3
        -g
        -std=c++17
        -Wall
        -Wcast-qual
        -Werror
        -Wignored-qualifiers
        -Wno-comment
        -Wno-psabi
        -Wno-unknown-warning-option
        -Wno-unused-function
        -Wsign-compare
        -Wsuggest-override
        -s ASSERTIONS=1
        -s ALLOW_MEMORY_GROWTH=1
        -s ENVIRONMENT=node
        )

    set(SRCS)
    foreach (S IN LISTS args_SRCS)
        list(APPEND SRCS "${CMAKE_CURRENT_SOURCE_DIR}/${S}")
    endforeach ()

    set(INCLUDES)
    foreach (I IN LISTS args_INCLUDES)
        list(APPEND INCLUDES "-I${I}")
    endforeach ()

    set(DEPS)
    foreach (D IN LISTS args_DEPS)
        list(APPEND DEPS $<TARGET_FILE:${D}>)
    endforeach ()

    add_custom_command(OUTPUT "${TARGET}.wasm" "${TARGET}.js"
                       COMMAND ${EMCC} ${EMCC_FLAGS} ${INCLUDES} ${SRCS} ${DEPS} -o "${TARGET}.js"
                       DEPENDS ${SRCS} ${DEPS}
                       VERBATIM)

    add_custom_target("${TARGET}" ALL
                      DEPENDS "${TARGET}.wasm" "${TARGET}.js")

endfunction()

function(add_wasm_halide_test TARGET)
    set(options)
    set(oneValueArgs)
    set(multiValueArgs GROUPS ENABLE_IF)
    cmake_parse_arguments(args "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if (args_ENABLE_IF AND NOT (${args_ENABLE_IF}))
        return()
    endif ()

    add_halide_test("${TARGET}"
                    GROUPS ${args_GROUPS}
                    COMMAND ${NODE_JS_EXECUTABLE} "${TARGET}.js")
endfunction()
