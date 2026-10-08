param(
    [Parameter(Mandatory=$true)][string]$ProjectRoot,
    [Parameter(Mandatory=$true)][string]$IocFile,
    [Parameter(Mandatory=$true)][string]$LogDirectory,
    [string]$FirmwarePath = ''
)

$ErrorActionPreference = 'Stop'
$config = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'projects.json')) | ConvertFrom-Json
$mxExecutable = $config.cubemx_executable
$mxInstall = Split-Path -Parent $mxExecutable
$javaExecutable = Join-Path $mxInstall 'jre\bin\java.exe'
$projectRootPath = (Resolve-Path -LiteralPath $ProjectRoot).Path
# This entry point is deliberately limited to disposable CubeMX validation copies.
if ($projectRootPath -notmatch '[\\/]AI_Workspace[\\/]cache[\\/]cubemx[\\/]') {
    throw 'CubeMX validation must run in an AI_Workspace/cache/cubemx copy'
}
$iocPath = Join-Path $projectRootPath $IocFile
if (-not (Test-Path -LiteralPath $iocPath -PathType Leaf)) { throw "IOC missing: $iocPath" }
$null = New-Item -ItemType Directory -Path $LogDirectory -Force
$logRoot = (Resolve-Path -LiteralPath $LogDirectory).Path
$scriptPath = Join-Path $logRoot 'commands.txt'
$outputPath = Join-Path $logRoot 'stdout.log'
if (Test-Path -LiteralPath $outputPath) { throw "Refusing to overwrite CubeMX log: $outputPath" }
$commands = @(
    'popupwrapper set',
    'popupwrapper record_answer "New STM32Cube firmware version available" BUTTON_1',
    'popupwrapper record_answer "Warning: Code Generation" YES',
    ('config load "{0}"' -f $iocPath),
    ('project path "{0}"' -f $projectRootPath)
)
if (-not [string]::IsNullOrWhiteSpace($FirmwarePath)) {
    $commands += ('project setCustomFWPath "{0}"' -f (Resolve-Path -LiteralPath $FirmwarePath).Path)
}
$commands += @('project generate','exit')
[IO.File]::WriteAllText($scriptPath,($commands -join "`n")+"`n",[Text.UTF8Encoding]::new($false))
$arguments = '-Xmx1024m --add-opens java.desktop/java.awt=ALL-UNNAMED --add-exports java.desktop/sun.awt=ALL-UNNAMED -Djavax.net.ssl.trustStoreType=WINDOWS-ROOT -jar "{0}" -q "{1}"' -f $mxExecutable,$scriptPath
$started = Get-Date
$process = Start-Process -FilePath $javaExecutable -ArgumentList $arguments -WorkingDirectory $mxInstall -WindowStyle Hidden -RedirectStandardOutput $outputPath -RedirectStandardError (Join-Path $logRoot 'stderr.log') -PassThru -Wait
$result = [pscustomobject]@{ Project = $projectRootPath; Started = $started.ToString('o'); ExitCode = $process.ExitCode; Seconds = [math]::Round(((Get-Date)-$started).TotalSeconds,2); Log = $outputPath }
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $logRoot 'result.json') -Encoding utf8
$result | ConvertTo-Json
if ($process.ExitCode -ne 0) { exit 1 }
# A zero Java exit code alone does not establish successful generation: review
# generated files, the CLI log, compilation and retained-function comparisons.
