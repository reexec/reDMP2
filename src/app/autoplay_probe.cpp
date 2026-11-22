#include "autoplay.h"

#include "repsp/platform/input.h"
#include "repsp/runtime/guest_memory.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define PROBE_ASSERT(cond)                                                              \
    do                                                                                  \
    {                                                                                   \
        if (!(cond))                                                                    \
        {                                                                               \
            std::fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__);      \
            std::fflush(nullptr);                                                       \
            std::_Exit(1);                                                              \
        }                                                                               \
    } while (false)

namespace
{

using namespace redmp2::app::autoplay_layout;
using namespace repsp::platform;

struct SynthNote
{
    uint32_t time = 0;
    uint8_t type = kNoteTypeNormal;
    uint8_t channel = 0;
    uint16_t duration = 6u;
    uint8_t judged = 0u;
};

struct SynthChartFixture
{
    static constexpr uint32_t kSceneAddr = 0x08E00000u;
    static constexpr uint32_t kScoreAddr = kSceneAddr + 0x413CCu;
    static constexpr uint32_t kGearAddr = kSceneAddr + 0x436A4u;
    static constexpr uint32_t kJudgeAddr = kSceneAddr + kSceneJudgeOffset;
    static constexpr uint32_t kChartAddr = 0x08E60000u;
    static constexpr uint32_t kArraysBase = 0x08EA0000u;
    static constexpr uint32_t kNotesBase = 0x08EB0000u;

    uint32_t next_array_addr = kArraysBase;
    uint32_t next_note_addr = kNotesBase;

    static void WriteDefaultKeyConfig(repsp::runtime::GuestMemory& mem)
    {
        // Default DMP2 key-config table measured at vblank 4200 (docs/analysis/autoplay.md §2).
        static constexpr uint8_t kTable[4][kKeyConfigModeStride] = {
            {7, 4, 0, 0, 12, 13, 0, 0, 9, 8, 14},   // 4B
            {7, 4, 5, 15, 12, 13, 0, 0, 9, 8, 14},  // 5B
            {7, 4, 5, 15, 12, 13, 0, 0, 9, 8, 14},  // 6B
            {7, 4, 5, 15, 12, 13, 8, 9, 3, 3, 14},  // 8B
        };
        for (uint32_t m = 0; m < 4u; ++m)
        {
            mem.WriteBlock(kAddrKeyConfigTable + m * kKeyConfigModeStride,
                           kTable[m],
                           kKeyConfigModeStride);
        }
    }

    void WriteScene(repsp::runtime::GuestMemory& mem, uint32_t vtable, uint32_t button_lanes, uint8_t mode)
    {
        WriteDefaultKeyConfig(mem);
        const uint32_t total_lanes = button_lanes + 1u; // +1 analog lane
        mem.WriteU32(kAddrCurrentScenePtr, kSceneAddr);
        mem.WriteU32(kSceneAddr, vtable);
        mem.WriteU32(kJudgeAddr + kJudgeScorePtrOffset, kScoreAddr);
        mem.WriteU32(kJudgeAddr + kJudgeGearPtrOffset, kGearAddr);
        mem.WriteU32(kJudgeAddr + kJudgeStartLaneOffset, 0u);
        mem.WriteU32(kJudgeAddr + kJudgeEndLaneOffset, total_lanes);
        mem.WriteU32(kGearAddr + kGearTotalLanesOffset, total_lanes);
        mem.WriteU32(kScoreAddr + kScoreChartPtrOffset, kChartAddr);
        mem.WriteU32(kScoreAddr + kScoreNowTimeOffset, 0u);
        mem.WriteU8(kAddrModeByte, mode);
        // Empty analog channel by default.
        SetLaneNotes(mem, button_lanes, 9u, {{0u, kNoteTypeHeader, 9u, 0u}});
    }

