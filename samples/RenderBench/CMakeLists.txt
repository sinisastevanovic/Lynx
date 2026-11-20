# The project the renderer is measured with. It holds the benchmark scenes and the files in bench/
# that describe each benchmark. Run them with tools/bench.ps1.

file(GLOB_RECURSE RENDER_BENCH_SOURCES
    CONFIGURE_DEPENDS
    "src/*.cpp"
    "src/*.h"
)

lynx_add_game(RenderBench
    PROJECT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/RenderBench.lxproj"
    SOURCES ${RENDER_BENCH_SOURCES}
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set_target_properties(RenderBenchCode PROPERTIES FOLDER "Samples")
