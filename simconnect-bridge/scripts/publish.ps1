param(
  [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$bridgeRoot = Resolve-Path (Join-Path $scriptDir "..")
$publishDir = Join-Path $bridgeRoot "publish"

dotnet publish $bridgeRoot -c $Configuration -r win-x64 --self-contained true -p:PublishSingleFile=false -o $publishDir

Write-Host "Bridge published to $publishDir"
