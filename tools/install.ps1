# The installer's engine: Setup.exe unpacks the collection's source into <Install>\source and
# runs this, hidden, reading its progress. It can also be run by hand.
#
#   install.ps1 -Install <folder> [-Sor1 <rom>] [-Sor2 <rom>] [-Sor3 <rom>] [-NoSystemTools]
#
# A game with other editions (Streets of Rage 3: Bare Knuckle III) takes several ROMs as
# "<id>=<rom>|<id>=<rom>", the ids of games/<name>/game.json.
#
# For each game given a ROM: checks the ROM, recompiles it to C and builds that game, then
# puts it in <Install>\games\<name>\ (its settings.ini remembers the ROM; settings, saves and
# states there are kept on a reinstall). The collection's front end goes in <Install>\.
# The build tools are used from PATH when they will do, else downloaded once into
# <Install>\source\toolchain (the publishers' own releases, checked against pinned SHA-256s;
# nothing is installed into Windows).
#
# Progress goes to stdout as "@@ <percent> <what>", a failure as "@@error <why>" (exit 1),
# and everything else the tools print as it comes.
param([Parameter(Mandatory)][string]$Install, [string]$Sor1, [string]$Sor2, [string]$Sor3, [switch]$NoSystemTools)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$Root = Split-Path -Parent $PSScriptRoot
$Tools = Join-Path $Root 'toolchain'
Set-Location $Root
# Windows' own curl and tar, not whichever comes first on PATH: Git's tar reads C:\... as a host.
$Curl = Join-Path $env:SystemRoot 'System32\curl.exe'
$Tar = Join-Path $env:SystemRoot 'System32\tar.exe'

function Progress($percent, $text) { [Console]::Out.WriteLine("@@ $percent $text"); [Console]::Out.Flush() }
function Say($text) { [Console]::Out.WriteLine($text); [Console]::Out.Flush() }

# ---- Tools ------------------------------------------------------------------------------
# Clang must be a MinGW one (llvm-mingw): the MSVC-flavoured LLVM would need Visual Studio's libraries.
$Needed = @(
    @{ Name = 'Node.js 22.20.0'; Dir = 'node'; Bin = '.'; Exe = 'node.exe'; MB = 34
       Url = 'https://nodejs.org/dist/v22.20.0/node-v22.20.0-win-x64.zip'
       Sha = 'bb819d6eb8f5bfda294bbc83a7e4ec6539da67c4233d54b0d655b9248b15e29d'
       Ok = { $v = (& node --version); $v -match '^v(\d+)' -and [int]$Matches[1] -ge 18 } },
    @{ Name = 'CMake 3.31.12'; Dir = 'cmake'; Bin = 'bin'; Exe = 'cmake.exe'; MB = 45
       Url = 'https://github.com/Kitware/CMake/releases/download/v3.31.12/cmake-3.31.12-windows-x86_64.zip'
       Sha = '0c4baa40f28b3f8225eb3fdf6946c987b4fe901403b4eaf2fbbd9378100aaa0c'
       Ok = { $v = (& cmake --version) | Select-Object -First 1; $v -match '(\d+)\.(\d+)' -and ([int]$Matches[1] -gt 3 -or [int]$Matches[2] -ge 20) } },
    @{ Name = 'Ninja 1.13.2'; Dir = 'ninja'; Bin = '.'; Exe = 'ninja.exe'; MB = 1
       Url = 'https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-win.zip'
       Sha = '07fc8261b42b20e71d1720b39068c2e14ffcee6396b76fb7a795fb460b78dc65'
       Ok = { $null -ne (& ninja --version) } },
    @{ Name = 'LLVM-MinGW 20260616 (Clang)'; Dir = 'llvm-mingw'; Bin = 'bin'; Exe = 'clang.exe'; MB = 179
       Url = 'https://github.com/mstorsjo/llvm-mingw/releases/download/20260616/llvm-mingw-20260616-ucrt-x86_64.zip'
       Sha = 'b9b68a4d276e16fa25802aaba458e4638f64b3884c290aaccdc2d87083b6ca35'
       Ok = { $m = (& clang -dumpmachine); $m -match 'mingw|windows-gnu' } }
)

