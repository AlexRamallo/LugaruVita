# Lugaru Vita

![Box art](/screenshot/boxart.png "Lugaru Vita")

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

See [COMPILING.md](COMPILING.md)

# Screenshots

![scr1](/screenshot/scr1.jpg "Dead wolf")
![scr2](/screenshot/scr2.jpg "Weapons")
![scr3](/screenshot/scr3.jpg "Ragdolls")
![scr4](/screenshot/scr4.jpg "Sneak attacks")