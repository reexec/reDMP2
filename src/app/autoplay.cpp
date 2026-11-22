#include "autoplay.h"

#include "repsp/hle/ctrl.h"
#include "repsp/runtime/guest_memory.h"

#include <cstdio>

namespace redmp2::app
{

namespace
{

constexpr uint32_t kMaxChannelNotes = 65536u;

bool ReadU8Checked(const repsp::runtime::GuestMemory& mem, uint32_t addr, uint8_t& out)
{
    if (!mem.IsValidAddress(addr, 1u))
    {
        return false;
    }
    mem.ReadBlock(addr, &out, 1u);
    return true;
}

bool ReadU16Checked(const repsp::runtime::GuestMemory& mem, uint32_t addr, uint16_t& out)
{
    if (!mem.IsValidAddress(addr, 2u))
    {
        return false;
    }
    mem.ReadBlock(addr, &out, 2u);
    return true;
}

bool ReadU32Checked(const repsp::runtime::GuestMemory& mem, uint32_t addr, uint32_t& out)
{
    if (!mem.IsValidAddress(addr, 4u))
    {
        return false;
    }
    mem.ReadBlock(addr, &out, 4u);
    return true;
}

} // namespace

void Autoplay::SetEnabled(bool enabled)
{
    enabled_.store(enabled, std::memory_order_release);
}

bool Autoplay::IsEnabled() const
{
    return enabled_.load(std::memory_order_acquire);
}

bool Autoplay::IsInGameplay() const
{
    return in_gameplay_.load(std::memory_order_acquire);
}

void Autoplay::ResetState()
{
    last_score_addr_ = 0;
    last_chart_addr_ = 0;
    last_now_ = 0;
    has_last_now_ = false;
    stalled_calls_ = 0;
    logged_unsupported_type_ = false;
    lane_cursor_.fill(0u);
    lane_pressed_prev_.fill(false);
    lane_holding_.fill(false);
    lane_hold_until_.fill(0u);
    analog_cursor_ = 0;
    analog_pressed_prev_ = false;
    analog_holding_ = false;
    analog_hold_until_ = 0;
    analog_hold_left_next_ = false;
}

uint32_t Autoplay::BitIndexToMask(uint8_t bit_idx)
{
    return (bit_idx < 16u) ? (1u << bit_idx) : 0u;
}

uint32_t Autoplay::PrimaryLaneButton(
    uint8_t mode,
    const std::array<uint8_t, autoplay_layout::kKeyConfigModeStride>& key_row,
    uint32_t lane)
{
    // Key-config table offsets from docs/analysis/autoplay.md section 2.
    static constexpr uint8_t kOffsets4B[4] = {0u, 1u, 4u, 5u};
    static constexpr uint8_t kOffsets5B[5] = {0u, 1u, 2u, 4u, 5u};
    static constexpr uint8_t kOffsets6B[6] = {0u, 1u, 2u, 3u, 4u, 5u};
    static constexpr uint8_t kOffsets8B[8] = {6u, 0u, 1u, 2u, 3u, 4u, 5u, 7u};

    switch (mode)
    {
    case 0:
        return (lane < 4u) ? BitIndexToMask(key_row[kOffsets4B[lane]]) : 0u;
    case 1:
        return (lane < 5u) ? BitIndexToMask(key_row[kOffsets5B[lane]]) : 0u;
    case 2:
        return (lane < 6u) ? BitIndexToMask(key_row[kOffsets6B[lane]]) : 0u;
    case 3:
        return (lane < 8u) ? BitIndexToMask(key_row[kOffsets8B[lane]]) : 0u;
    default:
        return 0u;
    }
}

uint32_t Autoplay::AllLaneButtonsMask(
    uint8_t mode,
    const std::array<uint8_t, autoplay_layout::kKeyConfigModeStride>& key_row,
    uint32_t lane_count)
{
    uint32_t mask = 0u;
    for (uint32_t i = 0; i < lane_count; ++i)
    {
        mask |= PrimaryLaneButton(mode, key_row, i);
    }
    // 5B center lane (0x0886EE50) accepts both key_row[2] and key_row[3].
    if (mode == 1u)
    {
        mask |= BitIndexToMask(key_row[3u]);
    }
    return mask;
}

void Autoplay::FilterSample(repsp::hle::ctrl::SceCtrlData& sample, const repsp::runtime::GuestMemory& mem)
{
    using namespace autoplay_layout;

    if (!IsEnabled())
    {
        in_gameplay_.store(false, std::memory_order_release);
        ResetState();
        return;
    }

    uint32_t scene_addr = 0;
    uint32_t scene_vtable = 0;
    if (!ReadU32Checked(mem, kAddrCurrentScenePtr, scene_addr) || scene_addr == 0u ||
        !ReadU32Checked(mem, scene_addr, scene_vtable) || scene_vtable != kGameplaySceneVtable)
    {
        in_gameplay_.store(false, std::memory_order_release);
        ResetState();
        return;
    }

    in_gameplay_.store(true, std::memory_order_release);

    const uint32_t g_addr = scene_addr + kSceneJudgeOffset;
    uint32_t score_addr = 0;
    uint32_t gear_addr = 0;
    uint32_t start_lane = 0;
    uint32_t end_lane = 0;
    uint32_t total_lanes = 0;
    uint32_t chart_addr = 0;
    uint32_t now = 0;
    uint8_t mode = 0;

    if (!ReadU32Checked(mem, g_addr + kJudgeScorePtrOffset, score_addr) || score_addr == 0u ||
        !ReadU32Checked(mem, g_addr + kJudgeGearPtrOffset, gear_addr) || gear_addr == 0u ||
        !ReadU32Checked(mem, g_addr + kJudgeStartLaneOffset, start_lane) || start_lane != 0u ||
        !ReadU32Checked(mem, g_addr + kJudgeEndLaneOffset, end_lane) ||
        !ReadU32Checked(mem, gear_addr + kGearTotalLanesOffset, total_lanes) ||
        total_lanes < 2u || end_lane < 2u ||
        !ReadU32Checked(mem, score_addr + kScoreChartPtrOffset, chart_addr) || chart_addr == 0u ||
        !ReadU32Checked(mem, score_addr + kScoreNowTimeOffset, now) ||
        !ReadU8Checked(mem, kAddrModeByte, mode) || mode > kMaxMode)
    {
        return;
    }

    const uint32_t analog_lane = total_lanes - 1u;
    const uint32_t button_lane_count = (end_lane > analog_lane) ? analog_lane : end_lane;
    if (button_lane_count == 0u || button_lane_count > kMaxLanes)
    {
        return;
    }

    const uint32_t key_row_addr = kAddrKeyConfigTable + static_cast<uint32_t>(mode) * kKeyConfigModeStride;
    if (!mem.IsValidAddress(key_row_addr, kKeyConfigModeStride))
    {
        return;
    }
    std::array<uint8_t, kKeyConfigModeStride> key_row{};
    mem.ReadBlock(key_row_addr, key_row.data(), key_row.size());

    if (score_addr != last_score_addr_ || chart_addr != last_chart_addr_ ||
        (has_last_now_ && now < last_now_))
    {
        ResetState();
    }

    // Paused (or not started): the chart time stands still. Hand the buttons
    // and stick over unchanged so the pause menu works, and drop any hold in progress.
    if (has_last_now_ && now == last_now_)
    {
        if (stalled_calls_ < kStalledCallsForPause)
        {
            ++stalled_calls_;
        }
    }
    else
    {
        stalled_calls_ = 0;
    }
    if (stalled_calls_ >= kStalledCallsForPause)
    {
        lane_pressed_prev_.fill(false);
        lane_holding_.fill(false);
        analog_pressed_prev_ = false;
        analog_holding_ = false;
        last_score_addr_ = score_addr;
        last_chart_addr_ = chart_addr;
        return;
    }

    uint32_t delta = has_last_now_ ? (now - last_now_) : 0u;
    if (delta > 200u)
    {
        delta = 0u;
    }
    const uint64_t threshold = static_cast<uint64_t>(now) + static_cast<uint64_t>(delta / 2u);

    last_score_addr_ = score_addr;
    last_chart_addr_ = chart_addr;
    last_now_ = now;
    has_last_now_ = true;

    // Clear human lane buttons and stick while preserving non-lane buttons (START, SELECT, etc.).
    uint32_t out = sample.buttons & ~AllLaneButtonsMask(mode, key_row, button_lane_count);
    sample.lx = kAnalogNeutral;
    sample.ly = kAnalogNeutral;

    for (uint32_t lane = 0; lane < button_lane_count; ++lane)
    {
        // Hold active long notes until their end time, then release for at least one frame.
        if (lane_holding_[lane])
        {
            if (static_cast<uint64_t>(now) < lane_hold_until_[lane])
            {
                out |= PrimaryLaneButton(mode, key_row, lane);
            }
            else
            {
                lane_holding_[lane] = false;
            }
            continue;
        }

        // Always release on the frame following a tap press so back-to-back notes trigger a fresh edge.
        if (lane_pressed_prev_[lane])
        {
            lane_pressed_prev_[lane] = false;
            continue;
        }

        uint32_t channel = 0;
        if (!ReadU32Checked(mem, gear_addr + kGearLaneChannelBaseOffset + lane * 4u, channel))
        {
            continue;
        }

        const uint32_t arr_addr = chart_addr + kChartChannelArrayBaseOffset + channel * kChartChannelEntryStride;
        uint32_t arr_begin = 0;
        uint32_t note_count = 0;
        if (!ReadU32Checked(mem, arr_addr + 0u, arr_begin) ||
            !ReadU32Checked(mem, arr_addr + 4u, note_count) ||
            arr_begin == 0u || note_count == 0u || note_count > kMaxChannelNotes)
        {
            continue;
        }

        const uint32_t byte_len = note_count * kNoteArrayElemSize;
        if (!mem.IsValidAddress(arr_begin, byte_len))
        {
            continue;
        }

        while (lane_cursor_[lane] < note_count)
        {
            const uint32_t elem_addr = arr_begin + lane_cursor_[lane] * kNoteArrayElemSize;
            uint32_t note_addr = 0;
            if (!ReadU32Checked(mem, elem_addr, note_addr) || note_addr == 0u ||
                !mem.IsValidAddress(note_addr, 16u))
            {
                break;
            }

            uint8_t note_type = 0;
            if (!ReadU8Checked(mem, note_addr + kNoteTypeOffset, note_type))
            {
                break;
            }

            if (note_type == kNoteTypeHeader)
            {
                ++lane_cursor_[lane];
                continue;
            }

            if (note_type != kNoteTypeNormal)
            {
                if (!logged_unsupported_type_)
                {
                    uint32_t note_time = 0;
                    (void)ReadU32Checked(mem, note_addr + kNoteTimeOffset, note_time);
                    std::fprintf(stderr,
                                 "[autoplay] skipping unsupported note type %u (time=%u, lane=%u, ch=%u)\n",
                                 static_cast<unsigned>(note_type),
                                 note_time,
                                 lane,
                                 channel);
                    logged_unsupported_type_ = true;
                }
                ++lane_cursor_[lane];
                continue;
            }

            uint8_t note_judged = 0;
            if (!ReadU8Checked(mem, note_addr + kNoteJudgedOffset, note_judged))
            {
                break;
            }
            if (note_judged != 0u)
            {
                // Already judged (missed/hit) or an already-active long note: skip without pressing
                // so enabling autoplay mid-song does not replay past notes.
                ++lane_cursor_[lane];
                continue;
            }

            uint32_t note_time = 0;
            uint16_t duration = 0;
            if (!ReadU32Checked(mem, note_addr + kNoteTimeOffset, note_time) ||
                !ReadU16Checked(mem, note_addr + kNoteDurationOffset, duration))
            {
                break;
            }

            if (static_cast<uint64_t>(note_time) <= threshold)
            {
                const uint32_t btn = PrimaryLaneButton(mode, key_row, lane);
                if (btn != 0u)
                {
                    out |= btn;
                    if (duration >= kLongNoteMinDuration)
                    {
                        lane_holding_[lane] = true;
                        lane_hold_until_[lane] = static_cast<uint64_t>(note_time) + static_cast<uint64_t>(duration);
                    }
                    else
                    {
                        lane_pressed_prev_[lane] = true;
                    }
                }
                ++lane_cursor_[lane];
            }
            break;
        }
    }

    sample.buttons = out;

    // Analog lane (gear+5756 - 1): wiggle-stick input per docs/analysis/autoplay.md §6.
    // Press sets RIGHT (255, 128); long notes alternate LEFT (0, 128) and RIGHT (255, 128)
    // on each call so 0x088320A4's 30-frame decay timer keeps refreshing; taps return to neutral.
    if (analog_holding_)
    {
        if (static_cast<uint64_t>(now) < analog_hold_until_)
        {
            sample.lx = analog_hold_left_next_ ? kAnalogLeft : kAnalogRight;
            sample.ly = kAnalogNeutral;
            analog_hold_left_next_ = !analog_hold_left_next_;
        }
        else
        {
            analog_holding_ = false;
        }
        return;
    }

    if (analog_pressed_prev_)
    {
        analog_pressed_prev_ = false;
        return;
    }

    if (analog_lane < end_lane)
    {
        uint32_t channel = 0;
        if (!ReadU32Checked(mem, gear_addr + kGearLaneChannelBaseOffset + analog_lane * 4u, channel))
        {
            return;
        }
        const uint32_t arr_addr = chart_addr + kChartChannelArrayBaseOffset + channel * kChartChannelEntryStride;
        uint32_t arr_begin = 0;
        uint32_t note_count = 0;
        if (!ReadU32Checked(mem, arr_addr + 0u, arr_begin) ||
            !ReadU32Checked(mem, arr_addr + 4u, note_count) ||
            arr_begin == 0u || note_count == 0u || note_count > kMaxChannelNotes)
        {
            return;
        }
        const uint32_t byte_len = note_count * kNoteArrayElemSize;
        if (!mem.IsValidAddress(arr_begin, byte_len))
        {
            return;
        }

        while (analog_cursor_ < note_count)
        {
            const uint32_t elem_addr = arr_begin + analog_cursor_ * kNoteArrayElemSize;
            uint32_t note_addr = 0;
            if (!ReadU32Checked(mem, elem_addr, note_addr) || note_addr == 0u ||
                !mem.IsValidAddress(note_addr, 16u))
            {
                break;
            }
            uint8_t note_type = 0;
            if (!ReadU8Checked(mem, note_addr + kNoteTypeOffset, note_type))
            {
                break;
            }
            if (note_type == kNoteTypeHeader)
            {
                ++analog_cursor_;
                continue;
            }
            if (note_type != kNoteTypeNormal)
            {
                if (!logged_unsupported_type_)
                {
                    uint32_t note_time = 0;
                    (void)ReadU32Checked(mem, note_addr + kNoteTimeOffset, note_time);
                    std::fprintf(stderr,
                                 "[autoplay] skipping unsupported note type %u (time=%u, lane=%u, ch=%u)\n",
                                 static_cast<unsigned>(note_type),
                                 note_time,
                                 analog_lane,
                                 channel);
                    logged_unsupported_type_ = true;
                }
                ++analog_cursor_;
                continue;
            }

            uint8_t note_judged = 0;
            if (!ReadU8Checked(mem, note_addr + kNoteJudgedOffset, note_judged))
            {
                break;
            }
            if (note_judged != 0u)
            {
                ++analog_cursor_;
                continue;
            }

            uint32_t note_time = 0;
            uint16_t duration = 0;
            if (!ReadU32Checked(mem, note_addr + kNoteTimeOffset, note_time) ||
                !ReadU16Checked(mem, note_addr + kNoteDurationOffset, duration))
            {
                break;
            }

            if (static_cast<uint64_t>(note_time) <= threshold)
            {
                sample.lx = kAnalogRight;
                sample.ly = kAnalogNeutral;
                if (duration >= kLongNoteMinDuration)
                {
                    analog_holding_ = true;
                    analog_hold_until_ = static_cast<uint64_t>(note_time) + static_cast<uint64_t>(duration);
                    analog_hold_left_next_ = true;
                }
                else
                {
                    analog_pressed_prev_ = true;
                }
                ++analog_cursor_;
            }
            break;
        }
    }
}

} // namespace redmp2::app
