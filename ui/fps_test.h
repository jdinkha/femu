#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Frame-timing recorder behind `femu --fps-test [seconds] <rom>`. The game
// runs exactly as it normally would - same window, renderer, audio, frame
// pacing and keyboard input, so you can play while it measures - and
// main.cpp hands this one sample per emulated frame. Report() then compares
// the run against a real NTSC NES: average fps, 1% / 0.1% lows, slow frames,
// how much of each frame's time budget went to emulating and rendering, and
// the length of the emulated frames themselves.
class FpsTest {
public:
    explicit FpsTest(double duration_seconds) : duration(duration_seconds) {}

    // interval: wall time from the previous frame's start to this one's -
    //           what the player actually sees, pacing wait included
    // work:     the part of `interval` spent before the pacing wait
    //           (events, emulation, ImGui, rendering, present)
    // emu:      the part of `work` spent in Bus::clock()
    // dots:     master clock ticks (= PPU dots) the emulated frame took
    void AddFrame(double interval, double work, double emu, uint32_t dots);

    double Duration() const { return duration; }
    bool Done() const { return measured_seconds >= duration; }

    // Prints the report to stdout. Returns true if the run passed.
    bool Report(const std::string& rom_path) const;

private:
    struct Frame {
        double at; // seconds into the measured run when this frame started
        double interval, work, emu;
        uint32_t dots;
    };

    double duration;
    double measured_seconds = 0.0;
    size_t warmup_frames_seen = 0;
    std::vector<Frame> frames;
};
