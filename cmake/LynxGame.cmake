# Builds a Lynx game from its source files.
#
#   lynx_add_game(<name>
#       PROJECT_FILE <path to the .lxproj file of the game>
#       SOURCES <file>...
#       [INCLUDE_DIRECTORIES <dir>...])
#
# Creates three targets:
#   <name>Code    static library holding the code of the game
#   <name>Module  shared library the editor loads. Name it as "GameModule" in the .lxproj file.
#   <name>        the standalone game executable
#
# One source file of the game must contain LX_GAME_MODULE(<class>), see Lynx/GameModule.h.
# Starting the editor or the game from the IDE opens PROJECT_FILE.

set(LYNX_LAUNCH_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../engine/launch" CACHE INTERNAL "Sources of the launchers that lynx_add_game compiles into each game")

function(lynx_add_game name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "PROJECT_FILE" "SOURCES;INCLUDE_DIRECTORIES")

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "lynx_add_game(${name}): SOURCES is required")
    endif()
    if(NOT ARG_PROJECT_FILE)
        message(FATAL_ERROR "lynx_add_game(${name}): PROJECT_FILE is required")
    endif()

    get_filename_component(project_file "${ARG_PROJECT_FILE}" ABSOLUTE)
    if(NOT EXISTS "${project_file}")
        message(FATAL_ERROR "lynx_add_game(${name}): project file not found: ${project_file}")
    endif()

    add_library(${name}Code STATIC ${ARG_SOURCES})
    target_include_directories(${name}Code PUBLIC ${ARG_INCLUDE_DIRECTORIES})
    target_link_libraries(${name}Code PUBLIC engine)
    if(MSVC)
        target_compile_options(${name}Code PRIVATE /MP)
    endif()

    add_library(${name}Module SHARED "${LYNX_LAUNCH_SOURCE_DIR}/GameModuleExports.cpp")
    target_link_libraries(${name}Module PRIVATE ${name}Code)

    add_executable(${name} "${LYNX_LAUNCH_SOURCE_DIR}/StandaloneMain.cpp")
    target_link_libraries(${name} PRIVATE ${name}Code)
    set_target_properties(${name} PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "\"${project_file}\"")

    # The editor finds the module next to its own executable, and the game finds the engine DLL the same way.
    get_target_property(engine_output_dir engine RUNTIME_OUTPUT_DIRECTORY)
    set_target_properties(${name}Code ${name}Module ${name} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${engine_output_dir}"
        LIBRARY_OUTPUT_DIRECTORY "${engine_output_dir}"
        RUNTIME_OUTPUT_DIRECTORY "${engine_output_dir}"
    )

    # The game is developed in <name>Code. The other two are thin launchers around it.
    set_target_properties(${name}Module ${name} PROPERTIES FOLDER "Launchers")

    # The editor loads the module at run time, so nothing links the two together.
    # The dependency makes building the editor also rebuild the game it is about to open.
    if(TARGET editor)
        add_dependencies(editor ${name}Module)
        set_target_properties(editor PROPERTIES VS_DEBUGGER_COMMAND_ARGUMENTS "\"${project_file}\"")
    endif()
endfunction()
