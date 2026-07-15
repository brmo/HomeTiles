$ErrorActionPreference = 'Stop'

$coreVersion = '3.3.7'
$arduinoEsp32 = Join-Path $env:LOCALAPPDATA 'Arduino15\packages\esp32'
$ar = Get-ChildItem (Join-Path $arduinoEsp32 'tools') -Recurse -Filter 'riscv32-esp-elf-ar.exe' |
    Select-Object -First 1 -ExpandProperty FullName

if (-not $ar) {
    throw 'riscv32-esp-elf-ar.exe was not found in the Arduino ESP32 installation.'
}

$repoRoot = Split-Path $PSScriptRoot -Parent
$fixDirectories = @('esp-hosted-3.3.7-tx-fix', 'esp-hosted-3.3.7-rx-fix')
$variants = @('esp32p4-libs', 'esp32p4_es-libs')

foreach ($variant in $variants) {
    $archive = Join-Path $arduinoEsp32 "tools\$variant\$coreVersion\lib\libespressif__esp_hosted.a"
    if (-not (Test-Path -LiteralPath $archive)) {
        throw "ESP-Hosted archive not found: $archive"
    }

    $backup = "$archive.hometiles-unpatched-backup"
    if (-not (Test-Path -LiteralPath $backup)) {
        Copy-Item -LiteralPath $archive -Destination $backup
    }

    foreach ($fixDirectory in $fixDirectories) {
        $objects = Get-ChildItem -LiteralPath (Join-Path $repoRoot "tools\$fixDirectory\$variant") -Filter '*.obj'
        foreach ($object in $objects) {
            & $ar rs $archive $object.FullName
            if ($LASTEXITCODE -ne 0) {
                throw "Failed to inject $($object.Name) into $archive"
            }

            $verifyDirectory = Join-Path $env:TEMP ("hometiles-esp-hosted-verify-" + [Guid]::NewGuid())
            New-Item -ItemType Directory -Path $verifyDirectory | Out-Null
            Push-Location $verifyDirectory
            try {
                & $ar x $archive $object.Name
                if ($LASTEXITCODE -ne 0) {
                    throw "Failed to extract $($object.Name) from $archive"
                }
                $actual = (Get-FileHash -Algorithm SHA256 $object.Name).Hash
                $expected = (Get-FileHash -Algorithm SHA256 $object.FullName).Hash
                if ($actual -ne $expected) {
                    throw "Verification failed for $($object.Name) in $archive"
                }
            }
            finally {
                Pop-Location
                Remove-Item -LiteralPath $verifyDirectory -Recurse -Force
            }
        }
    }

    Write-Host "Patched and verified: $archive"
}
