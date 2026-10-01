#include "fps_test.h"

#include <cstdio>
#include <cmath>
#include <algorithm>
#include <functional>
#include <filesystem>

namespace {

// NTSC NES timing: the 21.477272 MHz master clock / 4 drives one PPU dot.
// A frame is 341 x 262 dots, except that odd frames are one dot shorter
// while rendering is enabled - so on average 89341.5 dots, or 60.0988 fps.
constexpr double kDotHz = 236250000.0 / 11.0 / 4.0;
constexpr double kRealDotsPerFrame = 89341.5;
constexpr double kRealFps = kDotHz / kRealDotsPerFrame;
constexpr double kRealFrameSeconds = 1.0 / kRealFps;

// The first second covers window mapping, the first texture upload and so
// on - startup cost, not gameplay - so it isn't measured.
constexpr size_t kWarmupFrames = 60;

// A frame that took 5%+ longer than a real NES frame counts as slow.
constexpr double kSlowFrameFactor = 1.05;

// Pass criteria: the average frame rate must track a real NES to within
// 0.1%, and the slowest 1% of frames must still average 95% of its speed.
constexpr double kAvgTolerance = 0.001;
constexpr double kOnePercentLowMin = 0.95 * kRealFps;

// Below this many frames a "1% low" is a handful of samples, not a statistic.
constexpr size_t kMinFrames = 100;

constexpr size_t kSlowestShown = 5;

// Average fps over the slowest `fraction` of frames - the usual "1% low".
// `intervals` must be sorted slowest first.
double LowFps(const std::vector<double>& intervals, double fraction) {
    size_t n = std::max<size_t>(1, (size_t)std::ceil(intervals.size() * fraction));
    double sum = 0.0;
    for (size_t i = 0; i < n; i++) sum += intervals[i];
    return n / sum;
}

// `sorted` must be ascending.
double Percentile(const std::vector<double>& sorted, double q) {
    size_t i = (size_t)std::ceil(sorted.size() * q);
    return sorted[std::min(sorted.size() - 1, i > 0 ? i - 1 : 0)];
}

struct Stats { double avg, p99, max; };

Stats Summarize(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    double sum = 0.0;
    for (double x : v) sum += x;
    return { sum / v.size(), Percentile(v, 0.99), v.back() };
}

double Ms(double seconds) { return seconds * 1000.0; }

std::string Fmt(const char* fmt, double a, double b = 0.0) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), fmt, a, b);
    return buf;
}

// One line of the report, with values and PASS/FAIL verdicts in fixed columns.
void Row(const char* label, const std::string& value, const std::string& note = "",
         const char* verdict = "") {
    char buf[192];
    std::snprintf(buf, sizeof(buf), "  %-15s %11s  %-30s %s", label, value.c_str(), note.c_str(), verdict);
    std::string line = buf;
    line.erase(line.find_last_not_of(' ') + 1);
    std::printf("%s\n", line.c_str());
}

} // namespace

void FpsTest::AddFrame(double interval, double work, double emu, uint32_t dots) {
    if (warmup_frames_seen < kWarmupFrames) {
        warmup_frames_seen++;
        return;
    }
    frames.push_back({ measured_seconds, interval, work, emu, dots });
    measured_seconds += interval;
}

