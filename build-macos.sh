#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"

# No Homebrew dependencies: only Apple's Command Line Tools and system frameworks.
# Build both CPU architectures by default; ARCHS=arm64 builds only Apple Silicon.
read -r -a architectures <<< "${ARCHS:-arm64 x86_64}"
task_build_dir="${BUILD_DIR:-build/macos}"
task_app="$task_build_dir/PokeStroller.app"
mkdir -p "$task_app/Contents/MacOS" "$task_app/Contents/Resources"
task_binaries=()
for arch in "${architectures[@]}"; do
    case "$arch" in arm64|x86_64) ;; *) echo "Unsupported architecture: $arch" >&2; exit 1 ;; esac
    task_obj="$task_build_dir/$arch"
    mkdir -p "$task_obj"
    task_cppflags=(-arch "$arch" -mmacosx-version-min=11.0 -std=c++23 -O2 -g
                   -fno-strict-aliasing -Ithird_party/pocketwalker -Isrc)
    task_objects=()
    while IFS= read -r source; do
        object="$task_obj/${source//\//_}.o"
        xcrun clang++ "${task_cppflags[@]}" -c "$source" -o "$object"
        task_objects+=("$object")
    done < <(find third_party/pocketwalker/core -name '*.cpp' -print)
    for source in src/emulator.cpp src/ir_link.cpp; do
        object="$task_obj/$(basename "$source").o"
        xcrun clang++ "${task_cppflags[@]}" -c "$source" -o "$object"
        task_objects+=("$object")
    done
    for source in src/macos_main.m src/session_store.m; do
        object="$task_obj/$(basename "$source").o"
        xcrun clang -arch "$arch" -mmacosx-version-min=11.0 -O2 -g -fobjc-arc -Wall -Wextra \
            -c "$source" -o "$object"
        task_objects+=("$object")
    done
    xcrun clang -arch "$arch" -mmacosx-version-min=11.0 -std=c11 -O2 -Wall -Wextra \
        -c src/macos_audio.c -o "$task_obj/audio.o"
    xcrun clang++ -arch "$arch" -mmacosx-version-min=11.0 "${task_objects[@]}" "$task_obj/audio.o" \
        -framework Cocoa -framework AudioToolbox -o "$task_obj/PokeStroller"
    task_binaries+=("$task_obj/PokeStroller")
done
xcrun lipo -create "${task_binaries[@]}" -output "$task_build_dir/PokeStroller.universal"
# Replace the executable's inode so rebuilding does not rewrite a running app's
# mapped code pages (which would invalidate that process's code signature).
mv "$task_build_dir/PokeStroller.universal" "$task_app/Contents/MacOS/PokeStroller"
cp macos/Info.plist "$task_app/Contents/Info.plist"
cp LICENSE "$task_app/Contents/Resources/LICENSE"
cp third_party/pocketwalker/LICENSE "$task_app/Contents/Resources/PocketWalker-LICENSE"
cp third_party/pocketwalker/UPSTREAM.md "$task_app/Contents/Resources/PocketWalker-NOTICE.md"
cp docs/MACOS.md "$task_app/Contents/Resources/LISEZ-MOI.md"
plutil -lint "$task_app/Contents/Info.plist"
# Ad-hoc signing permits local execution. This is not Developer ID notarization.
codesign --force --sign - "$task_app"
codesign --verify --strict "$task_app"
printf 'Application: %s\n' "$task_app"

if [[ "${1:-}" == "--package" ]]; then
    mkdir -p dist
    task_archive="dist/PokeStroller-macOS.zip"
    ditto -c -k --keepParent "$task_app" "$task_archive"
    (cd dist && shasum -a 256 PokeStroller-macOS.zip > SHA256SUMS)
    printf 'Archive: %s\n' "$task_archive"
elif [[ -n "${1:-}" ]]; then
    echo "Usage: $0 [--package]" >&2
    exit 1
fi
