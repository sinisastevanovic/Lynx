#include "Benchmark.h"

#include "FrameProfiler.h"
#include "Lynx/Asset/AssetManager.h"
#include "Lynx/Core/CommandLine.h"
#include "Lynx/Core/ContentPaths.h"
#include "Lynx/Log.h"
#include "Lynx/Renderer/Renderer.h"
#include "Lynx/Scene/Components/Components.h"
#include "Lynx/Scene/Entity.h"
#include "Lynx/Scene/Scene.h"

#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <numeric>

namespace Lynx
{
    namespace
    {
        constexpr const char* CameraEntityName = "Benchmark Camera";

        struct Statistics
        {
            double Median = 0.0;
            double Mean = 0.0;
            double Percentile95 = 0.0;
            double Percentile99 = 0.0;
            double Min = 0.0;
            double Max = 0.0;
        };

        Statistics Summarize(std::vector<double> samples)
        {
            Statistics statistics;
            if (samples.empty())
                return statistics;

            std::ranges::sort(samples);

            // Nearest rank: the smallest sample that at least the given share of samples does not exceed.
            const auto percentile = [&samples](double share)
            {
                const size_t rank = static_cast<size_t>(std::ceil(share * static_cast<double>(samples.size())));
                return samples[std::clamp<size_t>(rank, 1, samples.size()) - 1];
            };

            statistics.Median = percentile(0.50);
            statistics.Percentile95 = percentile(0.95);
            statistics.Percentile99 = percentile(0.99);
            statistics.Min = samples.front();
            statistics.Max = samples.back();
            statistics.Mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());
            return statistics;
        }

        nlohmann::json ToJson(const Statistics& statistics)
        {
            return {
                { "Median", statistics.Median },
                { "Mean", statistics.Mean },
                { "P95", statistics.Percentile95 },
                { "P99", statistics.Percentile99 },
                { "Min", statistics.Min },
                { "Max", statistics.Max }
            };
        }

        nlohmann::json ToJson(const std::vector<FrameProfiler::Series>& series)
        {
            nlohmann::json json = nlohmann::json::object();
            for (const FrameProfiler::Series& entry : series)
                json[entry.Name] = ToJson(Summarize(entry.Samples));
            return json;
        }

        std::expected<glm::vec3, std::string> ReadVector(const nlohmann::json& json, const char* key)
        {
            const auto found = json.find(key);
            if (found == json.end() || !found->is_array() || found->size() != 3)
                return std::unexpected(std::string("\"") + key + "\" has to be an array of three numbers");

            glm::vec3 vector;
            for (int axis = 0; axis < 3; ++axis)
            {
                if (!(*found)[axis].is_number())
                    return std::unexpected(std::string("\"") + key + "\" has to be an array of three numbers");
                vector[axis] = (*found)[axis].get<float>();
            }
            return vector;
        }

        // A curve through b and c that leaves each point in the direction of its neighbours.
        glm::vec3 CatmullRom(const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d, float t)
        {
            const float t2 = t * t;
            const float t3 = t2 * t;
            return 0.5f * ((2.0f * b) + (c - a) * t + (2.0f * a - 5.0f * b + 4.0f * c - d) * t2 + (3.0f * b - a - 3.0f * c + d) * t3);
        }

        BenchmarkSettings::CameraKey SampleCameraPath(const std::vector<BenchmarkSettings::CameraKey>& path, uint32_t frame)
        {
            if (frame <= path.front().Frame)
                return path.front();
            if (frame >= path.back().Frame)
                return path.back();

            size_t next = 1;
            while (path[next].Frame < frame)
                ++next;

            const size_t last = path.size() - 1;
            const BenchmarkSettings::CameraKey& a = path[next >= 2 ? next - 2 : 0];
            const BenchmarkSettings::CameraKey& b = path[next - 1];
            const BenchmarkSettings::CameraKey& c = path[next];
            const BenchmarkSettings::CameraKey& d = path[std::min(next + 1, last)];
            const float t = static_cast<float>(frame - b.Frame) / static_cast<float>(c.Frame - b.Frame);

            BenchmarkSettings::CameraKey sample;
            sample.Frame = frame;
            sample.Position = CatmullRom(a.Position, b.Position, c.Position, d.Position, t);
            sample.Target = CatmullRom(a.Target, b.Target, c.Target, d.Target, t);
            return sample;
        }