    void SetLaneNotes(repsp::runtime::GuestMemory& mem, uint32_t lane, uint32_t channel,
                      const std::vector<SynthNote>& notes)
    {
        mem.WriteU32(kGearAddr + kGearLaneChannelBaseOffset + lane * 4u, channel);
        const uint32_t arr_begin = next_array_addr;
        for (size_t i = 0; i < notes.size(); ++i)
        {
            const uint32_t note_addr = next_note_addr;
            next_note_addr += 17u; // intentionally odd stride (>=16) to test unaligned reads
            uint8_t raw[16] = {0};
            raw[0] = static_cast<uint8_t>(notes[i].time & 0xFFu);
            raw[1] = static_cast<uint8_t>((notes[i].time >> 8) & 0xFFu);
            raw[2] = static_cast<uint8_t>((notes[i].time >> 16) & 0xFFu);
            raw[3] = static_cast<uint8_t>((notes[i].time >> 24) & 0xFFu);
            raw[4] = notes[i].type;
            raw[5] = notes[i].channel;
            raw[6] = notes[i].judged;
            raw[14] = static_cast<uint8_t>(notes[i].duration & 0xFFu);
            raw[15] = static_cast<uint8_t>((notes[i].duration >> 8) & 0xFFu);
            mem.WriteBlock(note_addr, raw, sizeof(raw));

            mem.WriteU32(next_array_addr + 0u, note_addr);
            mem.WriteU32(next_array_addr + 4u, static_cast<uint32_t>(i));
            next_array_addr += kNoteArrayElemSize;
        }
        const uint32_t ch_entry = kChartAddr + kChartChannelArrayBaseOffset + channel * kChartChannelEntryStride;
        mem.WriteU32(ch_entry + 0u, arr_begin);
        mem.WriteU32(ch_entry + 4u, static_cast<uint32_t>(notes.size()));
        mem.WriteU32(ch_entry + 8u, static_cast<uint32_t>(notes.size()));
    }

    void SetNow(repsp::runtime::GuestMemory& mem, uint32_t now)
    {
        mem.WriteU32(kScoreAddr + kScoreNowTimeOffset, now);
    }
};

} // namespace

