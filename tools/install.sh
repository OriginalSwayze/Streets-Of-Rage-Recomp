#!/usr/bin/env bash
# Streets of Rage Collection on Linux: what Setup.exe does on Windows. Run from the source
# (a release's source zip, or a clone with its submodules):
#
#   tools/install.sh --sor1 <rom> --sor2 <rom> --sor3 <rom>   (any of them)
#                    [--bk3 <rom>] [--bk3-en <rom>] [--bk3-dx <rom>]
#                    [--install <folder>] [--no-menu]
#   tools/install.sh --uninstall [--install <folder>] [--keep-saves]
#
# For each game given a ROM: checks the ROM, recompiles it to C and builds that game as a
# native Linux program, then puts it in <folder>/games/<name>/ (its settings.ini remembers
# the ROM; settings, saves and states there are kept on a reinstall). The collection's front
# end goes in <folder>, with an entry in the desktop's applications menu. The ROMs are copied
# into <folder>/roms, so the games keep working if the originals move. --bk3 is Bare Knuckle
# III (Japan), and --bk3-en and --bk3-dx its English and DX editions, already patched (as
# ROMs); they are built into Streets of Rage 3, so --sor3 is needed with them the first time.
#
# The folder is ~/.local/share/streets-of-rage-collection unless --install says otherwise.
# Running it again adds a game, or builds them again. The build tools come from the system's
# packages: it says which ones if any are missing.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL="${XDG_DATA_HOME:-$HOME/.local/share}/streets-of-rage-collection"
MENU=1 UNINSTALL=0 KEEP_SAVES=0
declare -A ROM=()

die() { echo "error: $*" >&2; exit 1; }
say() { echo "==> $*"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --sor1) ROM[sor1:main]="${2:?}"; shift 2 ;;
        --sor2) ROM[sor2:main]="${2:?}"; shift 2 ;;
        --sor3) ROM[sor3:us]="${2:?}"; shift 2 ;;
        --bk3) ROM[sor3:jp]="${2:?}"; shift 2 ;;
        --bk3-en) ROM[sor3:en]="${2:?}"; shift 2 ;;
        --bk3-dx) ROM[sor3:dx]="${2:?}"; shift 2 ;;
        --install) INSTALL="${2:?}"; shift 2 ;;
        --no-menu) MENU=0; shift ;;
        --uninstall) UNINSTALL=1; shift ;;
        --keep-saves) KEEP_SAVES=1; shift ;;
        -h|--help) sed -n '2,22p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) die "unknown option $1 (see --help)" ;;
    esac
done
INSTALL="$(mkdir -p "$INSTALL" && cd "$INSTALL" && pwd)"
DESKTOP="${XDG_DATA_HOME:-$HOME/.local/share}/applications/streets-of-rage-collection.desktop"
TITLES=([1]="Streets of Rage" [2]="Streets of Rage 2" [3]="Streets of Rage 3")
EXES=([1]=StreetsOfRage [2]=StreetsOfRage2 [3]=StreetsOfRage3)

if [ $UNINSTALL = 1 ]; then
    rm -rf "$INSTALL/roms" "$INSTALL/Streets of Rage Collection" "$DESKTOP"
    for g in 1 2 3; do rm -f "$INSTALL/games/sor$g/${EXES[$g]}"; done
    if [ $KEEP_SAVES = 0 ]; then rm -rf "$INSTALL/games"; rmdir "$INSTALL" 2>/dev/null || true
    else say "Settings, saves and states are kept in $INSTALL/games"; fi
    say "Removed."
    exit 0
