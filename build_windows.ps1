param(
  [string]$VcpkgRoot = $env:VCPKG_INSTALLATION_ROOT
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $VcpkgRoot) { $VcpkgRoot = "C:\vcpkg" }
$Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
$Vcpkg = Join-Path $VcpkgRoot "vcpkg.exe"
if (!(Test-Path $Toolchain)) { throw "vcpkg toolchain not found: $Toolchain" }
if (!(Test-Path $Vcpkg)) { throw "vcpkg executable not found: $Vcpkg" }
python (Join-Path $Root "tools\fetch_seed.py") (Join-Path $Root "resources\seed.json")
python (Join-Path $Root "tools\build_db.py") (Join-Path $Root "resources\seed.json") (Join-Path $Root "resources\reference.db")
& $Vcpkg install sqlite3:x64-windows-static
cmake -S $Root -B (Join-Path $Root "build-win") -A x64 -DCMAKE_TOOLCHAIN_FILE=$Toolchain -DVCPKG_TARGET_TRIPLET=x64-windows-static -DAL_BUILD_TESTS=ON
cmake --build (Join-Path $Root "build-win") --config Release --parallel
Push-Location (Join-Path $Root "build-win\Release")
try {
  & ".\al-core-test.exe" (Join-Path $Root "resources\reference.db")
  if ($LASTEXITCODE -ne 0) { throw "Core test failed with exit code $LASTEXITCODE" }
} finally {
  Pop-Location
}
$Dist = Join-Path $Root "dist\windows"
New-Item -ItemType Directory -Force (Join-Path $Dist "resources") | Out-Null
Copy-Item (Join-Path $Root "build-win\Release\adventure-land-reference-os.exe") (Join-Path $Dist "AdventureLandReferenceOS.exe") -Force
Copy-Item (Join-Path $Root "resources\reference.db") (Join-Path $Dist "resources\reference.db") -Force
Write-Host "Built: $Dist\AdventureLandReferenceOS.exe"
