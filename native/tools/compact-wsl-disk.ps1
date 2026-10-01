<#
.SYNOPSIS
  Return space freed INSIDE a WSL distro back to the Windows C: drive.

.DESCRIPTION
  A WSL2 distro lives in an ext4.vhdx that GROWS ON DEMAND AND NEVER SHRINKS.
  Deleting files inside the distro frees space for the distro and none for
  Windows: measured 2026-09-14, Ubuntu-24.04 dropped from 78 GB used to 51 GB
  after a cleanup while its .vhdx stayed at 81.2 GB and C: stayed at 11.3 GB
  free. Only a compaction hands those blocks back.

  `wsl --manage <d> --set-sparse true` is the modern alternative and WSL 2.7
  REFUSES it ("currently disabled due to potential data corruption") unless
  forced with --allow-unsafe. That force is deliberately not used here: this
  machine already has one corrupt SQLite index, and trading a disk-space
  problem for a second corruption risk is not a trade worth making.

  So this does the boring, safe thing: diskpart's `compact vdisk`, which only
  discards blocks the filesystem has already marked free. It needs elevation
  (hence this script) and the distro must be stopped, which it verifies rather
  than assumes.

.EXAMPLE
  # From an ADMIN PowerShell:
  .\compact-wsl-disk.ps1 -Distro Ubuntu-24.04

  # Or let it elevate itself (a UAC prompt appears):
  .\compact-wsl-disk.ps1 -Distro Ubuntu-24.04 -Elevate
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)][string]$Distro,
  [switch]$Elevate,
  # TERMINATING A DISTRO IS NOT ENOUGH.
  #
  # `wsl --terminate <d>` stops the distro but the SHARED WSL2 utility VM
  # (vmmemWSL) keeps the .vhdx open, so diskpart fails with "The process
  # cannot access the file because it is being used by another process" -
  # observed 2026-09-14 with the distro showing Stopped. Only `wsl --shutdown`
  # closes that handle, and it stops EVERY distro plus anything running in
  # them. That is a real cost, so it is opt-in rather than silent.
  [switch]$Shutdown
)

$ErrorActionPreference = 'Stop'

# RESOLVE wsl.exe BY PATH, NOT BY $env:PATH.
#
# Observed 2026-09-15 in the elevated child this script spawns: "The term
# 'wsl.exe' is not recognized", from a shell whose working directory was
# literally C:\WINDOWS\system32. The elevated context does not always inherit
# the PATH the user's shell had, and a 32-bit host would additionally need
# Sysnative (System32 redirects to SysWOW64, which has no wsl.exe). Both are
# avoided by naming the file.
function Get-WslExe {
  $cmd = Get-Command wsl.exe -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  foreach ($p in @("$env:SystemRoot\System32\wsl.exe",
                   "$env:SystemRoot\Sysnative\wsl.exe",
                   "$env:LOCALAPPDATA\Microsoft\WindowsApps\wsl.exe")) {
    if (Test-Path $p) { return $p }
  }
  throw "cannot find wsl.exe (looked on PATH, System32, Sysnative, WindowsApps)"
}
$WSL = Get-WslExe

function Get-DistroVhd {
  param([string]$Name)
  $lxss = 'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Lxss'
  foreach ($k in Get-ChildItem $lxss -ErrorAction SilentlyContinue) {
    $p = Get-ItemProperty $k.PSPath
    if ($p.DistributionName -eq $Name) { return (Join-Path $p.BasePath 'ext4.vhdx') }
  }
  throw "no distro named '$Name' is registered"
}

$vhd = Get-DistroVhd -Name $Distro
if (-not (Test-Path $vhd)) { throw "vhdx not found: $vhd" }

$before = (Get-Item $vhd).Length / 1GB
$freeBefore = (Get-PSDrive C).Free / 1GB
Write-Host ("vhdx  : {0}" -f $vhd)
Write-Host ("size  : {0:N1} GB     C: free: {1:N1} GB" -f $before, $freeBefore)

# The distro MUST be stopped: compacting a mounted disk is how you corrupt it.
# `wsl -l -v` marks the running ones; terminate just this distro so any OTHER
# distro (and anything running in it) is left alone.
$state = (& $WSL -l -v) -split "`n" | Where-Object { $_ -match [regex]::Escape($Distro) }
if ($state -match 'Running') {
  Write-Host "stopping $Distro (other distros are left running)"
  & $WSL --terminate $Distro | Out-Null
  Start-Sleep -Seconds 3
}

