# Epatoles

Songcraft Audio. A tape machine that runs both ways (AU / VST3 / Standalone), built with JUCE.

Epatoles is Selotape spelled backwards. It has everything Selotape has (tape saturation,
varispeed, tape stop, wow, flutter, hiss, oversampling, auto gain) and adds a transport that
can run in reverse.

## Reverse

- **Reverse** switch (next to Tape Stop): flips the transport direction. The motor goes
  through Inertia, so the tape slows to a stop and winds up backwards, the way a real
  deck does. Short Inertia flips almost instantly, long Inertia is a slow turnaround.
  The Speed dial still sets how fast the tape runs, in either direction.
- While the tape runs backwards, the playback head walks back through the last few seconds
  you played. When a chunk runs out it jumps to the newest material and starts again.
  At -1x (Speed 0 st) the chunks line up end to end with no gaps or overlaps.
- **Window** sets how long each reversed chunk is (0.1 to 10 s). In forward mode it keeps
  its Selotape job (how far the head can fall behind).
- **Smooth** is the crossfade between chunks (5 to 250 ms). Short is more tape-splice,
  long is softer.
- **Chunk Sync** locks the chunks to the host grid (1/16 to 2 bars) so every chunk is
  exactly one beat or bar of what just played, reversed. It needs the host to be playing
  and to report tempo. Without that it falls back to Window. "1 bar" and "2 bars"
  follow the host time signature.
- The arrows above the pills show the live direction of the tape.

Speed and Reverse can both be automated. Reverse + Tape Stop works as you'd expect.

## Build (macOS)

Requires Xcode command line tools and CMake (`brew install cmake`).

    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j 8

The AU is copied to `~/Library/Audio/Plug-Ins/Components/` automatically.
Validate it with:

    auval -v aufx Epat Sngc

If your DAW doesn't see it, run `killall -9 AudioComponentRegistrar` and rescan.

## Interface

- Large dial: Speed (varispeed, semitones). The small white dot on its ring shows the
  live transport speed.
- Tape Stop and Reverse under the dial.
- Knobs: Drive, Warmth, Tone, Mix, Output / Wow, Flutter, Hiss, Inertia, Window, Smooth.
- Chunk Sync: bottom right of the knob grid.
- Top right: Oversampling (Off / 2x / 4x / 8x) and Auto Gain.
- Double-click any knob to reset it to its default.

## Notes

- Reports one fixed latency to the host (60 ms head delay plus the 8x oversampler's
  filter latency), same as Selotape. The dry path is delayed by the same amount.
  In reverse the wet signal is whatever the tape plays back, so it is not time-aligned
  with the dry signal.
- Reversed chunks can reach back about 22 s. At slow tape speeds a very long Window is
  cut short to stay inside that.
- For varispeed and reverse work, use Mix at 100%.
- `TapeHeads.h` holds the read-head logic and has no JUCE dependency.
