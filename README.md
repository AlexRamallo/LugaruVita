# Lugaru Vita

This is a port of [Lugaru](https://osslugaru.gitlab.io/) to the PS Vita by [AlexRamallo](https://aramallo.com).

This includes a lot of major refactoring of the original LugaruOSS codebase. The original game ran on a single thread, and I had to refactor many systems to support multithreading, otherwise it would not be playable on the Vita. I also made zero effort to support desktops/other platforms, so it currently only builds for Vita, and many of the changes I did make likely introduced subtle changes to the original gameplay. Therefore, merging this with upstream is probably not going to happen any time soon.

This port was made possible thanks to:

* [Rinnegatamante's VitaGL](https://github.com/Rinnegatamante/vitaGL)
* [NorthFear's VitaGL adaptation of SDL2](https://github.com/Northfear/SDL/tree/vitagl)
* And of course the [Wolfire Games](wolfire.com) and the [OSS Lugaru contributors](AUTHORS)

# Installing

Just install the VPK on your Vita or VitaTV.

# Controls

I highly recommend that you play the tutorial!

* The main menu uses a cursor, which you can control with the left analog stick and by swiping on the touch screen.
* Square: attack **and select menu item**
* Cross: jump
* Triangle: sheath/unsheath weapon
* Circle: throw/pickup weapon
* Right shoulder: crouch and reverse attacks
* Down+Left Shoulder+Square: dodge-roll backwards

# Performance

The game is fully playable at a mostly stable 60 FPS on stock clock speeds. Slowdowns occur on maps with lots of characters and objects, such as the Sky Palace in the Empire campaign (however it is still around 40 FPS on stock clocks). Other slow downs may occur when there are a lot of ragdolls active at once.

Less intensive maps, like the majority of the main campaign + challenges, can even be played at 60 FPS while *underclocked*!

The inital load when you start the game is pretty slow due to the way the game loads audio files, but after that the load times aren't too bad.

# Known bugs/issues

There are a couple of issues to be aware of. I'm releasing it as-is because the game is fully playable. If I ever decide to dedicate more time to this project, these are high on my list of priorities to fix (PRs are welcome though!)

* **Major:** Random crash occurs sometimes when throwing a knife, possibly an overflow of a std container. Was a rare occurrence in my testing, and when it happens all you lose is the progress for that level.

* **Major:** Crashes when loading a level after playing for a while. Most likely a memory leak.

* **Minor:** Multi-layer terrain rendering is disabled for performance reasons. It can be enabled by disabling the `DRAW_SPEEDHACK` flag for vitaGL, but that has a noticeable impact on performance for most maps.

* **Minor:** Animation jittering occurs some times, possibly a race condition introduced late into the optimization process. Since it doesn't cause problems beyond occasional visual ugliness, it wasn't a high priority.

# Building

The original build system was replaced with waf in order to more easily support inclusion of an asset processor (for transcoding textures). Waf is included in the repo (it's a Python script), so there's no need to download it.

Builds tested on Ubuntu only, but it might still work on Windows/Mac with minor modifications to the instructions below.

## Dependencies

* Python 3.7+ and Pip
* "Pillow" library from Pip
* [PVRTexTool from Imagination Technologies](https://developer.imaginationtech.com/pvrtextool/). You can create a free account to download it. (make sure `PVRTexToolCLI` is in your PATH)
* [VitaSDK](https://vitasdk.org) (make sure the `VITASDK` environment variable is present)
* [SDL2 VitaGL library by NorthFear](https://github.com/Northfear/SDL/tree/vitagl)

## Installing Pillow

Depending on how pip is installed, you may need one of the following variations:

```
pip install pillow
#or
python3 -m pip install pillow
#or
pip3 install pillow
#or...
```

## Building SDL2-VitaGL

```
git clone https://github.com/Northfear/SDL sdl2-vita --branch vitagl
cd sdl2-vita
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DBUILD_TYPE=Release
make install
```

## Building a release VPK

NOTE: `waf` is a Python script which requires Python 3.7 or later. You might have to use `python3 ./waf` instead, depending on how your system is configured.

```
./waf configure_release
./waf build_release --tex-quality=pvrtcbest

#Alternatively (only if you have VitaCompanion installed on your Vita/VitaTV)

./waf install_release --tex-quality=pvrtcbest --PSVITAIP=<ipaddress>:<port> --destdir=./INSTALL
```

Output will be at `build/release/LugaruVita.vpk`. If you used the `install_release` command instead, then the VPK will also be uploaded to your Vita/VitaTV at `ux0:data/LugaruVita.vpk`, so that it can be installed with VitaShell.

Note: Transcoding textures can take a very long time (multiple hours), so for testing purposes it's best to use `--tex-quality=pvrtcnormal` or `--tex-quality=pvrtcfastest` (which is default if you omit the option)

## Debug/incremental builds (for development)

```
./waf configure --no-audio --profiling
./waf build --no-vpk
```

This will build the assets `build/Data.zip` and the application `build/eboot.bin`.

If using [VitaCompanion](https://github.com/devnoname120/vitacompanion), you can automatically **build + install + launch** the game on your Vita/VitaTV with the following command:

```
./waf install --PSVITAIP=<ipaddress>:<port> --destdir=./INSTALL --no-vpk --vclaunch=1
```

This gives you a fast edit-run loop with a single command (as long as your FTP connection is fast). 

That will ONLY work if you've installed the VPK manually through VitaShell at least once. This command also works for release builds by replacing `install` with `install_release`

(also note the --destdir flag is a lazy hack, don't think about it too much...)

### Debug/development tools

Note that LugaruOSS has its own runtime development tools, but I have not tested to see if they work.

* If you configured your debug build with the `--profiling` flag, you can press **Left Shoulder + Triangle** to dump the last 100 frames of profiling data to `ux0:data/microprofile_dump.html` as well as the last couple thousand lines of logs to `ux0:data/lugaru_runlog.txt`.

* `vita_debug.py` is a simple script that will automatically download the last core dump and lugaru_runlog.txt, and display them with vitaparsecore and a konsole window using lnav. This is highly specific to my workflow, but it can be trivially modified if you want. It requires you to manually change the ip address of your vita before running.