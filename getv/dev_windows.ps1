<#
  Incremental edit/build/play loop for an existing Windows build.

    .\getv\dev_windows.ps1                 # changed game C + port + link + play
    .\getv\dev_windows.ps1 -NoRun          # build only
    .\getv\dev_windows.ps1 -Full           # rebuild everything, then play

  This uses build_windows.ps1 for the actual compiler flags and linker. The
  focused game target recompiles only changed game translation units. Port
  sources are rebuilt as a batch because they are relatively few and may share
  headers. Source patches adding/changing game headers, generated assets,
  compiler flags, or patch setup require -Full.
#>
[CmdletBinding()]
param(
  [switch]$Full,
  [switch]$NoRun,
  [string]$Mingw = 'C:\msys64\mingw64',
  [int]$Jobs = 0
)

$ErrorActionPreference = 'Stop'
$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $here
$decomp = Join-Path $root 'vendor\ge-decomp'
$objDir = Join-Path $here 'build-windows\obj'
$exe = Join-Path $here 'build-windows\goldeneye.exe'
$builder = Join-Path $here 'build_windows.ps1'

# gcc.exe is called by its full path, but it still launches cc1.exe via PATH.
$toolBin = Join-Path $Mingw 'bin'
if ($env:PATH -notlike "*$toolBin*") { $env:PATH = "$toolBin;$env:PATH" }

function Invoke-Builder {
  param([string]$Target, [string[]]$Sources = @())
  Write-Host "`n== $Target =="
  if ($Target -eq 'game') {
    & $builder -Target game -GameSource $Sources -Mingw $Mingw -Jobs $Jobs
  } else {
    & $builder -Target $Target -Mingw $Mingw -Jobs $Jobs
  }
  if (-not $?) { throw "build target $Target failed" }
}

if (-not (Test-Path $exe) -or -not (Test-Path $objDir)) {
  Write-Host 'No previous build found; doing the initial full build.'
  $Full = $true
}

if (-not $Full) {
  $gameObjects = @(Get-ChildItem $objDir -Filter 'game_*.o' -File)
  if ($gameObjects.Count -eq 0) {
    Write-Host 'No game objects found; doing a full build.'
    $Full = $true
  } else {
    # A changed game header may affect many source files. The oldest existing
    # game object is the conservative boundary; rebuild fully if any header
    # is newer. Assets use their own objects and require the same treatment.
    $oldestGame = ($gameObjects | Sort-Object LastWriteTimeUtc | Select-Object -First 1).LastWriteTimeUtc
    $headers = @(Get-ChildItem -Path @((Join-Path $decomp 'src'), (Join-Path $decomp 'include')) `
                     -Recurse -Filter '*.h' -File -ErrorAction SilentlyContinue |
                 Where-Object { $_.LastWriteTimeUtc -gt $oldestGame })
    if ($headers.Count -gt 0) {
      Write-Host "Changed game header ($($headers[0].Name)); doing a full build."
      $Full = $true
    }

    if (-not $Full) {
      $changedGu = @(Get-ChildItem (Join-Path $decomp 'src\libultra\gu') `
                      -Filter '*.c' -File -ErrorAction SilentlyContinue |
                     Where-Object { $_.LastWriteTimeUtc -gt $oldestGame })
      if ($changedGu.Count -gt 0) {
        Write-Host "Changed libultra/gu source ($($changedGu[0].Name)); doing a full build."
        $Full = $true
      }
    }

    if (-not $Full) {
      $assetObjects = @(Get-ChildItem $objDir -Filter 'asset_*.o' -File)
      if ($assetObjects.Count -eq 0) {
        Write-Host 'No asset objects found; doing a full build.'
        $Full = $true
      } else {
        $oldestAsset = ($assetObjects | Sort-Object LastWriteTimeUtc | Select-Object -First 1).LastWriteTimeUtc
        $changedAssets = @(Get-ChildItem (Join-Path $decomp 'assets') -Recurse -Filter '*.c' `
                             -File -ErrorAction SilentlyContinue |
                           Where-Object { $_.LastWriteTimeUtc -gt $oldestAsset })
        if ($changedAssets.Count -gt 0) {
          Write-Host "Changed asset source ($($changedAssets[0].Name)); doing a full build."
          $Full = $true
        }
      }
    }
  }
}

if ($Full) {
  Invoke-Builder all
} else {
  $changedGame = @()
  $skip = '(ramromreplay|audi|usb|rmon|sched|ramrom|init|indy_comms|indy_commands|crash|spectrum|tlb_manage)\.c$'
  Push-Location $decomp
  try {
    $gameFiles = @(Get-ChildItem 'src' -Recurse -Filter '*.c' -File |
      Where-Object { $_.Name -notlike '._*' -and
                     $_.FullName -notmatch '[\\/]src[\\/]libultra(re)?[\\/]' -and
                     $_.Name -notin @('ge_layout_audit.c','ge_asset_fileview_check.c') -and
                     $_.Name -notmatch $skip })
    foreach ($file in $gameFiles) {
      $relative = (Resolve-Path -Relative $file.FullName)
      $stem = ($relative -replace '[\\/]','_') -replace ':','' -replace '\.c$',''
      $object = Join-Path $objDir ("game_$stem.o")
      if (-not (Test-Path $object) -or $file.LastWriteTimeUtc -gt (Get-Item $object).LastWriteTimeUtc) {
        $changedGame += $relative
      }
    }
  } finally { Pop-Location }

  if ($changedGame.Count -gt 0) {
    Write-Host "Recompiling $($changedGame.Count) changed game source(s)."
    Invoke-Builder game $changedGame
  } else {
    Write-Host 'No game sources changed.'
  }
  Invoke-Builder port
  Invoke-Builder app
}

if (-not $NoRun) {
  Push-Location $root
  try {
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "game exited with code $LASTEXITCODE" }
  } finally { Pop-Location }
}