# The utility VM holds the vhdx even for a stopped distro (see -Shutdown).
$vm = Get-Process vmmemWSL -ErrorAction SilentlyContinue
if ($vm) {
  if ($Shutdown) {
    Write-Host "shutting down the WSL VM so the disk can be detached (this stops ALL distros)"
    & $WSL --shutdown | Out-Null
    Start-Sleep -Seconds 6
    if (Get-Process vmmemWSL -ErrorAction SilentlyContinue) {
      Write-Warning "vmmemWSL is still running; the compact below may fail"
    }
  } else {
    Write-Warning ("The WSL VM (vmmemWSL) is running and holds $Distro's disk open, " +
      "so diskpart cannot attach it. Re-run with -Shutdown to stop every distro " +
      "first, or run 'wsl --shutdown' yourself and try again.")
  }
}

$isAdmin = ([Security.Principal.WindowsPrincipal] `
            [Security.Principal.WindowsIdentity]::GetCurrent()
           ).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)

if (-not $isAdmin) {
  if ($Elevate) {
    # ASCII ONLY IN THIS FILE. Windows PowerShell 5.1 reads a .ps1 with no BOM
    # as ANSI (Windows-1252), so a UTF-8 em-dash (E2 80 94) arrives as the
    # three characters a-hat, euro, RIGHT DOUBLE QUOTATION MARK -- and that
    # last one is a smart quote, which the parser treats as a string
    # delimiter. One em-dash in a comment produced "The string is missing the
    # terminator" pointing 25 lines further down. Keep this file 7-bit.
    Write-Host "not elevated - relaunching with UAC"
    $childArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass',
                   '-File', $PSCommandPath, '-Distro', $Distro)
    if ($Shutdown) { $childArgs += '-Shutdown' }   # the flag must survive UAC
    Start-Process powershell -Verb RunAs -Wait -ArgumentList $childArgs
    $after = (Get-Item $vhd).Length / 1GB
    $nowFree = (Get-PSDrive C).Free / 1GB
    Write-Host ("size  : {0:N1} GB -> {1:N1} GB     C: free: {2:N1} GB" -f $before, $after, $nowFree)
    return
  }
  throw "diskpart's compact needs Administrator. Re-run from an admin shell, or pass -Elevate."
}

# diskpart speaks a script file, not arguments.
$script = Join-Path $env:TEMP ("compact-wsl-{0}.txt" -f [guid]::NewGuid())
@(
  ('select vdisk file="' + $vhd + '"')
  "attach vdisk readonly"      # read-only: nothing can write while compacting
  "compact vdisk"
  "detach vdisk"
  "exit"
) | Set-Content -Path $script -Encoding ascii

$out = ''
try {
  Write-Host "compacting (this can take several minutes on a large disk)..."
  $out = (& diskpart.exe /s $script 2>&1) -join [Environment]::NewLine
  $out -split [Environment]::NewLine |
    Where-Object { $_ -match 'percent|success|error|DiskPart has' } |
    ForEach-Object { Write-Host "  $_" }
} finally {
  Remove-Item $script -ErrorAction SilentlyContinue
}

# A FAILED COMPACT MUST NOT READ AS A NO-OP. The first version printed
# "recovered 0.0 GB" under diskpart's own error line, which looks like "there
# was nothing to reclaim" rather than "this did not run". Name the cause.
if ($out -match 'being used by another process') {
  Write-Host ''
  Write-Warning ("diskpart could not attach the disk: the WSL VM still has it open. " +
    "Re-run with -Shutdown (stops all distros), or run 'wsl --shutdown' first.")
  exit 1
}
if ($out -match 'DiskPart has encountered an error') {
  Write-Host ''
  Write-Warning "diskpart reported an error above; the disk was NOT compacted."
  exit 1
}

$after = (Get-Item $vhd).Length / 1GB
$freeAfter = (Get-PSDrive C).Free / 1GB
Write-Host ("size  : {0:N1} GB -> {1:N1} GB" -f $before, $after)
$gained = $freeAfter - $freeBefore
Write-Host ("C:    : {0:N1} GB -> {1:N1} GB free   (recovered {2:N1} GB)" -f $freeBefore, $freeAfter, $gained)