bool FpsTest::Report(const std::string& rom_path) const {
    const std::string rom = std::filesystem::path(rom_path).filename().string();
    std::printf("\n==== femu FPS test: %s ====\n", rom.c_str());

    if (frames.size() < kMinFrames) {
        std::printf("Only %zu frames measured (need at least %zu, after %zu warm-up frames).\n",
                    frames.size(), kMinFrames, kWarmupFrames);
        std::printf("\nRESULT: FAIL (not enough frames)\n");
        return false;
    }

    const size_t n = frames.size();
    std::vector<double> intervals, work, emu;
    double dots_sum = 0.0;
    for (const Frame& f : frames) {
        intervals.push_back(f.interval);
        work.push_back(f.work);
        emu.push_back(f.emu);
        dots_sum += f.dots;
    }

    std::printf("Measured %.1f s, %zu frames (after %zu warm-up frames)", measured_seconds, n, kWarmupFrames);
    if (!Done()) std::printf(" - stopped early, %.0f s were requested", duration);
    std::printf("\n");

    // ---- Frame rate vs a real NES ----
    const double avg_fps = n / measured_seconds;
    const double avg_error = avg_fps / kRealFps - 1.0;
    std::vector<double> slowest_first = intervals;
    std::sort(slowest_first.begin(), slowest_first.end(), std::greater<double>());
    const double low1 = LowFps(slowest_first, 0.01);
    const double low01 = LowFps(slowest_first, 0.001);
    const double slow_threshold = kRealFrameSeconds * kSlowFrameFactor;
    const size_t slow = (size_t)std::count_if(intervals.begin(), intervals.end(),
                                              [&](double t) { return t > slow_threshold; });

    const bool avg_ok = std::fabs(avg_error) <= kAvgTolerance;
    const bool low_ok = low1 >= kOnePercentLowMin;

    std::printf("\nFrame rate vs a real NES (NTSC: %.4f fps, %.3f ms per frame)\n",
                kRealFps, Ms(kRealFrameSeconds));
    Row("average fps", Fmt("%.4f", avg_fps), Fmt("%+.3f%% vs real NES", avg_error * 100.0),
        Fmt(avg_ok ? "PASS (needs +/-%.1f%%)" : "FAIL (needs +/-%.1f%%)", kAvgTolerance * 100.0).c_str());
    Row("1% low fps", Fmt("%.3f", low1), "avg of slowest 1% of frames",
        Fmt(low_ok ? "PASS (needs >= %.2f)" : "FAIL (needs >= %.2f)", kOnePercentLowMin).c_str());
    Row("0.1% low fps", Fmt("%.3f", low01));
    Row("worst frame", Fmt("%.3f ms", Ms(slowest_first.front())), Fmt("%.2f fps", 1.0 / slowest_first.front()));
    Row("slow frames", Fmt("%.0f of %.0f", (double)slow, (double)n),
        Fmt("%.2f%% took over %.3f ms", 100.0 * slow / n, Ms(slow_threshold)));

    // ---- Where each frame's time went ----
    const Stats emu_stats = Summarize(emu);
    const Stats work_stats = Summarize(work);
    std::printf("\nFrame time budget (%.3f ms), before the pacing wait\n", Ms(kRealFrameSeconds));
    std::printf("                         avg        p99        max\n");
    std::printf("  emulation          %7.3f ms %7.3f ms %7.3f ms\n",
                Ms(emu_stats.avg), Ms(emu_stats.p99), Ms(emu_stats.max));
    std::printf("  total work         %7.3f ms %7.3f ms %7.3f ms   (max = %.0f%% of budget)\n",
                Ms(work_stats.avg), Ms(work_stats.p99), Ms(work_stats.max),
                100.0 * work_stats.max / kRealFrameSeconds);

    // ---- The emulated frames themselves ----
    const double avg_dots = dots_sum / n;
    std::printf("\nEmulated frame length vs a real NES (info)\n");
    Row("PPU dots/frame", Fmt("%.1f", avg_dots),
        Fmt("real NES: %.1f (odd frames are 1 dot short while rendering)", kRealDotsPerFrame));
    Row("NES-time fps", Fmt("%.4f", kDotHz / avg_dots), Fmt("real NES: %.4f", kRealFps));

    // ---- The worst offenders ----
    // A slow frame whose work fit in the budget wasn't the emulator's fault -
    // the OS woke the pacing wait up late.
    std::vector<const Frame*> worst;
    for (const Frame& f : frames) worst.push_back(&f);
    const size_t shown = std::min(kSlowestShown, worst.size());
    std::partial_sort(worst.begin(), worst.begin() + shown, worst.end(),
                      [](const Frame* a, const Frame* b) { return a->interval > b->interval; });
    std::printf("\nSlowest frames\n");
    std::printf("        at     interval        work   emulation   cause\n");
    for (size_t i = 0; i < shown; i++) {
        const Frame& f = *worst[i];
        const char* cause = f.interval <= slow_threshold ? "on time"
                          : f.work > kRealFrameSeconds   ? "work over budget"
                                                         : "late wake-up (OS)";
        std::printf("  %7.2f s  %8.3f ms  %7.3f ms  %7.3f ms   %s\n",
                    f.at, Ms(f.interval), Ms(f.work), Ms(f.emu), cause);
    }

    const bool pass = avg_ok && low_ok;
    std::printf("\nRESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass;
}
