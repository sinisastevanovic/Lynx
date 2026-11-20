// Compiled into the standalone executable of every game by lynx_add_game.
//
//   <game> [<project file>] [--profiler] [--bench=<file> [--bench-out=<folder>] [--bench-set=<name>=<number>]...]
//
// --profiler shows the timing overlay from the start. --bench runs a benchmark and exits, see Lynx/Profiling/Benchmark.h.

#include <Lynx/Core/CommandLine.h>
#include <Lynx/Engine.h>
#include <Lynx/GameModule.h>

#include <iostream>

int main(int argc, char** argv)
{
    const Lynx::CommandLine commandLine(argc, argv);

    const auto project = Lynx::Project::LoadFromCommandLine(commandLine);
    if (!project)
    {
        std::cerr << project.error() << std::endl;
        return 1;
    }

    Lynx::Engine engine;
    engine.Initialize(*project);
    engine.SetProfilerOverlayVisible(commandLine.Has("profiler"));

    auto benchmark = Lynx::BenchmarkSettings::FromCommandLine(commandLine, project->RootDirectory, engine.GetContentPaths().TempDirectory() / "bench");
    if (!benchmark)
    {
        std::cerr << benchmark.error() << std::endl;
        engine.Shutdown();
        return 1;
    }

    const bool isBenchmark = benchmark->has_value();
    if (isBenchmark)
        engine.SetBenchmark(std::move(**benchmark));

    Lynx::IGameModule* gameModule = LynxCreateGameModule();
    engine.SetGameModule(gameModule);

    const bool isPlaying = engine.PlayStartScene();
    if (isPlaying)
        engine.Run();

    // Read before Shutdown: a benchmark that did not reach its end, or could not write its results, is a failure.
    const bool benchmarkFailed = isBenchmark && !engine.GetBenchmark()->Succeeded();

    engine.Shutdown();
    LynxDestroyGameModule(gameModule);
    return isPlaying && !benchmarkFailed ? 0 : 1;
}