function OnPath($tool) {
    if ($NoSystemTools) { return $false }
    try { return [bool](& $tool.Ok) } catch { return $false }
}

function Fetch($tool) {
    $zip = Join-Path $Tools "$($tool.Dir).zip"
    $tmp = Join-Path $Tools "$($tool.Dir).tmp"
    & $Curl -L --fail --silent --show-error -o $zip $tool.Url
    if ($LASTEXITCODE -ne 0) { throw "Could not download $($tool.Name). Check the internet connection and try again." }
    $sha = (Get-FileHash -Algorithm SHA256 $zip).Hash.ToLower()
    if ($sha -ne $tool.Sha) { Remove-Item $zip; throw "$($tool.Name): the download does not match its published SHA-256. Not using it." }
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
    New-Item -ItemType Directory $tmp | Out-Null
    & $Tar -xf $zip -C $tmp
    if ($LASTEXITCODE -ne 0) { throw "Could not unpack $($tool.Name)." }
    Remove-Item $zip
    $inner = @(Get-ChildItem $tmp)    # most zips hold one top folder; Ninja's holds ninja.exe alone
    $src = if ($inner.Count -eq 1 -and $inner[0].PSIsContainer) { $inner[0].FullName } else { $tmp }
    $dest = Join-Path $Tools $tool.Dir
    if (Test-Path $dest) { Remove-Item -Recurse -Force $dest }
    Move-Item $src $dest
    if (Test-Path $tmp) { Remove-Item -Recurse -Force $tmp }
}

# A native tool, its output passed on; throws with $why if it fails.
function Run($why) {
    $exe = $args[0]
    $rest = @($args | Select-Object -Skip 1)    # an array even with one argument
    # Windows PowerShell turns a native tool's stderr lines into errors when they are merged
    # into its output: the tools' warnings must not stop the install.
    $ErrorActionPreference = 'Continue'
    & $exe @rest 2>&1 | ForEach-Object { Say "$_" }
    $code = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'
    if ($code -ne 0) { throw $why }
}

# ---- The games ----------------------------------------------------------------------------
$Games = @(
    @{ Name = 'sor1'; Title = 'Streets of Rage'; Exe = 'StreetsOfRage.exe'; Rom = $Sor1 },
    @{ Name = 'sor2'; Title = 'Streets of Rage 2'; Exe = 'StreetsOfRage2.exe'; Rom = $Sor2 },
    @{ Name = 'sor3'; Title = 'Streets of Rage 3'; Exe = 'StreetsOfRage3.exe'; Rom = $Sor3 }
)

# A game's ROMs: "<rom>", or several editions as "<id>=<rom>|<id>=<rom>" (the game's own first).
function RomList($spec) {
    @($spec -split '\|' | Where-Object { $_ } | ForEach-Object {
        if ($_ -match '^([a-z]+)=(.+)$') { @{ Id = $Matches[1]; Path = $Matches[2] } } else { @{ Id = ''; Path = $_ } }
    })
}

# settings.ini: rom_path is the first ROM (the game's own if picked), and with editions each
# one's rom_<id>; the edition played stays if its ROM is still there.
function SetRomPath($ini, $roms) {
    $roms = @($roms)                    # (one ROM comes as itself, not a list)
    $utf8 = New-Object System.Text.UTF8Encoding $false
    $ids = @($roms | Where-Object { $_.Id } | ForEach-Object { $_.Id })
    $lines = @()
    if (Test-Path $ini) {
        $lines = @([IO.File]::ReadAllLines($ini, $utf8) | Where-Object {
            $_ -notmatch '^rom_path=' -and -not ($_ -match '^rom_([a-z]+)=' -and $ids -contains $Matches[1]) -and
            -not ($_ -match '^edition=(.*)$' -and $ids.Count -and -not ($ids -contains $Matches[1]))
        })
    }
    $head = @("rom_path=$($roms[0].Path)")
    if ($roms.Count -gt 1 -or ($roms[0].Id -and $roms[0].Id -ne 'main')) { $head += @($roms | ForEach-Object { "rom_$($_.Id)=$($_.Path)" }) }
    [IO.File]::WriteAllLines($ini, [string[]]($head + $lines), $utf8)
}

