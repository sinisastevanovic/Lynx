file(GLOB_RECURSE GAME_SOURCES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

lynx_add_game(MyGame
    PROJECT_FILE "${CMAKE_SOURCE_DIR}/MyGame.lxproj"
    SOURCES ${GAME_SOURCES}
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src"
)
