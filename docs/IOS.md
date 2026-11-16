# ReStreet on iPhone and iPad

Status: **port code written, not yet run on a device.** The touch controls are tested on a desktop
(`Installer/recomp-template/src/ios/touch_pad_test.cpp`); the app, the SDK patch and the
display-clock code are compile-checked but have not been built with Xcode or played on an iPhone
yet. See [Open points](#open-points) before relying on it.

The iOS port reuses the iOS platform layer of the GTA V recompilation (`rexglue-sdk-gtav-linux.patch`
in gtav-recomp: UIKit main thread, guest memory in the app sandbox, MoltenVK, `args.txt`). Both
projects use ReXGlue v0.10.0, so it stacks on `patches/rexglue-local-changes.patch`.

## What it adds

| | |
|---|---|
| **Touch controls** | A football pad drawn over the game: floating move stick, A / B / X / Y with their actions, Sprint, Street Ball Control, flicks, a trick stick, D-pad, Back / Start. Hidden while a Bluetooth controller is connected. |
| **Locked 60 fps** | The game's vblank comes from the screen itself (CADisplayLink at exactly 60 Hz, also on 120 Hz ProMotion screens), through the SDK's existing `Presenter::WaitForMonitorVBlank` path that the PC build uses with DXGI. |
| **Phone paths** | Game files, saves, caches and logs live in the app's Documents folder (Finder / Files app). |

### Controls

| Touch | Pad | Action | Notes |
|---|---|---|---|
| Left side, anywhere | Left stick | Move | Floating: it centres where your thumb lands. Push past the outer ring to also **sprint**. |
| A (green) | A | Pass | |
| B (red) | B | Lob | |
| X (blue) | X | Shoot | Hold longer for more power. |
| Y (yellow) | Y | Through ball | |
| Sprint (big, bottom right) | RT | Sprint | |
| Control | LT | Street Ball Control | |
| Flick | RB | Flick-up modifier | |
| LB, R3 | LB, R3 | | As on the 360 pad. |
| Right side, free space | Right stick | Tricks | Floating, like the move stick. |
| D-pad (top left) | D-pad | | |
| Back / Start (top centre) | Back / Start | Pause | |

Slide a finger from one face button to another to press the new one (pass into shot without
lifting). Settings (`Documents/args.txt`, one per line):
`--touch_scale=0.6…1.3`, `--touch_opacity=0.1…1`, `--touch_inset=…` (distance from the notch side),
`--touch_sprint_on_push=false`, `--touch_button_slide=false`, `--touch_controls=false`.

### How the 60 fps lock works

The game presents once per guest vblank, so the vblank rate is the frame rate. On PC the SDK
patch already lets the vblank follow the monitor (`Presenter::WaitForMonitorVBlank`, DXGI
`WaitForVBlank`) when vsync is on; otherwise it is a timer at exactly 60.000 Hz. A timer drifts
against a phone screen's own clock, and every few seconds one frame is shown twice or skipped: a
visible stutter even at a steady "60 fps".

On iOS, `WaitForMonitorVBlank` (`patches/rexglue-ios.patch`) waits for the screen instead:
`src/ios/display_link.mm` runs a CADisplayLink locked to 60 Hz (`preferredFrameRateRange`
60/60/60) on its own high-priority thread. If the screen cannot run at that rate, the app is in
the background, or a wait times out, that wait falls back to the timer, as on PC. The vblank and
display-link threads run at `QOS_CLASS_USER_INTERACTIVE`, so they stay on the performance cores.
`MonitorVblankTiming` lines in the log (every 5 s) show how many refreshes were caught up.

This removes the drift and timer jitter. It cannot make a slow frame fast: if the GPU or the
recompiled code needs more than 16.7 ms for a frame, that frame still misses its refresh. On a
weaker phone lower the cost in `Documents/args.txt` (keep 1x internal resolution, no MSAA / SSAO).

## Requirements

- A Mac with Xcode (iOS SDK 17 or newer), CMake 3.25+, Ninja and `extract-xiso`.
- Your own FIFA Street (2012) Xbox 360 ISO.
- An iPhone or iPad with an A14 chip or newer (iPhone 12 and later), iOS 17+, about 12 GB free.
- An ios-arm64 MoltenVK dynamic framework (`MoltenVK.xcframework/ios-arm64/MoltenVK.framework`,
  from the Vulkan SDK or a MoltenVK release).
- To install: a development provisioning profile (Xcode), or Sideloadly / AltStore with your
  own Apple ID (free accounts re-sign every 7 days).

## Build

```sh
# 1. SDK for iOS: the pinned ReXGlue + the PC patch + the iOS layer.
git clone --recursive https://github.com/rexglue/rexglue-sdk.git ReXGlue
git -C ReXGlue checkout c94f5ebdcb3c9d1a460ca48e04f9758448f8d518
git -C ReXGlue submodule update --init --recursive
git -C ReXGlue apply ../patches/rexglue-local-changes.patch
git -C ReXGlue apply ../patches/rexglue-ios.patch
cd ReXGlue
# Host (macOS) build first: the codegen tool runs on the Mac.
cmake --preset mac-arm64 && cmake --build out/build/mac-arm64 && cmake --install out/build/mac-arm64
# iOS runtime libraries (no codegen tool, no Vulkan loader):
cmake -S . -B out/build/ios-arm64 -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DCMAKE_C_FLAGS=-mcpu=apple-a14 -DCMAKE_CXX_FLAGS=-mcpu=apple-a14 -DCMAKE_CXX_STANDARD=23 \
  -DREXGLUE_USE_VULKAN=ON -DREXGLUE_ENABLE_TRACY=OFF -DCMAKE_INSTALL_PREFIX=$PWD/out/install/ios-arm64
ninja -C out/build/ios-arm64 && cmake --install out/build/ios-arm64
# (the install registers the package in ~/.cmake/packages/rexglue: remove that entry, or a
#  later Mac configure can pick up the iOS SDK)
cd ..

# 2. Game files and recompiled code (on the Mac, as BuildFifaStreet.ps1 does on Windows)
extract-xiso -x -d "$PWD/dist/GameData" "FIFA Street.iso"
cp -R Installer/recomp-template work && cd work
#    write fifastreet_manifest.toml with game_root / file_path pointing at dist/GameData
#    (the template in BuildFifaStreet.ps1, "Preparing manifest"), then:
../ReXGlue/out/install/mac-arm64/bin/rexglue codegen fifastreet_manifest.toml

# 3. The game for iOS
cmake -S . -B ../build/ios -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0 \
  -DCMAKE_C_FLAGS=-mcpu=apple-a14 -DCMAKE_CXX_FLAGS=-mcpu=apple-a14 \
  -DCMAKE_PREFIX_PATH=$PWD/../ReXGlue/out/install/ios-arm64 \
  -DCMAKE_FIND_ROOT_PATH=$PWD/../ReXGlue/out/install/ios-arm64
ninja -C ../build/ios

# 4. App bundle (the PC settings file goes in with it; ios/args.txt overrides the iOS bits)
EXE=fifastreet NAME=ReStreet BUNDLE_ID=com.yourname.restreet IOS_BUILD=../build/ios \
  IOS_SDK=$PWD/../ReXGlue/out/install/ios-arm64 MOLTENVK_FW=<MoltenVK.framework> \
  CONFIG_TOML=../Installer/payload/Game/fifastreet.toml \
  ios/make_app.sh --ipa        # unsigned ipa for Sideloadly / AltStore
#  or: ios/make_app.sh --install   (signed with your Xcode profile, installed over USB)
```

What `patches/rexglue-ios.patch` contains (on top of `rexglue-local-changes.patch`): the POSIX /
ARM64 / iOS layer from gtav-recomp (`REX_PLATFORM_IOS`, sandbox-safe guest memory, `SDL_RunApp`
with `args.txt`, MoltenVK next to the executable, SDL uikit), the iOS branch of
`Presenter::WaitForMonitorVBlank`, and two merged files: the Vulkan command processor keeps both
projects' additions, and the input system keeps this project's `devices_mutex_` locking (GTA V's
second mutex for the same job is left out). It also carries GTA V options that default off
(`gpu_skip_rendering`, resolve dumps, the scripted test pad).

## Put the game on the phone

Copy the extracted ISO folder (`default.xex`, `fifadllzf.xex.dll`, `dlc/…`) into the app's
Documents as `GameData/`: Finder > your iPhone > Files > ReStreet (Mac), Apple Devices / iTunes
File Sharing (Windows), or the Files app. Layout:

```
Documents/GameData/     the extracted ISO
Documents/args.txt      optional: your own launch arguments (read after the app's)
Documents/userdata      saves (World Tour) and profile
Documents/cache         shader cache
Documents/logs          fifastreet.log
```

## Open points

- **Not run on a device yet.** The SDK's Vulkan backend has an Apple / MoltenVK mode (no geometry
  shaders or wireframe fill required: primitives are converted on the CPU), which iOS inherits,
  but this game has not been run through it on a phone. First things to check: rendering
  (`fbo` render-target path), memory (jetsam limits on 4 GB phones) and the frame time in a match.
- CI (`.github/workflows/ios-check.yml`) builds the patched SDK for iOS with Xcode, compiles the
  app's iOS sources and runs the touch pad test. It cannot build the game: that needs your copy.
- Online play and the launcher's social page are Windows programs; on the phone the game runs
  offline unless the online settings are passed in `Documents/args.txt`.
- The executable contains code recompiled from your copy of the game: never share the app or ipa.