function Main {
    $chosen = @($Games | Where-Object { $_.Rom })
    if (-not $chosen.Count) { throw 'No game was picked.' }
    New-Item -ItemType Directory -Force $Install | Out-Null

    Progress 2 'Checking the build tools'
    $missing = @()
    foreach ($t in $Needed) {
        $portable = Join-Path (Join-Path (Join-Path $Tools $t.Dir) $t.Bin) $t.Exe
        if (Test-Path $portable) { $t.Path = Split-Path $portable }
        elseif (-not (OnPath $t)) { $missing += $t }
    }
    if ($missing.Count) {
        New-Item -ItemType Directory -Force $Tools | Out-Null
        $k = 0
        foreach ($t in $missing) {
            Progress (4 + [int](26 * $k / $missing.Count)) "Downloading $($t.Name) ($($t.MB) MB)"
            Fetch $t
            $t.Path = Join-Path (Join-Path $Tools $t.Dir) $t.Bin
            $k++
        }
    }
    foreach ($t in $Needed) { if ($t.Path) { $env:PATH = "$($t.Path);$env:PATH" } }
    if (-not (Test-Path (Join-Path $Root 'third_party\sdl2\CMakeLists.txt'))) { throw 'The source is incomplete: third_party is empty.' }

    $span = 66 / $chosen.Count
    for ($i = 0; $i -lt $chosen.Count; $i++) {
        $g = $chosen[$i]
        $at = 30 + [int]($span * $i)
        Progress $at "$($g.Title): checking your ROM"
        $roms = @(RomList $g.Rom)
        foreach ($r in $roms) { Run "That file is not $($g.Title) (or an edition of it), unchanged." node tools/setup.mjs --game $g.Name $r.Path }
        Progress ($at + [int]($span * .1)) "$($g.Title): finding its code"
        $env:GAME = $g.Name
        Run "$($g.Title): the disassembler failed." node tools/disasm.mjs
        Remove-Item Env:GAME
        $build = "build/$($g.Name)"
        if (-not (Test-Path (Join-Path $Root "$build\build.ninja"))) {
            Progress ($at + [int]($span * .2)) "$($g.Title): setting up the build"
            Run "$($g.Title): CMake could not set up the build." cmake -S . -B $build -G Ninja "-DGAME=$($g.Name)" -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
        }
        Progress ($at + [int]($span * .3)) "$($g.Title): recompiling and building (a few minutes)"
        Run "$($g.Title): the build failed." cmake --build $build
        $dest = Join-Path $Install "games\$($g.Name)"
        New-Item -ItemType Directory -Force $dest | Out-Null
        Copy-Item (Join-Path $Root "$build\$($g.Exe)") $dest -Force
        SetRomPath (Join-Path $dest 'settings.ini') $roms
        if ($g.Name -eq 'sor1') { Copy-Item (Join-Path $Root "$build\Streets of Rage Collection.exe") $Install -Force }
    }
    # The front end comes with the first game's build; with SoR1 skipped, build it on its own.
    if (-not (Test-Path (Join-Path $Install 'Streets of Rage Collection.exe'))) {
        Progress 96 'Building the collection'
        if (-not (Test-Path (Join-Path $Root 'build\sor1\build.ninja'))) {
            Run 'CMake could not set up the build.' cmake -S . -B build/sor1 -G Ninja -DGAME=sor1 -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
        }
        Run 'The collection failed to build.' cmake --build build/sor1 --target Collection
        Copy-Item (Join-Path $Root 'build\sor1\Streets of Rage Collection.exe') $Install -Force
    }
    Progress 100 'Done'
}

try { Main; exit 0 }
catch { [Console]::Out.WriteLine("@@error $($_.Exception.Message)"); exit 1 }
