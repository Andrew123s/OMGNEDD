# Building and installing

## What you need

| | |
| --- | --- |
| **Windows** | Visual Studio 2022 with the *Desktop development with C++* workload, CMake 3.22 or newer, Git |
| **macOS** | Xcode 14 or newer, CMake 3.22 or newer |
| **Linux** | GCC 11 or newer, CMake 3.22, Ninja, and the packages listed below |

JUCE 8.0.8 is expected as a checkout at `JUCE/` inside this repository. It is
not committed here and is excluded by `.gitignore`.

```bash
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git JUCE
```

---

## Windows, which is what this is for

From a *Developer Command Prompt for VS 2022*, in the repository root:

```bat
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git JUCE

cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

That produces:

```
build\OMGNEDD_artefacts\Release\VST3\OMGNEDD.vst3
```

`OMGNEDD.vst3` is a folder, not a single file. Copy the whole folder.

### Installing

Copy it into the system VST3 folder:

```
C:\Program Files\Common Files\VST3\
```

so that you end up with `C:\Program Files\Common Files\VST3\OMGNEDD.vst3\`.
Copying there needs an administrator command prompt, or use Explorer and accept
the prompt:

```bat
xcopy /E /I /Y "build\OMGNEDD_artefacts\Release\VST3\OMGNEDD.vst3" "C:\Program Files\Common Files\VST3\OMGNEDD.vst3"
```

To have CMake do it on every build instead, set `COPY_PLUGIN_AFTER_BUILD TRUE`
in `CMakeLists.txt`.

### Making FL Studio find it

1. Open FL Studio.
2. **Options → Manage plugins**.
3. Check that `C:\Program Files\Common Files\VST3` is in the *Plugin search
   paths* list. Add it if it is not.
4. Click **Find more plugins**. Tick *Verify plugins* if you want FL Studio to
   report anything it could not load.
5. When the scan finishes, OMGNEDD appears under **Effects**. Drag it onto a
   mixer insert, or open the mixer track, click an empty slot and pick it.

If you rebuild while FL Studio is open, FL Studio keeps the old copy loaded.
Close FL Studio before copying the new build in.

### Using the sidechain in FL Studio

1. Put OMGNEDD on the mixer track carrying the vocal.
2. Route the track you want to trigger from into OMGNEDD's track: click the
   source track, then right-click the arrow under OMGNEDD's track and choose
   **Sidechain to this track**.
3. In the plugin's ADV panel, turn **EXTERNAL SC** on. The status line at the
   bottom of that panel says whether the sidechain is connected.
4. SC AMOUNT blends between the vocal itself and the sidechain as the detector
   source.

---

## macOS

```bash
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -G Xcode
cmake --build build --config Release
```

The VST3 lands in `build/OMGNEDD_artefacts/Release/VST3/`. Copy it to
`~/Library/Audio/Plug-Ins/VST3/`.

---

## Linux

```bash
sudo apt-get install -y \
  libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev libcurl4-openssl-dev \
  libxcomposite-dev

git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

The VST3 lands in `build/OMGNEDD_artefacts/Release/VST3/`. Copy it to
`~/.vst3/`.

---

## The other targets

```bash
# the verification suite (--quick skips the knob sweep, --report prints it)
cmake --build build --target OMGNEDD_Tests
./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests

# the CPU benchmark, which has to be enabled at configure time
cmake -B build -DCMAKE_BUILD_TYPE=Release -DOMGNEDD_BUILD_BENCH=ON
cmake --build build --target OMGNEDD_Bench
./build/OMGNEDD_Bench_artefacts/Release/OMGNEDD_Bench
```

On Windows the executables are under
`build\OMGNEDD_Tests_artefacts\Release\OMGNEDD_Tests.exe`.

On a headless Linux machine the verification target needs a display, because it
builds and paints the real editor:

```bash
xvfb-run -a ./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests
```

See [TESTING.md](TESTING.md).

---

## Troubleshooting

**FL Studio does not list OMGNEDD after a scan.**
Check that you copied the whole `OMGNEDD.vst3` *folder*, not just the `.vst3`
file inside it, and that the path you copied it to is in FL Studio's plugin
search paths. Run the scan again with *Verify plugins* ticked; FL Studio will
then say what it rejected and why.

**The scan finds it but it fails to load.**
You almost certainly built a Debug configuration, or built 32-bit. Build with
`--config Release` and `-A x64`.

**`add_subdirectory given source "JUCE" which is not an existing directory`.**
The JUCE checkout is missing. Run the `git clone` line above from the repository
root, so that `JUCE/CMakeLists.txt` exists.

**The build fails in `juceaide`.**
CMake is building a small helper tool first. On Linux this means a development
package is missing; install the list above. On Windows it usually means the
Desktop C++ workload is not installed.

**The plugin loads but the interface is blank or the wrong size.**
Reset the window with **SETTINGS → 100 %**. The editor size is stored with the
session, so a project saved on a much larger screen can open off-screen.

**It sounds quiet, or the level jumps when I move DRIVE.**
It should not: every engine has automatic level matching, so DRIVE and DEPTH
change the character rather than the level. If it does, check INPUT GAIN and
the dry/wet levels in the ADV panel's GAIN STAGING group, and check that MIX is
where you think it is: the MIX module prints the dry and wet percentages under
the knob.

**A synced wobble or delay drifts against the beat.**
It follows the host transport. Check that the FL Studio project tempo is what
you think it is and that the song is playing; when the transport is stopped the
LFO free-runs at the synced rate and cannot know where the beat is.

**The reverb or delay tail cuts off at the end of a clip.**
The plugin reports an 8 s tail. If FL Studio still cuts it, render with
*Leave remainder* (tail) enabled in the export dialog.

**High CPU.**
Turn oversampling down. The figures are in [TESTING.md](TESTING.md#cpu). 2x is
enough for anything short of extreme CRUSH and FOLD settings, and the sound does
not change with the setting, only the aliasing does.
