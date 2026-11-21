# Compares the benchmark results of two labels written by tools/bench.ps1: timings, counters and pictures.
#
#   tools/bench-compare.ps1 -Before <label> -After <label> [-Bench <name>,...] [-Threshold <percent>] [-All]
#
# The timings compared are the "clean" ones, see tools/bench.ps1. A timing that changed by less than
# the threshold is treated as unchanged. Rows that changed more are marked "faster" or "SLOWER".
# "Frame" is the slower of the CPU's and the GPU's time for a frame, which is what limits the frame rate.
# Measured on an RTX 5080 with five runs: clean GPU times repeat within 1.3 % and the frame within 2.0 %.
# CPU times of single passes and the CPU's side of a frame vary by up to 8 % and get four times the threshold.
# The 99th percentile is raw and varies by up to 50 % on its own, so it gets no verdict.
# Between sessions the same build drifts, so the script warns when the two results are far apart in time.
# Rows whose time is too small to measure reliably are left out unless -All is given.
# For every picture that differs, a picture showing where is written next to the "after" result.
param(
    [Parameter(Mandatory)][string]$Before,
    [Parameter(Mandatory)][string]$After,
    [string[]]$Bench = @(),
    [double]$Threshold = 2.5,
    [switch]$All
)

$ErrorActionPreference = "Stop"

