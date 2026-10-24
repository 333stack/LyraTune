#pragma once

// What the editor's visualisers show this frame, derived from DSP telemetry on the message thread.
struct DisplayState
{
    bool voiced = false;
    float inputMidi = 0, outputMidi = 0; // fractional MIDI
    int target = -1;                      // target note, -1 = none
    int pending = -1;                     // Note Stabilizer candidate, -1 = none
    float level = 0;                      // 0..1 smoothed input level for animation
    float presence = 0;                   // 0..1 fades in/out with voicing (avoids flicker)
};
