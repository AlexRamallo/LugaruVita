# Building LugaruVita

The original build system was replaced with waf in order to more easily support inclusion of an asset processor (for transcoding textures). Waf is included in the repo (it's a Python script), so there's no need to download it.

Builds tested on Ubuntu only, but it might still work on Windows/Mac with minor modifications to the instructions below.

**NOTE:** There's a Dockerfile that you can use to build, or as a reference for setting up your environment.

## Dependencies

You need to obtain these dependencies yourself

* Python 3.7+ and Pip
* "Pillow" library from Pip
* [PVRTexTool from Imagination Technologies](https://developer.imaginationtech.com/pvrtextool/). You can create a free account to download it. (make sure `PVRTexToolCLI` is in your PATH)
* [VitaSDK](https://vitasdk.org) (make sure the `VITASDK` environment variable is present)

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

TODO: improve this

```
git clone https://gitlab.com/alexramallo/lugaruvita && cd lugaruvita
git submodule --init --recursive

cd sdl2-vitagl-lugaru
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$VITASDK/share/vita.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DVIDEO_VITA_VGL=TRUE -DVIDEO_VITA_PVR=FALSE
make install -j8
```

## Building a release VPK

NOTE: `waf` is a Python script and this project requires Python 3.7 or later. You might have to use `python3 ./waf` instead, depending on how your system is configured.

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