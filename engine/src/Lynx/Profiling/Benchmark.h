#pragma once

#include "Lynx/Core.h"
#include "Lynx/Renderer/Screenshot.h"

#include <glm/glm.hpp>

#include <expected>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Lynx
{
    class AssetManager;
    class CommandLine;
    class FrameProfiler;
    class Renderer;
    class Scene;

    // Describes one benchmark: which scene to play, how the camera moves and what to measure.
    // Read from a JSON file, so a benchmark can be changed without building.
    struct LX_API BenchmarkSettings
    {
        struct CameraKey
        {
            uint32_t Frame = 0;
            glm::vec3 Position{ 0.0f };
            glm::vec3 Target{ 0.0f, 0.0f, -1.0f };
        };

        std::string Name;

        // Relative to the project's asset directory. Empty plays the project's start scene.
        std::filesystem::path Scene;

        uint32_t Width = 1920;
        uint32_t Height = 1080;

        // Frames rendered before measuring starts. Measuring also waits for every asset to finish loading.
        uint32_t WarmupFrames = 60;
        uint32_t Frames = 600;

        // Every measured frame advances the game by this much, whatever the real frame time is.
        // That makes frame N the same picture in every run.
        float FixedDeltaTime = 1.0f / 60.0f;

        float FieldOfViewDegrees = 60.0f;
        float NearClip = 0.1f;
        float FarClip = 1000.0f;

        // The camera moves on a smooth curve through these, sorted by frame. Empty leaves the scene's camera alone.
        std::vector<CameraKey> CameraPath;

        // Measured frames to save as pictures.
        std::vector<uint32_t> ScreenshotFrames;

        // Free-form numbers for the game, for example how many objects to create.
        std::map<std::string, double, std::less<>> Parameters;

        // Where the results are written. Absolute.
        std::filesystem::path OutputDirectory;

        double Parameter(std::string_view name, double fallback) const;

        // Returns a message describing the problem on failure.
        static std::expected<BenchmarkSettings, std::string> Load(const std::filesystem::path& file);

        // Reads the benchmark named by the command line: "--bench=<file>", relative to projectRoot unless absolute.
        // "--bench-out=<folder>" chooses where the results go, and "--bench-set=<name>=<number>" overrides a parameter.
        // Returns no settings if the command line asks for no benchmark.
        static std::expected<std::optional<BenchmarkSettings>, std::string> FromCommandLine(const CommandLine& commandLine, const std::filesystem::path& projectRoot, const std::filesystem::path& defaultOutputRoot);
    };

    // Runs a benchmark frame by frame: warms up, moves the camera, records timings, saves pictures
    // and writes the results when the last frame is done.
    class LX_API Benchmark
    {
    public:
        explicit Benchmark(BenchmarkSettings settings);
        ~Benchmark();

        const BenchmarkSettings& Settings() const { return m_Settings; }

        // Call once at the start of every frame, before the scene is updated.
        // Returns how far the game should advance in this frame, in seconds.
        float BeginFrame(Scene& scene, Renderer& renderer, AssetManager& assetManager, FrameProfiler& profiler);

        bool IsFinished() const { return m_Phase == Phase::Finished; }

        // True once the results have been written without a problem.
        bool Succeeded() const { return m_Succeeded; }

    private:
        enum class Phase
        {
            Warmup,
            Measure,
            Finish,
            Finished
        };

        void PlaceCamera(Scene& scene, Renderer& renderer, uint32_t frame);
        void KeepScreenshots(std::vector<Screenshot> screenshots);
        bool WriteResults(Renderer& renderer, const FrameProfiler& profiler);

    private:
        BenchmarkSettings m_Settings;
        Phase m_Phase = Phase::Warmup;
        uint32_t m_WarmupFramesDone = 0;
        uint32_t m_Frame = 0;
        bool m_Succeeded = false;

        // Screenshots are kept in memory and encoded after the last frame, so that saving them costs no measured time.
        std::vector<Screenshot> m_Screenshots;
    };
}
