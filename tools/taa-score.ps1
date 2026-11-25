# Scores pictures of the RenderBench benchmarks against reference pictures of the same frames: how
# anti-aliasing over time is judged by numbers, next to the eye.
#
#   tools/taa-score.ps1 -Truth <label> -Label <label>[,<label>...] [-Bench <name>,...] [-Still <label>,<label>]
#
# The truth is a label made with many samples a pixel, for example
#   tools/bench.ps1 -Label truth -Set Jitter=1,JitterLength=64,Accumulate=1,HoldFrames=63
# and each label is a run to judge, for example with -Set TAA=1.
#
# Per label, benchmark and saved frame it prints:
#   Off     how far the picture is from the truth: the root of the mean squared difference, in steps of 255.
#   Detail  how much fine detail it has, as a share of the truth's: the mean difference between neighbouring
#           pixels. Under 100 % the picture is softer than the truth, over 100 % it has jagged edges or noise.
#
# -Still takes two labels of the same held frames that differ by one frame of holding, for example
# HoldFrames=40 and HoldFrames=41 with -Set TAA=1. Their difference is what flickers in a picture that
# stands still, printed as Shimmer: the mean difference between the two, in steps of 255.

param(
    [Parameter(Mandatory = $true)][string]$Truth,
    [string[]]$Label = @(),
    [string[]]$Bench = @("motion", "trees"),
    [string[]]$Still = @()
)

$ErrorActionPreference = "Stop"
$Label = @($Label | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })
$Bench = @($Bench | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })
$Still = @($Still | ForEach-Object { $_ -split "," } | Where-Object { $_ -ne "" })

Add-Type -AssemblyName System.Drawing
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @'
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class PictureScore
{
    static byte[] Read(string file, out int width, out int height)
    {
        using (Bitmap bitmap = new Bitmap(file))
        {
            width = bitmap.Width;
            height = bitmap.Height;
            BitmapData data = bitmap.LockBits(new Rectangle(0, 0, width, height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            byte[] pixels = new byte[width * height * 4];
            for (int y = 0; y < height; ++y)
                Marshal.Copy(IntPtr.Add(data.Scan0, y * data.Stride), pixels, y * width * 4, width * 4);
            bitmap.UnlockBits(data);
            return pixels;
        }
    }

    // The root of the mean squared difference over red, green and blue.
    public static double Off(string file, string truthFile)
    {
        int width, height, truthWidth, truthHeight;
        byte[] picture = Read(file, out width, out height);
        byte[] truth = Read(truthFile, out truthWidth, out truthHeight);
        if (width != truthWidth || height != truthHeight)
            return double.NaN;

        double sum = 0.0;
        for (int index = 0; index < picture.Length; ++index)
        {
            if (index % 4 == 3)
                continue;
            double difference = picture[index] - truth[index];
            sum += difference * difference;
        }
        return Math.Sqrt(sum / (width * height * 3.0));
    }

    // The mean absolute difference over red, green and blue.
    public static double MeanDifference(string file, string otherFile)
    {
        int width, height, otherWidth, otherHeight;
        byte[] picture = Read(file, out width, out height);
        byte[] other = Read(otherFile, out otherWidth, out otherHeight);
        if (width != otherWidth || height != otherHeight)
            return double.NaN;

        double sum = 0.0;
        for (int index = 0; index < picture.Length; ++index)
        {
            if (index % 4 != 3)
                sum += Math.Abs(picture[index] - other[index]);
        }
        return sum / (width * height * 3.0);
    }

    // The mean difference between a pixel and its neighbours to the right and below, over red, green and blue.
    public static double Detail(string file)
    {
        int width, height;
        byte[] picture = Read(file, out width, out height);
        double sum = 0.0;
        for (int y = 0; y < height - 1; ++y)
        {
            for (int x = 0; x < width - 1; ++x)
            {
                int at = (y * width + x) * 4;
                for (int channel = 0; channel < 3; ++channel)
                {
                    sum += Math.Abs(picture[at + channel] - picture[at + 4 + channel]);
                    sum += Math.Abs(picture[at + channel] - picture[at + width * 4 + channel]);
                }
            }
        }
        return sum / ((width - 1) * (height - 1) * 6.0);
    }
}
'@

$benchRoot = Join-Path (Split-Path -Parent $PSScriptRoot) "samples\RenderBench\temp\bench"

$rows = foreach ($benchName in $Bench) {
    $truthDirectory = Join-Path $benchRoot "$Truth\$benchName"
    foreach ($truthFile in (Get-ChildItem $truthDirectory -Filter "frame_*.png" | Where-Object { $_.Name -notmatch "difference" })) {
        $truthDetail = [PictureScore]::Detail($truthFile.FullName)
        foreach ($name in $Label) {
            $file = Join-Path $benchRoot "$name\$benchName\$($truthFile.Name)"
            if (-not (Test-Path $file)) { continue }
            [pscustomobject]@{
                Benchmark = $benchName
                Frame     = $truthFile.BaseName -replace "frame_0*(\d)", '$1'
                Label     = $name
                Off       = "{0:N2}" -f [PictureScore]::Off($file, $truthFile.FullName)
                Detail    = "{0:N0} %" -f (100.0 * [PictureScore]::Detail($file) / $truthDetail)
            }
        }
    }
}
$rows | Format-Table -AutoSize

if ($Still.Count -eq 2) {
    $stillRows = foreach ($benchName in $Bench) {
        foreach ($file in (Get-ChildItem (Join-Path $benchRoot "$($Still[0])\$benchName") -Filter "frame_*.png" | Where-Object { $_.Name -notmatch "difference" })) {
            $other = Join-Path $benchRoot "$($Still[1])\$benchName\$($file.Name)"
            if (-not (Test-Path $other)) { continue }
            [pscustomobject]@{
                Benchmark = $benchName
                Frame     = $file.BaseName -replace "frame_0*(\d)", '$1'
                Shimmer   = "{0:N3}" -f [PictureScore]::MeanDifference($file.FullName, $other)
            }
        }
    }
    $stillRows | Format-Table -AutoSize
}
