# Claude Clock - installer for Windows (PowerShell).
#   irm https://raw.githubusercontent.com/BolgarMaxym97/claude-code-desk-clock/main/install/install.ps1 | iex
# Downloads claude-clock.exe from the latest GitHub release, checks its sha256, puts it in
# %LOCALAPPDATA%\Programs\claude-clock (added to your user PATH) and runs `claude-clock setup`.
$ErrorActionPreference = 'Stop'
$repo = 'BolgarMaxym97/claude-code-desk-clock'
$arch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'arm64' } else { 'amd64' }
$name = "claude-clock-windows-$arch.exe"
$base = "https://github.com/$repo/releases/latest/download"
$dir = Join-Path $env:LOCALAPPDATA 'Programs\claude-clock'
$exe = Join-Path $dir 'claude-clock.exe'
$tmp = Join-Path ([IO.Path]::GetTempPath()) ([Guid]::NewGuid())
New-Item -ItemType Directory -Force -Path $tmp, $dir | Out-Null
try {
    Write-Host "Downloading $name..."
    Invoke-WebRequest -UseBasicParsing "$base/$name" -OutFile "$tmp\$name"
    Invoke-WebRequest -UseBasicParsing "$base/checksums.txt" -OutFile "$tmp\checksums.txt"
    $want = (Get-Content "$tmp\checksums.txt" | Where-Object { $_ -match " $([regex]::Escape($name))$" }) -split ' ' | Select-Object -First 1
    $have = (Get-FileHash -Algorithm SHA256 "$tmp\$name").Hash
    if (-not $want -or $want.ToLower() -ne $have.ToLower()) { throw 'Checksum mismatch, not installing.' }
    Copy-Item -Force "$tmp\$name" $exe
} finally { Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue }
Write-Host "Installed $exe"
$userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
if (-not ($userPath -split ';' | Where-Object { $_ -eq $dir })) {
    [Environment]::SetEnvironmentVariable('Path', ($userPath.TrimEnd(';') + ";$dir"), 'User')
    Write-Host "Added $dir to your PATH (open a new terminal to use 'claude-clock')."
}
& $exe setup
