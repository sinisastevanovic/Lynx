file(GLOB_RECURSE GAME_SOURCES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

add_library(game STATIC ${GAME_SOURCES})

target_include_directories(game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(game PUBLIC engine)