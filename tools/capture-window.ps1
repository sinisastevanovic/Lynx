# Starts the editor or a game, waits, saves a picture of its window as a PNG, then closes it.
# Used to check rendering changes by comparing a picture from before a change with one from after.
#
#   tools/capture-window.ps1 -Exe <path> -WorkingDirectory <project folder> -Output <png>
#       [-Arguments <text>] [-Wait <seconds>] [-ResizeWidth <px> -ResizeHeight <px>]
#
# The window does not have to be in front. Note that a game may lock the mouse cursor while it runs.
param(
    [Parameter(Mandatory)][string]$Exe,
    [Parameter(Mandatory)][string]$WorkingDirectory,
    [Parameter(Mandatory)][string]$Output,
    [string]$Arguments = "",
    [int]$Wait = 12,
    [int]$ResizeWidth = 0,
    [int]$ResizeHeight = 0
)

Add-Type -AssemblyName System.Drawing
Add-Type -Namespace LynxCapture -Name Win -MemberDefinition @'
[DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr hwnd, IntPtr hdc, uint flags);
[StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
[DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out RECT rect);
[DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr hwnd, int x, int y, int w, int h, bool repaint);
'@

$startInfo = New-Object System.Diagnostics.ProcessStartInfo
$startInfo.FileName = $Exe
$startInfo.Arguments = $Arguments
$startInfo.WorkingDirectory = $WorkingDirectory
$startInfo.UseShellExecute = $false
$startInfo.RedirectStandardOutput = $true
$process = [System.Diagnostics.Process]::Start($startInfo)
$null = $process.StandardOutput.ReadToEndAsync()
Start-Sleep -Seconds $Wait

if ($process.HasExited) {
    "process exited early with code $($process.ExitCode)"
    exit 1
}

$process.Refresh()
$window = $process.MainWindowHandle

if ($ResizeWidth -gt 0) {
    [void][LynxCapture.Win]::MoveWindow($window, 100, 100, $ResizeWidth, $ResizeHeight, $true)
    Start-Sleep -Seconds 3
}

$rect = New-Object LynxCapture.Win+RECT
[void][LynxCapture.Win]::GetClientRect($window, [ref]$rect)
$bitmap = New-Object System.Drawing.Bitmap ($rect.R - $rect.L), ($rect.B - $rect.T)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$deviceContext = $graphics.GetHdc()

# 1 = the content area only, 2 = render the full content, which a window drawn by the GPU needs.
$captured = [LynxCapture.Win]::PrintWindow($window, $deviceContext, 3)
$graphics.ReleaseHdc($deviceContext)
$bitmap.Save($Output, [System.Drawing.Imaging.ImageFormat]::Png)
"captured=$captured size=$($bitmap.Width)x$($bitmap.Height)"

[void]$process.CloseMainWindow()
if ($process.WaitForExit(20000)) {
    "exit code $($process.ExitCode)"
} else {
    $process.Kill()
    "did not exit within 20 seconds, killed"
}