fi
[ ${#ROM[@]} -gt 0 ] || die "no ROM given (see --help)"
for k in "${!ROM[@]}"; do [ -f "${ROM[$k]}" ] || die "no such file: ${ROM[$k]}"; done

# ---- The build tools -----------------------------------------------------------------------
missing=()
version_at_least() {    # <have> <want>: dotted versions
    [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -1)" = "$2" ]
}
node_v=$(node --version 2>/dev/null | sed 's/^v//') || true
[ -n "$node_v" ] && version_at_least "$node_v" 18 || missing+=("Node.js 18 or newer")
cmake_v=$(cmake --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+(\.[0-9]+)?') || true
[ -n "$cmake_v" ] && version_at_least "$cmake_v" 3.20 || missing+=("CMake 3.20 or newer")
command -v ninja >/dev/null || missing+=("Ninja")
command -v clang >/dev/null && command -v clang++ >/dev/null || missing+=("Clang")
# SDL is built from the source here and draws through X11 or Wayland and plays through
# PulseAudio (PipeWire's too) or ALSA: their headers decide what it can use.
has_header() { echo "#include <$1>" | cc -E -x c - >/dev/null 2>&1; }
command -v cc >/dev/null || missing+=("a C compiler (cc)")
if command -v cc >/dev/null; then
    has_header X11/Xlib.h || has_header wayland-client.h || missing+=("the X11 or Wayland development headers")
    has_header pulse/pulseaudio.h || has_header alsa/asoundlib.h || missing+=("the PulseAudio or ALSA development headers")
fi
if [ ${#missing[@]} -gt 0 ]; then
    echo "Missing: ${missing[*]}." >&2
    id_like=$( . /etc/os-release 2>/dev/null; echo "${ID:-} ${ID_LIKE:-}")
    case "$id_like" in
        *debian*|*ubuntu*) echo "Install them with:  sudo apt install nodejs cmake ninja-build clang libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxss-dev libwayland-dev libxkbcommon-dev libdecor-0-dev libasound2-dev libpulse-dev libudev-dev" >&2 ;;
        *fedora*|*rhel*) echo "Install them with:  sudo dnf install nodejs cmake ninja-build clang libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXi-devel libXScrnSaver-devel wayland-devel libxkbcommon-devel libdecor-devel alsa-lib-devel pulseaudio-libs-devel systemd-devel" >&2 ;;
        *arch*) echo "Install them with:  sudo pacman -S --needed nodejs cmake ninja clang libx11 libxext libxrandr libxcursor libxi libxss wayland libxkbcommon libdecor alsa-lib libpulse" >&2 ;;
        *suse*) echo "Install them with:  sudo zypper install nodejs cmake ninja clang libX11-devel libXext-devel libXrandr-devel libXcursor-devel libXi-devel libXss-devel wayland-devel libxkbcommon-devel alsa-devel libpulse-devel" >&2 ;;
        *) echo "Install them with your distribution's package manager, then run this again." >&2 ;;
    esac
    exit 1
fi
[ -f "$ROOT/third_party/sdl2/CMakeLists.txt" ] || die "the source is incomplete: third_party is empty (git submodule update --init --recursive)"

