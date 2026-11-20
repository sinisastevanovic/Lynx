# Runs the benchmarks of the RenderBench project and collects their results under one label.
# Used to measure a rendering change: run it before the change and after it, then compare the two
# labels with tools/bench-compare.ps1.
#
#   tools/bench.ps1 [-Label <name>] [-Bench <name>,...] [-Runs <count>] [-Configuration Release|Debug]
#       [-BuildDirectory <folder>] [-NoBuild] [-Set <name>=<number>,...]
#
# Each benchmark is run several times, because frame times vary from run to run. The run with the
# median frame time is the result. The pictures of all runs are compared to check that the benchmark
# renders the same frames every time.
#
# Results go to samples/RenderBench/temp/bench/<label>/<benchmark>/: result.json and the pictures.
# The label defaults to the current commit, with "-dirty" appended when there are uncommitted changes.
# The exit code is 1 if a run failed or wrote errors to the engine log.
param(
    [string]$Label = "",
    [string[]]$Bench = @("features", "sponza", "grid"),
    [int]$Runs = 3,
    [ValidateSet("Release", "Debug")][string]$Configuration = "Release",
    [string]$BuildDirectory = "",
    [switch]$NoBuild,
    [string[]]$Set = @()
)

$ErrorActionPreference = "Stop"

$engineRoot = Split-Path -Parent $PSScriptRoot
$projectRoot = Join-Path $engineRoot "samples\RenderBench"
$projectFile = Join-Path $projectRoot "RenderBench.lxproj"
$engineLog = Join-Path $projectRoot "temp\engine.log"

# The engine is built as part of a game project. By default that is the sandbox game next to the engine.
if ($BuildDirectory -eq "") {
    $BuildDirectory = Join-Path (Split-Path -Parent $engineRoot) "LynxSandboxGame\build"
}
$executable = Join-Path $BuildDirectory "bin\$Configuration\RenderBench.exe"

$commit = (git -C $engineRoot rev-parse --short HEAD).Trim()
$isDirty = [bool](git -C $engineRoot status --porcelain --untracked-files=no)
if ($Label -eq "") {
    $Label = $commit
    if ($isDirty) { $Label += "-dirty" }
}

if (-not $NoBuild) {
    "Building RenderBench ($Configuration)..."
    $buildOutput = cmake --build $BuildDirectory --config $Configuration --target RenderBench 2>&1
    if ($LASTEXITCODE -ne 0) {
        $buildOutput | Select-String -Pattern "error" | Select-Object -First 30
        "Build failed."
        exit 1
    }
}

if (-not (Test-Path $executable)) {
    "Not found: $executable"
    exit 1
}

$labelDirectory = Join-Path $projectRoot "temp\bench\$Label"
$failed = $false
$summary = @()

foreach ($benchName in $Bench) {
    $benchDirectory = Join-Path $labelDirectory $benchName
    if (Test-Path $benchDirectory) { Remove-Item -Recurse -Force $benchDirectory }

    $runResults = @()
    for ($run = 1; $run -le $Runs; $run++) {
        $runDirectory = Join-Path $benchDirectory "run$run"
        $arguments = @("`"$projectFile`"", "--bench=bench/$benchName.json", "--bench-out=`"$runDirectory`"")
        foreach ($assignment in $Set) { $arguments += "--bench-set=$assignment" }

        $process = Start-Process -FilePath $executable -ArgumentList $arguments -WorkingDirectory $projectRoot -PassThru -Wait -WindowStyle Normal
        $resultFile = Join-Path $runDirectory "bench.json"

        # The log is overwritten by the next run, so each run keeps its own copy.
        $problems = @()
        if (Test-Path $engineLog) {
            if (Test-Path $runDirectory) { Copy-Item $engineLog (Join-Path $runDirectory "engine.log") }
            $problems = @(Select-String -Path $engineLog -Pattern "\[error\]", "\[critical\]", "NVRHI:" | Select-Object -First 10)
        }

        if ($process.ExitCode -ne 0 -or -not (Test-Path $resultFile)) {
            "${benchName}: run $run failed with exit code $($process.ExitCode)."
            $problems | ForEach-Object { "    " + $_.Line }
            $failed = $true
            continue
        }
        if ($problems.Count -gt 0) {
            "${benchName}: run $run wrote errors to the log:"
            $problems | ForEach-Object { "    " + $_.Line }
            $failed = $true
        }

        $runResults += [pscustomobject]@{
            Run = $run
            Directory = $runDirectory
            Result = Get-Content $resultFile -Raw | ConvertFrom-Json
        }
    }

    if ($runResults.Count -eq 0) { continue }

    # The run in the middle stands for the benchmark: it is less affected by a single disturbed run than the mean.
    $sorted = @($runResults | Sort-Object { $_.Result.FrameMs.Median })
    $chosen = $sorted[[int][math]::Floor(($sorted.Count - 1) / 2)]

    # Identical pictures in every run show that the benchmark is repeatable.
    $picturesMatch = $true
    foreach ($picture in $chosen.Result.Screenshots) {
        $hashes = @($runResults | ForEach-Object { (Get-FileHash (Join-Path $_.Directory $picture) -Algorithm SHA256).Hash } | Sort-Object -Unique)
        if ($hashes.Count -ne 1) { $picturesMatch = $false }
        Copy-Item (Join-Path $chosen.Directory $picture) (Join-Path $benchDirectory $picture)
    }

    $result = $chosen.Result
    $result | Add-Member -NotePropertyName Label -NotePropertyValue $Label
    $result | Add-Member -NotePropertyName Commit -NotePropertyValue $commit
    $result | Add-Member -NotePropertyName UncommittedChanges -NotePropertyValue $isDirty
    $result | Add-Member -NotePropertyName RunFrameMsMedians -NotePropertyValue @($runResults | ForEach-Object { $_.Result.FrameMs.Median })
    $result | Add-Member -NotePropertyName PicturesMatchAcrossRuns -NotePropertyValue $picturesMatch
    $result | ConvertTo-Json -Depth 10 | Set-Content -Path (Join-Path $benchDirectory "result.json") -Encoding utf8

    $medians = @($runResults | ForEach-Object { $_.Result.FrameMs.Median })
    $spread = 0.0
    if ($result.FrameMs.Median -gt 0) {
        $spread = (($medians | Measure-Object -Maximum).Maximum - ($medians | Measure-Object -Minimum).Minimum) / $result.FrameMs.Median * 100.0
    }

    $summary += [pscustomobject]@{
        Benchmark = $benchName
        "Frame ms" = "{0:N3}" -f $result.FrameMs.Median
        "P99 ms" = "{0:N3}" -f $result.FrameMs.P99
        "GPU ms" = "{0:N3}" -f $result.GpuMs.Frame.Median
        FPS = "{0:N0}" -f $result.FramesPerSecond
        "Run spread" = "{0:N1} %" -f $spread
        "Same pictures" = $picturesMatch
    }
}

""
"Label $Label, $Configuration, $Runs runs each. Results in $labelDirectory"
$summary | Format-Table -AutoSize | Out-String -Width 200

if ($failed) { exit 1 }
exit 0
