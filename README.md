# Tg

Tg is a desktop timegrapher for mechanical watches. It listens to the ticking sound through the machine's audio input and estimates values such as rate, beat error, amplitude, and beat frequency.

## Download

- Windows, Linux MacOS installers available https://github.com/larrynz/tg/releases

## What You Need

- a working microphone or audio input device
- a quiet enough environment to capture the watch clearly
- the correct BPH and lift angle if you want the most reliable readings
- calibration, or at least a known-good setup. Use a loud quartz clock as a stable 1 Hz calibration signal.
   I also have had success using https://truemetronome.app/online/ and setting to 60 BPM.

If you want practical setup advice instead of just build instructions, start here:

- [docs/project-overview.md](docs/project-overview.md)
- [docs/microphone-and-calibration-guide.md](docs/microphone-and-calibration-guide.md)
- [docs/windows-vscode-development.md](docs/windows-vscode-development.md)

## Supported Platforms

- Windows
- macOS
- Linux

## Dependencies

Build and runtime depend on:

- GTK+ 3
- GLib 2.0
- PortAudio 2.0
- FFTW3 single-precision (`fftw3f`)
- pthread
- libm

Build tooling depends on:

- a C99-capable compiler
- `autoconf`
- `automake`
- `libtool`
- `make`
- `pkg-config`

## Build From Source

Generic release build:

```sh
git clone https://github.com/larrynz/tg.git
cd tg
./autogen.sh
./configure
make
```

Debug build:

```sh
make tg-timer-dbg
```

## Platform Notes

### Windows

The historical build path is MSYS2 with MinGW-w64.

For the intended developer workflow inside VS Code on Windows, see [docs/windows-vscode-development.md](docs/windows-vscode-development.md).

Typical package set:

```sh
pacman -S mingw-w64-x86_64-gcc make pkg-config mingw-w64-x86_64-gtk3 mingw-w64-x86_64-portaudio mingw-w64-x86_64-fftw git autoconf automake libtool
```

Then build with the generic steps above.

### Debian and Ubuntu

```sh
sudo apt-get install libgtk-3-dev libjack-jackd2-dev portaudio19-dev libfftw3-dev git autoconf automake libtool
git clone https://github.com/larrynz/tg.git
cd tg
./autogen.sh
./configure
make
```

`libjack-jackd2-dev` is included in the original instructions as a workaround for an old Debian bug.

### Fedora

```sh
sudo dnf install fftw-devel portaudio-devel gtk3-devel autoconf automake libtool
git clone https://github.com/larrynz/tg.git
cd tg
./autogen.sh
./configure
make
```

### macOS

macOS installer provided but untested (I don't have a mac). 
If you work on macOS, expect to verify your GTK, PortAudio, and microphone permission setup manually.

## Before You Trust The Numbers

Check these first:

1. The microphone is actually capturing the watch clearly.
2. The watch is positioned very close to the microphone.
3. The room is quiet enough for stable measurements.
4. The selected BPH is correct, or the guessed value is plausible.
5. The lift angle is correct if amplitude matters.
6. Calibration has been performed or at least checked for repeatability.

## Project Status

Known maintenance realities today (according to https://github.com/agrigera/tg):

- external discussions about Tg are generally positive about the tool itself
- users repeatedly ask for better microphone and calibration guidance
- packaging and maintenance visibility are spread across multiple forks and downstream packages

Repository-tracked review notes live under [docs/issues](docs/issues).

Recent implementation progress is tracked in [docs/issues/backport-progress-2026-03-22.md](docs/issues/backport-progress-2026-03-22.md).

## License

Tg is distributed under the GNU GPL version 2. See [LICENSE](LICENSE).
