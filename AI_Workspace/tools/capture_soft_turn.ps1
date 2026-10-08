param(
    [Parameter(Mandatory=$true)][string]$Port,
    [Parameter(Mandatory=$true)][string]$Output,
    [ValidateRange(1,3600)][int]$Seconds = 30
)
$ErrorActionPreference = 'Stop'
# Run only when the vehicle is connected to the test logger, not the referee box.
if (-not ('System.IO.Ports.SerialPort' -as [type])) { Add-Type -AssemblyName System.IO.Ports }
$serialPort = [System.IO.Ports.SerialPort]::new($Port, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$serialPort.ReadTimeout = 100
$capturePath = [IO.Path]::GetFullPath($Output)
$null = [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($capturePath))
$captureFile = $null
try {
    $captureFile = [IO.File]::Open($capturePath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
    $serialPort.Open()
    $captureClock = [Diagnostics.Stopwatch]::StartNew()
    $captureBuffer = [byte[]]::new(4096)
    while ($captureClock.Elapsed.TotalSeconds -lt $Seconds) {
        try {
            $receivedCount = $serialPort.Read($captureBuffer, 0, $captureBuffer.Length)
            $captureFile.Write($captureBuffer, 0, $receivedCount)
        } catch [System.TimeoutException] { }
    }
    Write-Output $capturePath
} finally {
    $serialPort.Dispose()
    if ($null -ne $captureFile) { $captureFile.Dispose() }
}
