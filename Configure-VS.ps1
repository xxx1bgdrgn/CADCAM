[CmdletBinding()]
param(
    [ValidateSet('CADCAM2D', 'CADCAM3D', 'All')]
    [string]$Project = 'All',
    [string]$CMakePath
)

$ErrorActionPreference = 'Stop'
if (!$CMakePath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $installation = & $vswhere -version '[18.0,19.0)' -latest -products '*' -property installationPath
        if ($installation) {
            $candidate = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
            if (Test-Path -LiteralPath $candidate) { $CMakePath = $candidate }
        }
    }
    if (!$CMakePath) { $CMakePath = (Get-Command cmake -ErrorAction Stop).Source }
}

$projects = if ($Project -eq 'All') { 'CADCAM2D', 'CADCAM3D' } else { $Project }
foreach ($name in $projects) {
    $source = Join-Path $PSScriptRoot $name
    $build = Join-Path $source 'build/vs2026'
    & $CMakePath -S $source -B $build -G 'Visual Studio 18 2026' -A x64 -T v145
    if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed for $name." }

    [xml]$projectXml = Get-Content -Raw -LiteralPath (Join-Path $build "$name.vcxproj")
    $guid = $projectXml.SelectSingleNode('//*[local-name()="ProjectGuid"]').InnerText
    $configurations = $projectXml.SelectNodes('//*[local-name()="ProjectConfiguration"]') |
        ForEach-Object { $_.Include }

    $solution = @(
        'Microsoft Visual Studio Solution File, Format Version 12.00'
        '# Visual Studio Version 18'
        'VisualStudioVersion = 18.0.0.0'
        'MinimumVisualStudioVersion = 10.0.40219.1'
        ('Project("{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}") = "{0}", "build\vs2026\{0}.vcxproj", "{1}"' -f $name, $guid)
        'EndProject'
        'Global'
        "`tGlobalSection(SolutionConfigurationPlatforms) = preSolution"
        $configurations | ForEach-Object { "`t`t$_ = $_" }
    )
    $solution += @(
        "`tEndGlobalSection"
        "`tGlobalSection(ProjectConfigurationPlatforms) = postSolution"
        foreach ($config in $configurations) {
            "`t`t$guid.$config.ActiveCfg = $config"
            "`t`t$guid.$config.Build.0 = $config"
        }
        "`tEndGlobalSection"
        'EndGlobal'
    )
    $solutionPath = Join-Path $source "$name.sln"
    Set-Content -LiteralPath $solutionPath -Value $solution -Encoding ASCII
    Write-Output $solutionPath
}
