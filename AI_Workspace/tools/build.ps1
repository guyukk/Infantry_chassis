param(
    [ValidateSet('chassis', 'gimbal', 'both')][string]$Board = 'both',
    [string]$LogDirectory = ''
)

$ErrorActionPreference = 'Stop'
$rootCandidate = [IO.DirectoryInfo]$PSScriptRoot
while ($null -ne $rootCandidate -and -not (Test-Path -LiteralPath (Join-Path $rootCandidate.FullName 'MDK-ARM\standard_tpye_c.uvprojx'))) {
    $rootCandidate = $rootCandidate.Parent
}
if ($null -eq $rootCandidate) { throw 'Cannot locate chassis project root' }
$chassisRoot = $rootCandidate.FullName
$config = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'projects.json')) | ConvertFrom-Json
$keilExecutable = $config.keil_executable
$projects = @($config.projects.PSObject.Properties | ForEach-Object {
    $projectItem = $_.Value
    @{ Board = $_.Name; Root = [IO.Path]::GetFullPath((Join-Path $chassisRoot $projectItem.root)); File = $projectItem.project; Target = $projectItem.target }
})
if ([string]::IsNullOrWhiteSpace($LogDirectory)) {
    $LogDirectory = Join-Path $chassisRoot ('AI_Workspace\artifacts\logs\build-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
}

if (-not (Test-Path -LiteralPath $keilExecutable -PathType Leaf)) { throw "Keil not found: $keilExecutable" }
$null = New-Item -ItemType Directory -Path $LogDirectory -Force
$logRoot = (Resolve-Path -LiteralPath $LogDirectory).Path
$results = @()
foreach ($project in $projects) {
    if ($Board -ne 'both' -and $Board -ne $project.Board) { continue }
    $projectPath = Join-Path $project.Root $project.File
    [xml]$projectXml = [System.IO.File]::ReadAllText($projectPath)
    $targetXml = @($projectXml.Project.Targets.Target | Where-Object { $_.TargetName -eq $project.Target })[0]
    $outputOption = $targetXml.TargetOption.TargetCommonOption
    $imagePath = Join-Path (Join-Path (Split-Path -Parent $projectPath) $outputOption.OutputDirectory) ($outputOption.OutputName + '.axf')
    $logPath = Join-Path $logRoot ($project.Board + '.log')
    if (Test-Path -LiteralPath $logPath) { throw "Refusing to overwrite build evidence: $logPath" }
    $arguments = '-r "{0}" -t "{1}" -o "{2}" -j0' -f $projectPath, $project.Target, $logPath
    $started = Get-Date
    $process = Start-Process -FilePath $keilExecutable -ArgumentList $arguments -WorkingDirectory (Split-Path -Parent $projectPath) -WindowStyle Hidden -PassThru -Wait
    $result = [pscustomobject]@{
        Board = $project.Board
        Target = $project.Target
        Project = $projectPath
        ExitCode = $process.ExitCode
        Started = $started.ToString('o')
        Seconds = [math]::Round(((Get-Date) - $started).TotalSeconds, 2)
        Log = $logPath
        Summary = @()
        Image = $imagePath
        ImageSHA256 = $null
        Success = $false
    }
    if (Test-Path -LiteralPath $logPath) {
        $result.Summary = @(Get-Content -LiteralPath $logPath | Where-Object { $_ -match 'Error\(s\)|Warning\(s\)|Program Size|Target not created|Build Time|Error:|error:' })
    }
    $hasZeroErrors = @($result.Summary | Where-Object { $_ -match ' - 0 Error\(s\),' }).Count -gt 0
    $freshImage = (Test-Path -LiteralPath $imagePath) -and ((Get-Item -LiteralPath $imagePath).LastWriteTime -ge $started.AddSeconds(-2))
    $result.Success = ($result.ExitCode -in @(0, 1)) -and $hasZeroErrors -and $freshImage
    if ($result.Success) { $result.ImageSHA256 = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash }
    $results += $result
    $result | ConvertTo-Json -Depth 4
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $logRoot 'results.json') -Encoding UTF8
# uVision returns 1 for a successful build with warnings; errors start at 2.
if (@($results | Where-Object { -not $_.Success }).Count -gt 0) { exit 1 }
