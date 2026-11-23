#pragma once

// Epatoles (Songcraft Audio): read-head control for a tape that runs both ways.
//
// Standard C++ only (no JUCE), so the logic can be tested on its own.
//
// Two read heads sit on a ring buffer of recorded tape. `pos` is the head
// position in absolute sample indices; the write head is at `writeIndex`, so
// delay = writeIndex - pos. Each sample the heads move by `speed` (negative =
// the tape runs backwards, so the head walks back through what was recorded).
//
// Forward (speed >= 0)
//   Same behaviour as Selotape: below 1x the head falls behind and jumps to the
//   newest material at the edge of the Window, above 1x it catches up and jumps
//   back out to the Window, and at 1x it locks back onto the latency-matched
//   position.
//
// Reverse (speed < 0)
//   The head starts at the newest material and plays it backwards. Delay grows
//   at (1 + |speed|) samples per sample, because the write head keeps moving
//   while the read head runs away from it. A chunk ends in one of two ways:
//     - free:  after `chunkSamples` of source tape has been played, the head
//              jumps back to the newest material (delay limit below);
//     - synced: when the host grid says so (requestGridJump), so every chunk is
//              exactly one beat/bar of the last audio, reversed.
//   At exactly -1x the free chunks tile the input with no gaps or overlap.
//
// A jump starts the second head at the new position and crossfades to it
// (equal power). Jumps are never started while a fade is running.

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace epat
{
struct HeadParams
{
    double windowSamples = 0.0;  // forward: maximum delay before wrapping
    double chunkSamples  = 0.0;  // reverse: source samples per chunk (free mode)
    double lockTarget    = 0.0;  // forward: delay that lines up with the dry path
    double minDelay      = 0.0;  // delay a freshly jumped head starts at
    double maxDelay      = 0.0;  // hard ceiling, keeps heads inside the ring
    double sampleRate    = 44100.0;
    int    xfade         = 0;    // forward jump crossfade (samples)
    int    rfade         = 0;    // reverse jump crossfade (samples)
    bool   gridSync      = false;
};

class TapeHeads
{
public:
    void reset (double lockTarget)
    {
        pos[0] = pos[1] = -lockTarget;
        active = 0;
        fading = false;
        fadeCount = 0;
        fadeLen = 1;
        pending = false;
    }

    // Reverse + synced: ask for a jump to the newest material at the next
    // opportunity (kept until the current fade has finished).
    void requestGridJump() { pending = true; }

    // Decide whether to jump. `speed` is the per-sample head speed after wow and
    // flutter; the return value is the same speed plus any lock-in slew.
    double manage (int64_t writeIndex, double curSpeed, double speed, const HeadParams& hp)
    {
        if (fading)
            return speed;

        const double delay = (double) writeIndex - pos[active];

        if (curSpeed < 0.0)
        {
            bool jump = delay > hp.maxDelay;

            if (hp.gridSync)
            {
                jump = jump || pending;
            }
            else
            {
                const double spd = std::max (std::abs (curSpeed), 0.05);
                const double limit = hp.minDelay + hp.chunkSamples * (1.0 + 1.0 / spd);
                jump = jump || delay > limit;
            }

            if (jump)
                jumpTo (writeIndex, hp.minDelay, hp.rfade);

            pending = false;
            return speed;
        }

        pending = false;

        if (curSpeed < 1.0 && delay > hp.windowSamples)
        {
            jumpTo (writeIndex, hp.minDelay, hp.xfade);
        }
        else if (curSpeed > 1.0 && delay < hp.minDelay)
        {
            jumpTo (writeIndex, hp.windowSamples, hp.xfade);
        }
        else if (std::abs (curSpeed - 1.0) < 0.002)
        {
            // Back at 1x: bring the wet path in time with the dry path.
            const double err = delay - hp.lockTarget;

            if (std::abs (err) > 0.05 * hp.sampleRate)
                jumpTo (writeIndex, hp.lockTarget, hp.xfade);
            else
                speed += std::clamp (err / hp.sampleRate, -0.002, 0.002);
        }

        return speed;
    }

    // Crossfade weights for the active head (w0) and the other head (w1).
    void weights (float& w0, float& w1) const
    {
        if (! fading)
        {
            w0 = 1.0f;
            w1 = 0.0f;
            return;
        }

        const float t = (float) fadeCount / (float) fadeLen;
        w0 = std::cos (t * 1.5707963f);
        w1 = std::sin (t * 1.5707963f);
    }

    void advance (double speed)
    {
        pos[0] += speed;
        pos[1] += speed;

        if (fading && ++fadeCount >= fadeLen)
        {
            active = 1 - active;
            fading = false;
        }
    }

    double activePos() const { return pos[active]; }
    double otherPos()  const { return pos[1 - active]; }
    bool   isFading()  const { return fading; }

private:
    void jumpTo (int64_t writeIndex, double newDelay, int fadeSamples)
    {
        pos[1 - active] = (double) writeIndex - newDelay;
        fading = true;
        fadeCount = 0;
        fadeLen = std::max (1, fadeSamples);
    }

    double pos[2] { 0.0, 0.0 };
    int    active = 0;
    bool   fading = false;
    int    fadeCount = 0;
    int    fadeLen = 1;
    bool   pending = false;
};
} // namespace epat
