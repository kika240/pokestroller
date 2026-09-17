# PokeStroller — a PokéWalker emulator for macOS and Windows

This fork adds a native **macOS application for Apple Silicon and Intel** to
[jpcerrone's experimental PokeStroller emulator](https://github.com/jpcerrone/pokestroller).
It loads your own ROM and EEPROM, displays your Pokémon, and runs the menus and
dowsing / Poké Radar minigames. The original Windows frontend is retained.

**[Guide macOS en français](docs/MACOS.md)** · [Validation notes](docs/VALIDATION.md)

## macOS quick start

Requires macOS 11+ and Apple's Command Line Tools 15+ (C++23). No Homebrew libraries required.

```sh
./build-macos.sh
open build/macos/PokeStroller.app
```

Click **Ouvrir…**, select your ROM, then your EEPROM **where they already live**.
No renaming or copying is required. The files are opened read-only and never
bundled or uploaded. Accepted sizes: 48 KiB ROM (or a 64 KiB full memory dump)
and 64 KiB EEPROM. Use only your own dumps.

- Left / right: **Z / X** or **arrow keys**. Center / wake: **Space / Return**.
- Clickable buttons, pause (**⌘P**), reload (**⌘R**), and native file dialogs.
- **⌘S** exports the emulated EEPROM to a separate file. Original input files
  are protected against overwrite. Reopen the exported EEPROM to resume later.
- **Marcher / ⌘M** simulates steps; **Son** controls native audio output.
- Automatic saves resume when you reopen the same original files. Sessions live
  in Application Support, with recovery copies and independent profiles.
- **Infrarouge** connects two instances over local TCP.
- Native Retina rendering, integer LCD scaling, and a resizable window.

`./build-macos.sh --package` also creates `dist/PokeStroller-macOS.zip` and
`dist/SHA256SUMS`. Local builds are ad-hoc signed, **not Apple-notarized**.
The ZIP contains only the application and documentation, no device dumps.

![home](https://github.com/jpcerrone/pokestroller/blob/master/img/home.gif)
![menu](https://github.com/jpcerrone/pokestroller/blob/master/img/menu.gif)
![dowsing](https://github.com/jpcerrone/pokestroller/blob/master/img/dowsing.gif)
![battle](https://github.com/jpcerrone/pokestroller/blob/master/img/battle.gif)

## Windows
To run the emulator you'll need a copy of your pokewalker's ROM and a copy of your EEPROM binary. You can dump both of these from your pokewalker using [PoroCYon's dumper for DSi/3ds](https://gitlab.ulyssis.org/pcy/pokewalker-rom-dumper) or [DmitryGR's PalmOS app](https://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker#_TOC_377b8050cfd1e60865685a4ca39bc4c0).

Download the latest release from the "Releases" section. 

Place both the eeprom and rom files in the same folder as the emulator binary and rename them to `eeprom.bin` and `rom.bin` accordingly.

Run the emulator, the buttons are controlled with `Z`, `X` and the `spacebar`.

## TODO list — implemented in the macOS port

- [x] Audio: native 32 kHz output, mute and volume controls.
- [x] Virtual IR: TCP connection between two emulator instances, including a
  complete in-game encounter and gift. DS-emulator interoperability is unverified.
- [x] RTC: local system clock, BCD registers and periodic/calendar interrupts.
- [x] Accelerometer simulation: firmware counts synthetic steps and awards watts.
- [x] Automatic EEPROM saving: independent profiles, atomic writes, recovery copy
  and preservation of cached health data on the supported firmware.
- [x] Poké Radar wrong-bush path: tested through the escape message and return.

The native frontend now uses the GPL-3.0 PocketWalker core with targeted fixes;
see [attribution](third_party/pocketwalker/UPSTREAM.md),
[usage](docs/MACOS.md), and [validation](docs/VALIDATION.md).
The historical Windows C backend retains its original limitations.

## Compiling
### Windows
Install the MSVC build tools for windows and run `build.bat` from the command line
https://learn.microsoft.com/en-us/cpp/build/building-on-the-command-line?view=msvc-170
### macOS
See the quick start above or [the detailed guide](docs/MACOS.md).

### Tests
`./tests/run-tests.sh` runs synthetic core regression tests with AddressSanitizer
and UndefinedBehaviorSanitizer. No Nintendo ROM or EEPROM is required.

An optional local smoke test can use dumps directly at their existing paths:

```sh
./tests/run-tests.sh "/path/to/rom.bin" "/path/to/eeprom.bin" 120
```

This executes 30 seconds of emulated time, including a wake-button press, without
writing either input. A successful smoke test is not proof of complete hardware
emulation. This command checks the legacy C backend; use
`./tests/run-native-tests.sh` to check the macOS backend and peripherals.
CPU register aliases in the
upstream core require `-fno-strict-aliasing` with Clang/GCC.

### Linux and other systems
No frontend is included for these systems on this branch.

## Contributing
Feel free to contribute by opening up a PR!

## Shameless plug
Check out my ear training software for guitar! [http://gapsguitar.com/](http://gapsguitar.com/)