# Started with "powershell -File", a comma-separated list arrives as one string.
$Bench = @($Bench | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class LynxPictureCompare
{
    public class Difference
    {
        public bool SameSize;
        public long DifferentPixels;
        public long TotalPixels;
        public int LargestChannelDifference;
        public double MeanChannelDifference;
    }

    static byte[] Pixels(Bitmap bitmap)
    {
        BitmapData data = bitmap.LockBits(new Rectangle(0, 0, bitmap.Width, bitmap.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
        byte[] pixels = new byte[data.Stride * data.Height];
        Marshal.Copy(data.Scan0, pixels, 0, pixels.Length);
        bitmap.UnlockBits(data);
        return pixels;
    }

    // Writes a picture to differencePath in which every differing pixel is bright, when the pictures differ.
    public static Difference Compare(string beforePath, string afterPath, string differencePath)
    {
        using (Bitmap before = new Bitmap(beforePath))
        using (Bitmap after = new Bitmap(afterPath))
        {
            Difference difference = new Difference();
            difference.SameSize = before.Width == after.Width && before.Height == after.Height;
            if (!difference.SameSize)
                return difference;

            byte[] beforePixels = Pixels(before);
            byte[] afterPixels = Pixels(after);
            byte[] differencePixels = new byte[beforePixels.Length];
            long channelSum = 0;

            for (int offset = 0; offset < beforePixels.Length; offset += 4)
            {
                int largest = 0;
                for (int channel = 0; channel < 3; channel++)
                {
                    int delta = Math.Abs(beforePixels[offset + channel] - afterPixels[offset + channel]);
                    channelSum += delta;
                    if (delta > largest) largest = delta;
                }

                if (largest > 0)
                {
                    difference.DifferentPixels++;
                    if (largest > difference.LargestChannelDifference) difference.LargestChannelDifference = largest;
                }

                // Small differences are hard to see, so they are amplified.
                byte shade = (byte)Math.Min(255, largest * 8 + (largest > 0 ? 64 : 0));
                differencePixels[offset + 0] = shade;
                differencePixels[offset + 1] = shade;
                differencePixels[offset + 2] = shade;
                differencePixels[offset + 3] = 255;
            }

            difference.TotalPixels = beforePixels.Length / 4;
            difference.MeanChannelDifference = (double)channelSum / (difference.TotalPixels * 3);

            if (difference.DifferentPixels > 0)
            {
                using (Bitmap picture = new Bitmap(before.Width, before.Height, PixelFormat.Format32bppArgb))
                {
                    BitmapData data = picture.LockBits(new Rectangle(0, 0, picture.Width, picture.Height), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
                    Marshal.Copy(differencePixels, 0, data.Scan0, differencePixels.Length);
                    picture.UnlockBits(data);
                    picture.Save(differencePath, ImageFormat.Png);
                }
            }

            return difference;
        }
    }
}
'@

$engineRoot = Split-Path -Parent $PSScriptRoot
$benchRoot = Join-Path $engineRoot "samples\RenderBench\temp\bench"
$beforeRoot = Join-Path $benchRoot $Before
$afterRoot = Join-Path $benchRoot $After

foreach ($root in @($beforeRoot, $afterRoot)) {
    if (-not (Test-Path $root)) {
        "No results found at $root"
        exit 1
    }
}

if ($Bench.Count -eq 0) {
    $Bench = @(Get-ChildItem -Directory $afterRoot | Where-Object { Test-Path (Join-Path $beforeRoot $_.Name) } | ForEach-Object { $_.Name })
}

# $rowThreshold is the change in percent from which a time counts as changed. Zero gives no verdict,
# and a negative one marks the row as a count, which is "changed" whenever it differs at all.
function Compare-Row($name, $beforeValue, $afterValue, $rowThreshold) {
    $change = 0.0
    if ($beforeValue -ne 0) { $change = ($afterValue - $beforeValue) / $beforeValue * 100.0 }

    $verdict = ""
    $format = "{0:N3}"
    if ($rowThreshold -lt 0) {
        $format = "{0:N0}"
        if ($beforeValue -ne $afterValue) { $verdict = "changed" }
    }
    elseif ($rowThreshold -gt 0 -and [math]::Abs($change) -ge $rowThreshold) {
        if ($change -lt 0) { $verdict = "faster" } else { $verdict = "SLOWER" }
    }

    [pscustomobject]@{
        Measure = $name
        Before = $format -f $beforeValue
        After = $format -f $afterValue
        Change = "{0:+0.0;-0.0;0.0} %" -f $change
        Verdict = $verdict
    }
}

# A clean time is a plain number. A raw one is a set of statistics, of which the median is compared.
function Get-Number($value) {
    if ($value -is [System.Management.Automation.PSCustomObject]) { return $value.Median }
    return $value
}

# Times below $smallestMeasurableMs are dominated by measuring noise and are left out.
function Compare-Group($title, $beforeGroup, $afterGroup, $rowThreshold, $smallestMeasurableMs) {
    $isTime = $rowThreshold -ge 0
    $rows = @()
    if ($null -eq $afterGroup) { return $rows }

    foreach ($property in $afterGroup.PSObject.Properties) {
        $afterValue = Get-Number $property.Value
        $beforeValue = 0.0
        if ($null -ne $beforeGroup -and $null -ne $beforeGroup.($property.Name)) { $beforeValue = Get-Number $beforeGroup.($property.Name) }

        if ($isTime -and -not $All -and $beforeValue -lt $smallestMeasurableMs -and $afterValue -lt $smallestMeasurableMs) { continue }
        $rows += Compare-Row "$title $($property.Name)" $beforeValue $afterValue $rowThreshold
    }
    return $rows
}

foreach ($benchName in $Bench) {
    $beforeFile = Join-Path $beforeRoot "$benchName\result.json"
    $afterFile = Join-Path $afterRoot "$benchName\result.json"
    if (-not (Test-Path $beforeFile) -or -not (Test-Path $afterFile)) {
        "${benchName}: no result in one of the labels, skipped."
        continue
    }

    $beforeResult = Get-Content $beforeFile -Raw | ConvertFrom-Json
    $afterResult = Get-Content $afterFile -Raw | ConvertFrom-Json

    ""
    "=== $benchName   $Before ($($beforeResult.Build), $($beforeResult.Width)x$($beforeResult.Height))  ->  $After ($($afterResult.Build), $($afterResult.Width)x$($afterResult.Height)) ==="
    if ($beforeResult.Build -ne $afterResult.Build -or $beforeResult.Width -ne $afterResult.Width -or $beforeResult.Height -ne $afterResult.Height -or $beforeResult.Adapter -ne $afterResult.Adapter) {
        "    The two results were measured under different conditions: their timings are not comparable."
    }

    # The machine itself drifts: the same build measured an hour later differed by up to 5 % in frame time
    # and 12 % in GPU time. Pictures and counts are not affected.
    if ($null -ne $beforeResult.MeasuredAt -and $null -ne $afterResult.MeasuredAt) {
        $minutesApart = [math]::Abs(([datetime]$afterResult.MeasuredAt - [datetime]$beforeResult.MeasuredAt).TotalMinutes)
        if ($minutesApart -gt 20) {
            "    Measured {0:N0} minutes apart. Timings drift over that long: measure both back to back before trusting a verdict." -f $minutesApart
        }
    }

    $rows = @()
    $cpuThreshold = $Threshold * 4
    $noVerdict = 0.0
    $smallestGpuMs = 0.02
    $smallestCpuMs = 0.05
    $isCount = -1.0

    # Older results have no clean times. Clean and raw times are not comparable with each other.
    $useClean = $null -ne $beforeResult.Clean -and $null -ne $afterResult.Clean
    if ($useClean -and $beforeResult.Runs -ne $afterResult.Runs) {
        "    Made with $($beforeResult.Runs) and $($afterResult.Runs) runs: fewer runs read higher, so these timings are not comparable."
    }

    if ($useClean) {
        # The frame is as long as the slower of CPU and GPU needs, see tools/bench.ps1. Results made before
        # that was introduced hold a frame time that reads too low where the GPU is the limit.
        if ($null -ne $beforeResult.Clean.BoundBy -and $null -ne $afterResult.Clean.BoundBy) {
            "    Bound by the $($beforeResult.Clean.BoundBy) before, by the $($afterResult.Clean.BoundBy) after."
            $rows += Compare-Row "Frame" $beforeResult.Clean.FrameMs $afterResult.Clean.FrameMs $Threshold
            $rows += Compare-Row "Frame, CPU side" $beforeResult.Clean.CpuFrameMs $afterResult.Clean.CpuFrameMs $cpuThreshold
        }
        else {
            "    One of the results was made before the frame time was defined as the slower of CPU and GPU: no Frame row."
        }
        $rows += Compare-Row "Frame P99 (raw)" $beforeResult.FrameMs.P99 $afterResult.FrameMs.P99 $noVerdict
        $rows += Compare-Group "GPU" $beforeResult.Clean.GpuMs $afterResult.Clean.GpuMs $Threshold $smallestGpuMs
        $rows += Compare-Group "CPU" $beforeResult.Clean.CpuMs $afterResult.Clean.CpuMs $cpuThreshold $smallestCpuMs
    }
    else {
        "    One of the results has no clean times: comparing raw medians, which vary more."
        $rows += Compare-Row "Frame median" $beforeResult.FrameMs.Median $afterResult.FrameMs.Median $Threshold
        $rows += Compare-Row "Frame P99" $beforeResult.FrameMs.P99 $afterResult.FrameMs.P99 $noVerdict
        $rows += Compare-Group "GPU" $beforeResult.GpuMs $afterResult.GpuMs $Threshold $smallestGpuMs
        $rows += Compare-Group "CPU" $beforeResult.CpuMs $afterResult.CpuMs $cpuThreshold $smallestCpuMs
    }
    $rows += Compare-Group "Count" $beforeResult.Counters $afterResult.Counters $isCount 0.0
    $rows | Format-Table -AutoSize | Out-String -Width 200

    foreach ($picture in $afterResult.Screenshots) {
        $beforePicture = Join-Path $beforeRoot "$benchName\$picture"
        $afterPicture = Join-Path $afterRoot "$benchName\$picture"
        if (-not (Test-Path $beforePicture)) {
            "    ${picture}: not in $Before"
            continue
        }

        $differencePicture = Join-Path $afterRoot ("$benchName\" + [System.IO.Path]::GetFileNameWithoutExtension($picture) + "_difference_to_$Before.png")
        $difference = [LynxPictureCompare]::Compare($beforePicture, $afterPicture, $differencePicture)

        if (-not $difference.SameSize) {
            "    ${picture}: the sizes differ"
        }
        elseif ($difference.DifferentPixels -eq 0) {
            "    ${picture}: identical"
        }
        else {
            $share = $difference.DifferentPixels / $difference.TotalPixels * 100.0
            "    {0}: DIFFERENT in {1:N0} of {2:N0} pixels ({3:N4} %), largest difference {4} of 255. See {5}" -f $picture, $difference.DifferentPixels, $difference.TotalPixels, $share, $difference.LargestChannelDifference, $differencePicture
        }
    }
}
