param([string]$LogDirectory = '')
$ErrorActionPreference = 'Stop'
$rootCandidate = [IO.DirectoryInfo]$PSScriptRoot
while ($null -ne $rootCandidate -and -not (Test-Path -LiteralPath (Join-Path $rootCandidate.FullName 'MDK-ARM\standard_tpye_c.uvprojx'))) {
    $rootCandidate = $rootCandidate.Parent
}
if ($null -eq $rootCandidate) { throw 'Cannot locate chassis project root' }
$chassisRoot = $rootCandidate.FullName
$config = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'projects.json')) | ConvertFrom-Json
$originalProject = Join-Path $chassisRoot $config.projects.chassis.project
$temporaryProject = Join-Path (Split-Path -Parent $originalProject) 'soft-turn-debug.uvprojx'
if (Test-Path -LiteralPath $temporaryProject) { throw "Refusing to overwrite temporary project: $temporaryProject" }
if ([string]::IsNullOrWhiteSpace($LogDirectory)) {
    $LogDirectory = Join-Path $chassisRoot ('AI_Workspace\artifacts\logs\soft-turn-debug-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}
$null = New-Item -ItemType Directory -Path $LogDirectory -Force
$logRoot = (Resolve-Path -LiteralPath $LogDirectory).Path
$logPath = Join-Path $logRoot 'chassis-debug.log'
if (Test-Path -LiteralPath $logPath) { throw "Refusing to overwrite build evidence: $logPath" }
$debugOutput = Join-Path $chassisRoot 'AI_Workspace\cache\keil\soft-turn-debug'
$null = New-Item -ItemType Directory -Path $debugOutput -Force
[xml]$projectXml = [IO.File]::ReadAllText($originalProject)
$targetXml = @($projectXml.Project.Targets.Target | Where-Object { $_.TargetName -eq $config.projects.chassis.target })[0]
$controls = $targetXml.TargetOption.TargetArmAds.Cads.VariousControls
$debugProjectText = [IO.File]::ReadAllText($originalProject)
$debugProjectText = $debugProjectText.Replace('<Define>' + $controls.Define + '</Define>', '<Define>' + $controls.Define + ',CHASSIS_SOFT_TURN_TELEMETRY=1</Define>')
$debugProjectText = [regex]::new('<OutputDirectory>.*?</OutputDirectory>').Replace($debugProjectText, '<OutputDirectory>..\AI_Workspace\cache\keil\soft-turn-debug\</OutputDirectory>', 1)
$debugProjectText = [regex]::new('<OutputName>.*?</OutputName>').Replace($debugProjectText, '<OutputName>standard_tpye_c_soft_turn_debug</OutputName>', 1)
$debugProjectText = [regex]::new('<ListingPath>.*?</ListingPath>').Replace($debugProjectText, '<ListingPath>..\AI_Workspace\cache\keil\soft-turn-debug\</ListingPath>', 1)
try {
    # Same directory preserves every existing relative include/source path.
    # Preserve Keil's XML text and save UTF-8 without a BOM.
    [IO.File]::WriteAllText($temporaryProject, $debugProjectText, [Text.UTF8Encoding]::new($false))
    $arguments = '-r "{0}" -t "{1}" -o "{2}" -j0' -f $temporaryProject, $config.projects.chassis.target, $logPath
    $started = Get-Date
    $process = Start-Process -FilePath $config.keil_executable -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $originalProject) -WindowStyle Hidden -PassThru -Wait
    if (-not (Test-Path -LiteralPath $logPath)) { throw "Keil returned exit code $($process.ExitCode) without a build log" }
    $summary = @(Get-Content -LiteralPath $logPath | Where-Object { $_ -match 'Error\(s\)|Warning\(s\)|Program Size|error:|Error:' })
    $imagePath = Join-Path $debugOutput 'standard_tpye_c_soft_turn_debug.axf'
    $success = ($process.ExitCode -in @(0,1)) -and (@($summary | Where-Object { $_ -match ' - 0 Error\(s\),' }).Count -gt 0) -and
        (Test-Path -LiteralPath $imagePath) -and ((Get-Item -LiteralPath $imagePath).LastWriteTime -ge $started.AddSeconds(-2))
    $result = [pscustomobject]@{ Success=$success; ExitCode=$process.ExitCode; Summary=$summary; Image=$imagePath; Telemetry=1; Log=$logPath; ImageSHA256=$null }
    if ($success) {
        $result.ImageSHA256 = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash
        foreach ($extension in @('.axf', '.hex')) {
            $artifactPath = Join-Path $debugOutput ('standard_tpye_c_soft_turn_debug' + $extension)
            if (Test-Path -LiteralPath $artifactPath) { Copy-Item -LiteralPath $artifactPath -Destination $logRoot }
        }
    }
    $result | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $logRoot 'results.json') -Encoding UTF8
    $result | ConvertTo-Json -Depth 4
    if (-not $success) { throw 'Soft-turn telemetry firmware build failed' }
} finally {
    if (Test-Path -LiteralPath $temporaryProject) { Remove-Item -LiteralPath $temporaryProject -Force }
}