int main()
{
    auto& mem = repsp::runtime::GetGuestMemory();
    PROBE_ASSERT(mem.Initialize());

    redmp2::app::Autoplay ap;

    // 1. Disabled or non-gameplay scene leaves buttons untouched.
    {
        SynthChartFixture fx;
        fx.WriteScene(mem, kGameplaySceneVtable, 4u, 0u);
        ap.SetEnabled(false);
        PROBE_ASSERT(ap.FilterButtons(kPspCtrlLeft | kPspCtrlStart, mem) == (kPspCtrlLeft | kPspCtrlStart));
        PROBE_ASSERT(!ap.IsInGameplay());

        ap.SetEnabled(true);
        mem.WriteU32(SynthChartFixture::kSceneAddr, 0x08800000u); // non-gameplay vtable
        PROBE_ASSERT(ap.FilterButtons(kPspCtrlLeft | kPspCtrlStart, mem) == (kPspCtrlLeft | kPspCtrlStart));
        PROBE_ASSERT(!ap.IsInGameplay());
    }

    // 2. Mode 0 (4B): each note presses once, released on next call, back-to-back notes each pressed,
    //    human lane buttons cleared while START is preserved.
    {
        SynthChartFixture fx;
        fx.WriteScene(mem, kGameplaySceneVtable, 4u, 0u);
        // Lane 0 (LEFT): header + two back-to-back notes at t=100, t=116
        fx.SetLaneNotes(mem, 0u, 3u, {
            {0u, kNoteTypeHeader, 3u, 0u},
            {100u, kNoteTypeNormal, 3u, 6u},
            {116u, kNoteTypeNormal, 3u, 6u},
        });
        // Lane 1 (UP): unsupported type 2 at t=50 (skipped) + normal note at t=200
        fx.SetLaneNotes(mem, 1u, 4u, {
            {50u, 2u, 4u, 6u},
            {200u, kNoteTypeNormal, 4u, 6u},
        });
        // Lane 2 (TRIANGLE): note at t=300
        fx.SetLaneNotes(mem, 2u, 5u, {
            {300u, kNoteTypeNormal, 5u, 6u},
        });
        // Lane 3 (CIRCLE): note at t=300 (chord with lane 2)
        fx.SetLaneNotes(mem, 3u, 6u, {
            {300u, kNoteTypeNormal, 6u, 6u},
        });

        ap.ResetState();
        ap.SetEnabled(true);

        int lane0_presses = 0;
        int lane1_presses = 0;
        int lane2_presses = 0;
        int lane3_presses = 0;
        bool prev_lane0 = false;

        for (uint32_t t = 0; t <= 360u; t += 16u)
        {
            fx.SetNow(mem, t);
            const uint32_t in = kPspCtrlStart | kPspCtrlUp | kPspCtrlCircle;
            const uint32_t out = ap.FilterButtons(in, mem);
            PROBE_ASSERT(ap.IsInGameplay());
            PROBE_ASSERT((out & kPspCtrlStart) != 0u);

            const bool l0 = (out & kPspCtrlLeft) != 0u;
            const bool l1 = (out & kPspCtrlUp) != 0u;
            const bool l2 = (out & kPspCtrlTriangle) != 0u;
            const bool l3 = (out & kPspCtrlCircle) != 0u;

            if (prev_lane0)
            {
                PROBE_ASSERT(!l0);
            }
            prev_lane0 = l0;

            if (l0) ++lane0_presses;
            if (l1) ++lane1_presses;
            if (l2) ++lane2_presses;
            if (l3) ++lane3_presses;
        }

        PROBE_ASSERT(lane0_presses == 2);
        PROBE_ASSERT(lane1_presses == 1);
        PROBE_ASSERT(lane2_presses == 1);
        PROBE_ASSERT(lane3_presses == 1);
    }

    // 3. Mode 1 (5B), Mode 2 (6B), Mode 3 (8B) mapping verification, plus 5B clearing both [2] and [3]
    //    and custom key-config table following.
    {
        // Mode 1 (5B): LEFT, UP, RIGHT (center=[2]), TRIANGLE, CIRCLE; human SQUARE ([3]) also cleared.
        const uint32_t expected5[5] = {
            kPspCtrlLeft, kPspCtrlUp, kPspCtrlRight, kPspCtrlTriangle, kPspCtrlCircle
        };
        SynthChartFixture fx5;
        fx5.WriteScene(mem, kGameplaySceneVtable, 5u, 1u);
        for (uint32_t i = 0; i < 5u; ++i)
        {
            fx5.SetLaneNotes(mem, i, i + 1u, {{100u + i * 50u, kNoteTypeNormal, static_cast<uint8_t>(i + 1u)}});
        }
        ap.ResetState();
        for (uint32_t i = 0; i < 5u; ++i)
        {
            fx5.SetNow(mem, 100u + i * 50u);
            // Pass human SQUARE ([3]) + START: SQUARE must be cleared, START preserved.
            const uint32_t out = ap.FilterButtons(kPspCtrlSquare | kPspCtrlStart, mem);
            PROBE_ASSERT(out == (expected5[i] | kPspCtrlStart));
            fx5.SetNow(mem, 100u + i * 50u + 16u);
            PROBE_ASSERT(ap.FilterButtons(kPspCtrlSquare | kPspCtrlStart, mem) == kPspCtrlStart);
        }

        // Mode 2 (6B): LEFT, UP, RIGHT, SQUARE, TRIANGLE, CIRCLE
        const uint32_t expected6[6] = {
            kPspCtrlLeft, kPspCtrlUp, kPspCtrlRight, kPspCtrlSquare, kPspCtrlTriangle, kPspCtrlCircle
        };
        SynthChartFixture fx6;
        fx6.WriteScene(mem, kGameplaySceneVtable, 6u, 2u);
        for (uint32_t i = 0; i < 6u; ++i)
        {
            fx6.SetLaneNotes(mem, i, i + 1u, {{100u + i * 50u, kNoteTypeNormal, static_cast<uint8_t>(i + 1u)}});
        }
        ap.ResetState();
        for (uint32_t i = 0; i < 6u; ++i)
        {
            fx6.SetNow(mem, 100u + i * 50u);
            PROBE_ASSERT(ap.FilterButtons(0u, mem) == expected6[i]);
            fx6.SetNow(mem, 100u + i * 50u + 16u);
            PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        }

        // Mode 3 (8B): L, LEFT, UP, RIGHT, SQUARE, TRIANGLE, CIRCLE, R
        const uint32_t expected8[8] = {
            kPspCtrlLTrigger, kPspCtrlLeft, kPspCtrlUp, kPspCtrlRight,
            kPspCtrlSquare, kPspCtrlTriangle, kPspCtrlCircle, kPspCtrlRTrigger
        };
        SynthChartFixture fx8;
        fx8.WriteScene(mem, kGameplaySceneVtable, 8u, 3u);
        for (uint32_t i = 0; i < 8u; ++i)
        {
            fx8.SetLaneNotes(mem, i, i + 1u, {{100u + i * 50u, kNoteTypeNormal, static_cast<uint8_t>(i + 1u)}});
        }
        ap.ResetState();
        for (uint32_t i = 0; i < 8u; ++i)
        {
            fx8.SetNow(mem, 100u + i * 50u);
            PROBE_ASSERT(ap.FilterButtons(0u, mem) == expected8[i]);
            fx8.SetNow(mem, 100u + i * 50u + 16u);
            PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        }

        // Custom key-config test: remap 4B lane 0 (table[0]) from LEFT(7) to DOWN(6).
        SynthChartFixture fx_custom;
        fx_custom.WriteScene(mem, kGameplaySceneVtable, 4u, 0u);
        mem.WriteU8(kAddrKeyConfigTable + 0u, 6u); // DOWN
        fx_custom.SetLaneNotes(mem, 0u, 3u, {{100u, kNoteTypeNormal, 3u}});
        fx_custom.SetLaneNotes(mem, 1u, 4u, {});
        fx_custom.SetLaneNotes(mem, 2u, 5u, {});
        fx_custom.SetLaneNotes(mem, 3u, 6u, {});
        ap.ResetState();
        fx_custom.SetNow(mem, 100u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == kPspCtrlDown);
    }

    // 4. Long notes (duration >= 7 at +14): held from start until start + duration, then released.
    {
        SynthChartFixture fx;
        fx.WriteScene(mem, kGameplaySceneVtable, 4u, 0u);
        fx.SetLaneNotes(mem, 0u, 3u, {});
        fx.SetLaneNotes(mem, 1u, 4u, {
            {100u, kNoteTypeNormal, 4u, 64u},
            {200u, kNoteTypeNormal, 4u, 6u},
        });
        fx.SetLaneNotes(mem, 2u, 5u, {});
        fx.SetLaneNotes(mem, 3u, 6u, {});
        ap.ResetState();
        ap.SetEnabled(true);

        fx.SetNow(mem, 80u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        fx.SetNow(mem, 100u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == kPspCtrlUp);
        for (uint32_t t : {116u, 132u, 148u, 160u})
        {
            fx.SetNow(mem, t);
            PROBE_ASSERT(ap.FilterButtons(0u, mem) == kPspCtrlUp);
        }
        fx.SetNow(mem, 164u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        fx.SetNow(mem, 180u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        fx.SetNow(mem, 200u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == kPspCtrlUp);
        fx.SetNow(mem, 216u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
    }

    // 5. Pause: chart time stands still -> autoplay steps aside, then resumes.
    {
        SynthChartFixture fx;
        fx.WriteScene(mem, kGameplaySceneVtable, 4u, 0u);
        fx.SetLaneNotes(mem, 0u, 3u, {{200u, kNoteTypeNormal, 3u}});
        fx.SetLaneNotes(mem, 1u, 20u, {});
        fx.SetLaneNotes(mem, 2u, 21u, {});
        fx.SetLaneNotes(mem, 3u, 22u, {});
        ap.ResetState();
        ap.SetEnabled(true);

        const uint32_t human = kPspCtrlUp | kPspCtrlCircle | kPspCtrlStart;
        fx.SetNow(mem, 100u);
        PROBE_ASSERT((ap.FilterButtons(human, mem) & (kPspCtrlUp | kPspCtrlCircle)) == 0u);
        uint32_t out = 0;
        for (uint32_t i = 0; i < kStalledCallsForPause; ++i)
        {
            out = ap.FilterButtons(human, mem);
        }
        PROBE_ASSERT(out == human);
        PROBE_ASSERT(ap.FilterButtons(human, mem) == human);

        bool pressed = false;
        for (uint32_t t = 116u; t <= 260u; t += 16u)
        {
            fx.SetNow(mem, t);
            out = ap.FilterButtons(human, mem);
            PROBE_ASSERT((out & (kPspCtrlUp | kPspCtrlCircle)) == 0u);
            pressed = pressed || ((out & kPspCtrlLeft) != 0u);
        }
        PROBE_ASSERT(pressed);
    }

    // 6. Mid-song enable: skip already-judged tap notes and already-started long notes (+6 != 0)
    //    without pressing, and press from the first unjudged note onward.
    {
        SynthChartFixture fx;
        fx.WriteScene(mem, kGameplaySceneVtable, 4u, 0u);
        fx.SetLaneNotes(mem, 0u, 3u, {
            {0u, kNoteTypeHeader, 3u, 0u, 0u},
            {100u, kNoteTypeNormal, 3u, 6u, 1u},    // already missed/judged tap
            {200u, kNoteTypeNormal, 3u, 6u, 3u},    // already hit tap
            {300u, kNoteTypeNormal, 3u, 120u, 1u},  // already-started long note
            {500u, kNoteTypeNormal, 3u, 6u, 0u},    // first unjudged note
        });
        fx.SetLaneNotes(mem, 1u, 4u, {});
        fx.SetLaneNotes(mem, 2u, 5u, {});
        fx.SetLaneNotes(mem, 3u, 6u, {});

        ap.ResetState();
        ap.SetEnabled(false);
        fx.SetNow(mem, 440u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);

        // Enable autoplay mid-song at t=450: none of the judged notes (100, 200, 300) may be pressed.
        ap.SetEnabled(true);
        fx.SetNow(mem, 450u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        fx.SetNow(mem, 480u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
        // At t=500, the first unjudged note is pressed once and released on the next call.
        fx.SetNow(mem, 500u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == kPspCtrlLeft);
        fx.SetNow(mem, 516u);
        PROBE_ASSERT(ap.FilterButtons(0u, mem) == 0u);
    }

    mem.Shutdown();
    std::printf("[PASS] redmp2_autoplay_probe: all checks passed\n");
    return 0;
}