        bool WritePng(const std::filesystem::path& file, const Screenshot& screenshot)
        {
            const int stride = static_cast<int>(screenshot.Width) * 4;
            return stbi_write_png(file.string().c_str(), static_cast<int>(screenshot.Width), static_cast<int>(screenshot.Height), 4, screenshot.Rgba.data(), stride) != 0;
        }

        std::string ScreenshotFileName(uint64_t frame)
        {
            return std::format("frame_{:05}.png", frame);
        }
    }

    double BenchmarkSettings::Parameter(std::string_view name, double fallback) const
    {
        const auto found = Parameters.find(name);
        return found != Parameters.end() ? found->second : fallback;
    }

    std::expected<BenchmarkSettings, std::string> BenchmarkSettings::Load(const std::filesystem::path& file)
    {
        std::ifstream stream(file);
        if (!stream)
            return std::unexpected("Cannot open benchmark file: " + file.string());

        const nlohmann::json json = nlohmann::json::parse(stream, nullptr, false);
        if (!json.is_object())
            return std::unexpected("Benchmark file is not a JSON object: " + file.string());

        const auto fail = [&file](const std::string& problem)
        {
            return std::unexpected("Invalid benchmark file " + file.string() + ": " + problem);
        };

        try
        {
            BenchmarkSettings settings;
            settings.Name = json.value("Name", file.stem().string());
            settings.Scene = json.value("Scene", "");
            settings.Width = json.value("Width", settings.Width);
            settings.Height = json.value("Height", settings.Height);
            settings.WarmupFrames = json.value("WarmupFrames", settings.WarmupFrames);
            settings.Frames = json.value("Frames", settings.Frames);
            settings.FixedDeltaTime = json.value("FixedDeltaTime", settings.FixedDeltaTime);
            settings.FieldOfViewDegrees = json.value("FieldOfViewDegrees", settings.FieldOfViewDegrees);
            settings.NearClip = json.value("NearClip", settings.NearClip);
            settings.FarClip = json.value("FarClip", settings.FarClip);
            settings.ScreenshotFrames = json.value("ScreenshotFrames", std::vector<uint32_t>());

            if (settings.Frames == 0 || settings.Width == 0 || settings.Height == 0)
                return fail("\"Frames\", \"Width\" and \"Height\" have to be greater than zero");

            if (const auto path = json.find("CameraPath"); path != json.end())
            {
                if (!path->is_array())
                    return fail("\"CameraPath\" has to be an array");

                for (const nlohmann::json& keyJson : *path)
                {
                    if (!keyJson.is_object())
                        return fail("every entry of \"CameraPath\" has to be an object");

                    const auto position = ReadVector(keyJson, "Position");
                    const auto target = ReadVector(keyJson, "Target");
                    if (!position)
                        return fail(position.error());
                    if (!target)
                        return fail(target.error());

                    CameraKey key;
                    key.Frame = keyJson.value("Frame", 0u);
                    key.Position = *position;
                    key.Target = *target;

                    if (!settings.CameraPath.empty() && key.Frame <= settings.CameraPath.back().Frame)
                        return fail("the frames of \"CameraPath\" have to increase");
                    settings.CameraPath.push_back(key);
                }
            }

            if (const auto parameters = json.find("Parameters"); parameters != json.end())
            {
                if (!parameters->is_object())
                    return fail("\"Parameters\" has to be an object");

                for (const auto& [name, value] : parameters->items())
                {
                    if (!value.is_number())
                        return fail("parameter \"" + name + "\" has to be a number");
                    settings.Parameters[name] = value.get<double>();
                }
            }

            return settings;
        }
        catch (const nlohmann::json::exception& exception)
        {
            return fail(exception.what());
        }
    }

    std::expected<std::optional<BenchmarkSettings>, std::string> BenchmarkSettings::FromCommandLine(const CommandLine& commandLine, const std::filesystem::path& projectRoot, const std::filesystem::path& defaultOutputRoot)
    {
        const std::optional<std::string> benchFile = commandLine.Value("bench");
        if (!benchFile)
            return std::optional<BenchmarkSettings>();

        // An absolute path replaces projectRoot, which is how operator/ treats one.
        auto settings = Load(ContentPaths::Normalize(projectRoot / *benchFile));
        if (!settings)
            return std::unexpected(settings.error());

        for (const std::string& assignment : commandLine.Values("bench-set"))
        {
            const size_t separator = assignment.find('=');
            double number = 0.0;
            const char* const numberBegin = assignment.data() + separator + 1;
            const char* const numberEnd = assignment.data() + assignment.size();
            if (separator == std::string::npos || std::from_chars(numberBegin, numberEnd, number).ptr != numberEnd)
                return std::unexpected("--bench-set expects <name>=<number>, got: " + assignment);

            settings->Parameters[assignment.substr(0, separator)] = number;
        }

        if (const std::optional<std::string> output = commandLine.Value("bench-out"))
            settings->OutputDirectory = ContentPaths::Normalize(projectRoot / *output);
        else
            settings->OutputDirectory = ContentPaths::Normalize(defaultOutputRoot / settings->Name);

        return std::optional<BenchmarkSettings>(std::move(*settings));
    }

    Benchmark::Benchmark(BenchmarkSettings settings)
        : m_Settings(std::move(settings))
    {
    }

    Benchmark::~Benchmark() = default;

    float Benchmark::BeginFrame(Scene& scene, Renderer& renderer, AssetManager& assetManager, FrameProfiler& profiler)
    {
        switch (m_Phase)
        {
            case Phase::Warmup:
            {
                PlaceCamera(scene, renderer, 0);
                ++m_WarmupFramesDone;

                // Game time stands still during the warm-up, so its length does not change what is measured.
                if (m_WarmupFramesDone >= m_Settings.WarmupFrames && !assetManager.HasPendingLoads())
                {
                    LX_CORE_INFO("Benchmark '{0}': measuring {1} frames after {2} warm-up frames.", m_Settings.Name, m_Settings.Frames, m_WarmupFramesDone);
                    profiler.StartRecording();
                    m_Phase = Phase::Measure;
                }
                return 0.0f;
            }

            case Phase::Measure:
            {
                PlaceCamera(scene, renderer, m_Frame);

                if (std::ranges::find(m_Settings.ScreenshotFrames, m_Frame) != m_Settings.ScreenshotFrames.end())
                    renderer.RequestScreenshot(m_Frame);
                KeepScreenshots(renderer.TakeScreenshots());

                ++m_Frame;
                if (m_Frame == m_Settings.Frames)
                    m_Phase = Phase::Finish;
                return m_Settings.FixedDeltaTime;
            }

            case Phase::Finish:
            {
                // The last measured frame ended when this frame began.
                profiler.StopRecording();
                renderer.FlushGpuTimers();
                KeepScreenshots(renderer.TakeScreenshots(true));

                m_Succeeded = WriteResults(renderer, profiler);
                m_Phase = Phase::Finished;
                return 0.0f;
            }

            case Phase::Finished:
                break;
        }

        return 0.0f;
    }

    void Benchmark::PlaceCamera(Scene& scene, Renderer& renderer, uint32_t frame)
    {
        if (m_Settings.CameraPath.empty())
            return;

        if (!m_Camera)
        {
            // The benchmark's camera replaces whichever camera the scene was saved with.
            for (const auto entity : scene.Reg().view<CameraComponent>())
                scene.Reg().get<CameraComponent>(entity).Primary = false;

            m_Camera = scene.CreateEntity(CameraEntityName);
            CameraComponent& camera = m_Camera.AddComponent<CameraComponent>();
            camera.Primary = true;
            camera.Camera.SetPerspective(glm::radians(m_Settings.FieldOfViewDegrees), m_Settings.NearClip, m_Settings.FarClip);
        }

        const auto [viewportWidth, viewportHeight] = renderer.GetViewportSize();
        m_Camera.GetComponent<CameraComponent>().Camera.SetViewportSize(viewportWidth, viewportHeight);

        const BenchmarkSettings::CameraKey key = SampleCameraPath(m_Settings.CameraPath, frame);
        const glm::vec3 forward = glm::normalize(key.Target - key.Position);
        const glm::vec3 up = std::abs(forward.y) > 0.999f ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

        // The renderer blends between the previous and the current pose, so both are set.
        TransformComponent& transform = m_Camera.GetComponent<TransformComponent>();
        transform.Translation = key.Position;
        transform.Rotation = glm::quatLookAt(forward, up);
        transform.PreviousTranslation = transform.Translation;
        transform.PreviousRotation = transform.Rotation;
    }

    void Benchmark::KeepScreenshots(std::vector<Screenshot> screenshots)
    {
        for (Screenshot& screenshot : screenshots)
            m_Screenshots.push_back(std::move(screenshot));
    }

    bool Benchmark::WriteResults(Renderer& renderer, const FrameProfiler& profiler)
    {
        std::error_code error;
        std::filesystem::create_directories(m_Settings.OutputDirectory, error);
        if (error)
        {
            LX_CORE_ERROR("Benchmark '{0}': cannot create {1}: {2}", m_Settings.Name, m_Settings.OutputDirectory.string(), error.message());
            return false;
        }

        bool succeeded = true;
        const FrameProfiler::Recording& recording = profiler.GetRecording();

        nlohmann::json screenshotNames = nlohmann::json::array();
        std::ranges::sort(m_Screenshots, {}, &Screenshot::Tag);
        for (const Screenshot& screenshot : m_Screenshots)
        {
            const std::string fileName = ScreenshotFileName(screenshot.Tag);
            if (WritePng(m_Settings.OutputDirectory / fileName, screenshot))
            {
                screenshotNames.push_back(fileName);
            }
            else
            {
                LX_CORE_ERROR("Benchmark '{0}': cannot write {1}", m_Settings.Name, fileName);
                succeeded = false;
            }
        }

        if (m_Screenshots.size() != m_Settings.ScreenshotFrames.size())
        {
            LX_CORE_ERROR("Benchmark '{0}': {1} of {2} screenshots arrived.", m_Settings.Name, m_Screenshots.size(), m_Settings.ScreenshotFrames.size());
            succeeded = false;
        }

        const Statistics frameTime = Summarize(recording.FrameMs);
        const auto [viewportWidth, viewportHeight] = renderer.GetViewportSize();

        nlohmann::json json;
        json["Name"] = m_Settings.Name;
        json["Adapter"] = renderer.GetAdapterName();
#ifdef _DEBUG
        json["Build"] = "Debug";
#else
        json["Build"] = "Release";
#endif
        json["Width"] = viewportWidth;
        json["Height"] = viewportHeight;
        json["Frames"] = recording.FrameMs.size();
        json["WarmupFrames"] = m_WarmupFramesDone;
        json["Parameters"] = m_Settings.Parameters;
        json["FrameMs"] = ToJson(frameTime);
        json["FramesPerSecond"] = frameTime.Median > 0.0 ? 1000.0 / frameTime.Median : 0.0;
        json["CpuMs"] = ToJson(recording.CpuMs);
        json["GpuMs"] = ToJson(recording.GpuMs);
        json["Counters"] = ToJson(recording.Counters);
        json["Screenshots"] = screenshotNames;

        const std::filesystem::path resultFile = m_Settings.OutputDirectory / "bench.json";
        std::ofstream resultStream(resultFile);
        resultStream << json.dump(2) << '\n';
        if (!resultStream)
        {
            LX_CORE_ERROR("Benchmark '{0}': cannot write {1}", m_Settings.Name, resultFile.string());
            succeeded = false;
        }

        // One line per measured frame, for finding the frames behind a bad percentile and for seeing
        // how a time behaves over a run. A time that was not reported in every frame has no column.
        std::vector<std::pair<std::string, const std::vector<double>*>> columns;
        columns.emplace_back("FrameMs", &recording.FrameMs);
        for (const FrameProfiler::Series& series : recording.GpuMs)
            columns.emplace_back("Gpu." + series.Name, &series.Samples);
        for (const FrameProfiler::Series& series : recording.CpuMs)
            columns.emplace_back("Cpu." + series.Name, &series.Samples);
        std::erase_if(columns, [&recording](const auto& column) { return column.second->size() != recording.FrameMs.size(); });

        std::ofstream frameStream(m_Settings.OutputDirectory / "frames.csv");
        frameStream << "Frame";
        for (const auto& [name, samples] : columns)
            frameStream << ',' << name;
        frameStream << '\n';

        for (size_t frame = 0; frame < recording.FrameMs.size(); ++frame)
        {
            frameStream << frame;
            for (const auto& [name, samples] : columns)
                frameStream << ',' << (*samples)[frame];
            frameStream << '\n';
        }

        LX_CORE_INFO("Benchmark '{0}': median frame {1:.3f} ms over {2} frames. Results in {3}", m_Settings.Name, frameTime.Median, recording.FrameMs.size(), m_Settings.OutputDirectory.string());
        return succeeded;
    }
}