# ---- settings.ini: as Setup writes it ------------------------------------------------------
# rom_path is the first ROM (the game's own if picked), and with editions each one's
# rom_<id>; the edition played stays if its ROM is still there. The rest is kept.
set_rom_path() {        # <ini> <id=path>...
    local ini=$1; shift
    local ids=() head=()
    for r in "$@"; do ids+=("${r%%=*}"); done
    head+=("rom_path=${1#*=}")
    if [ $# -gt 1 ] || [ "${ids[0]}" != main ]; then for r in "$@"; do head+=("rom_${r%%=*}=${r#*=}"); done; fi
    local keep=""
    if [ -f "$ini" ]; then
        keep=$(IDS=" ${ids[*]} " awk '
            { sub(/\r$/, "") }
            tolower($0) ~ /^rom_path=/ { next }
            match(tolower($0), /^rom_[a-z]+=/) { id = substr(tolower($0), 5, RLENGTH - 5); if (index(ENVIRON["IDS"], " " id " ")) next }
            tolower($0) ~ /^edition=/ { v = tolower(substr($0, 9)); if (!index(ENVIRON["IDS"], " " v " ")) next }
            { print }' "$ini")
    fi
    { printf '%s\n' "${head[@]}"; [ -n "$keep" ] && printf '%s\n' "$keep"; } > "$ini.new"
    mv "$ini.new" "$ini"
}

# ---- The games ------------------------------------------------------------------------------
cd "$ROOT"
mkdir -p "$INSTALL/roms" "$INSTALL/games"
picked=()
for g in 1 2 3; do for k in "${!ROM[@]}"; do [[ $k == sor$g:* ]] && { picked+=($g); break; }; done; done
for g in "${picked[@]}"; do
    name=sor$g title=${TITLES[$g]} exe=${EXES[$g]}
    say "$title: checking your ROM"
    roms=()
    for id in main us jp en dx; do
        src=${ROM[$name:$id]:-}
        [ -n "$src" ] || continue
        if [ $id = main ] || [ $id = us ]; then dest="$INSTALL/roms/$name.md"; else dest="$INSTALL/roms/$name-$id.md"; fi
        [ "$(realpath "$src")" = "$(realpath -m "$dest")" ] || cp "$src" "$dest"
        node tools/setup.mjs --game $name "$dest" || die "that file is not $title (or an edition of it), unchanged"
        roms+=("$id=$dest")
    done
    # An edition picked before stays built in: its ROM is still in roms/.
    if [ $name = sor3 ]; then
        for id in jp en dx; do
            [ -n "${ROM[$name:$id]:-}" ] || [ ! -f "$INSTALL/roms/$name-$id.md" ] && continue
            node tools/setup.mjs --game $name "$INSTALL/roms/$name-$id.md" >/dev/null && roms+=("$id=$INSTALL/roms/$name-$id.md")
        done
        if [ -z "${ROM[$name:us]:-}" ]; then
            [ -f "$INSTALL/roms/$name.md" ] || die "Bare Knuckle III is built on Streets of Rage 3 (USA): give --sor3 too"
            node tools/setup.mjs --game $name "$INSTALL/roms/$name.md" >/dev/null
            roms=("us=$INSTALL/roms/$name.md" "${roms[@]}")
        fi
    fi
    say "$title: finding its code"
    GAME=$name node tools/disasm.mjs || die "$title: the disassembler failed"
    if [ ! -f "build/$name/build.ninja" ]; then
        say "$title: setting up the build"
        cmake -S . -B "build/$name" -G Ninja -DGAME=$name -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ || die "$title: CMake could not set up the build"
    fi
    say "$title: recompiling and building (a few minutes)"
    cmake --build "build/$name" || die "$title: the build failed"
    mkdir -p "$INSTALL/games/$name"
    install -m 755 "build/$name/$exe" "$INSTALL/games/$name/$exe"
    set_rom_path "$INSTALL/games/$name/settings.ini" "${roms[@]}"
    [ $name = sor1 ] && install -m 755 "build/sor1/Streets of Rage Collection" "$INSTALL/"
done
# The front end comes with Streets of Rage's build; without it, build it on its own.
if [ ! -f "$INSTALL/Streets of Rage Collection" ]; then
    say "Building the collection"
    [ -f build/sor1/build.ninja ] || cmake -S . -B build/sor1 -G Ninja -DGAME=sor1 -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
    cmake --build build/sor1 --target Collection
    install -m 755 "build/sor1/Streets of Rage Collection" "$INSTALL/"
fi

if [ $MENU = 1 ]; then
    mkdir -p "$(dirname "$DESKTOP")"
    cat > "$DESKTOP" <<EOF
[Desktop Entry]
Type=Application
Name=Streets of Rage Collection
Comment=Streets of Rage 1, 2 and 3, recompiled from your own ROMs
Exec="$INSTALL/Streets of Rage Collection"
Path=$INSTALL
Icon=applications-games
Categories=Game;ActionGame;
Terminal=false
EOF
fi
say "Done. Streets of Rage Collection is in $INSTALL$([ $MENU = 1 ] && echo ", and in your applications menu")."
