#ifndef REDMP2_APP_AUTOPLAY_H
#define REDMP2_APP_AUTOPLAY_H

#include <array>
#include <atomic>
#include <cstdint>

namespace repsp::hle::ctrl
{
struct SceCtrlData;
}

namespace repsp::runtime
{
class GuestMemory;
}

namespace redmp2::app
{

// Game-specific memory layout constants for DJ Max Portable 2 (ULKS-46116 v1.09).
// Basis: docs/analysis/autoplay.md (generated code + vblank 4200 guest memory dump).
namespace autoplay_layout
{
inline constexpr uint32_t kAddrCurrentScenePtr = 0x08972980u;       // 0x08831724: global current scene pointer
inline constexpr uint32_t kGameplaySceneVtable = 0x088D10C4u;       // 0x08852E48 at +32: PlayScene vtable at *(scene + 0)
inline constexpr uint32_t kAddrModeByte = 0x0897298Du;              // 0x08852574: mode (0=4B, 1=5B, 2=6B, 3=8B)
inline constexpr uint32_t kAddrKeyConfigTable = 0x089756D0u;        // 0x08864794: live key-config table (4 modes x 11B bit indices)
inline constexpr uint32_t kKeyConfigModeStride = 11u;               // 0x0886EE6C/0x0886F038/0x0886F1B0: 11 bytes per mode

inline constexpr uint32_t kSceneJudgeOffset = 0x44F20u;             // 0x08854B48: judgement object G = PlayScene + 0x44F20
inline constexpr uint32_t kJudgeScorePtrOffset = 108u;              // 0x0885829C: ScorePlayer S = *(G + 108) (== PlayScene + 0x413CC)
inline constexpr uint32_t kJudgeGearPtrOffset = 112u;               // 0x0885829C: Gear object = *(G + 112) (== PlayScene + 0x436A4)
inline constexpr uint32_t kJudgeStartLaneOffset = 356u;             // 0x088582CC: start lane index at *(G + 356)
inline constexpr uint32_t kJudgeEndLaneOffset = 360u;               // 0x088582D4: end lane index (incl. analog) at *(G + 360)

inline constexpr uint32_t kGearLaneChannelBaseOffset = 5668u;       // 0x0886BBFC: lane i chart channel at *(gear + 5668 + 4*i)
inline constexpr uint32_t kGearTotalLanesOffset = 5756u;            // 0x0886B2B4: total lanes (analog lane == *(gear + 5756) - 1)

inline constexpr uint32_t kScoreChartPtrOffset = 448u;              // 0x08858314: chart C = *(S + 448)
inline constexpr uint32_t kScoreNowTimeOffset = 8884u;              // 0x0885D418: current chart time at *(S + 8884)

inline constexpr uint32_t kChartChannelArrayBaseOffset = 8368u;     // 0x0885AFA0: note array at C + 8368 + channel*12
inline constexpr uint32_t kChartChannelEntryStride = 12u;           // 0x0885AFA0: {begin(+0), count(+4), capacity(+8)}
inline constexpr uint32_t kNoteArrayElemSize = 8u;                  // 0x0885AFA0: {note_addr(+0), note_index(+4)}

inline constexpr uint32_t kNoteTimeOffset = 0u;                     // 0x08858CFC: note time (u32, unaligned)
inline constexpr uint32_t kNoteTypeOffset = 4u;                     // 0x08858CFC: note type (u8)
inline constexpr uint32_t kNoteJudgedOffset = 6u;                   // 0x08858CFC: judged/active flag (u8; 0=unjudged, !=0=judged)
inline constexpr uint32_t kNoteDurationOffset = 14u;                // 0x08858D4C: duration (u16, unaligned); >= 7 is long note
inline constexpr uint16_t kLongNoteMinDuration = 7u;                // 0x08858D4C: slti $at, $v1, 7
inline constexpr uint8_t kNoteTypeNormal = 1u;                      // 0x08858CFC: tap and long note
inline constexpr uint8_t kNoteTypeHeader = 4u;                      // vblank 4200 dump: channel header (time 0)

inline constexpr uint32_t kMaxLanes = 8u;
inline constexpr uint8_t kMaxMode = 3u;
// Analog stick values for 0x088320A4 (docs/analysis/autoplay.md §6.1):
// 64..192 is neutral, Lx >= 193 is RIGHT, Lx < 64 is LEFT.
inline constexpr uint8_t kAnalogNeutral = 128u;
inline constexpr uint8_t kAnalogRight = 255u;
inline constexpr uint8_t kAnalogLeft = 0u;
// Filter calls with the chart time unchanged before autoplay treats the game
// as paused and hands the buttons over (about 3 frames).
inline constexpr uint32_t kStalledCallsForPause = 3u;
} // namespace autoplay_layout

// Input-stage autoplay controller for DJ Max Portable 2.
// Reads the live chart state and key-config table from guest memory inside
// sceCtrlReadBuffer* and synthesizes PSP lane button and analog stick inputs
// when notes reach the judgement line.
class Autoplay
{
public:
    void SetEnabled(bool enabled);
    bool IsEnabled() const;
    bool IsInGameplay() const;

    // Per-sample filter callback invoked on the guest thread from sceCtrlReadBuffer*.
    void FilterSample(repsp::hle::ctrl::SceCtrlData& sample, const repsp::runtime::GuestMemory& mem);

    // Resets per-song lane cursors and tracking state.
    void ResetState();

private:
    static uint32_t BitIndexToMask(uint8_t bit_idx);
    static uint32_t PrimaryLaneButton(uint8_t mode,
                                      const std::array<uint8_t, autoplay_layout::kKeyConfigModeStride>& key_row,
                                      uint32_t lane);
    static uint32_t AllLaneButtonsMask(uint8_t mode,
                                       const std::array<uint8_t, autoplay_layout::kKeyConfigModeStride>& key_row,
                                       uint32_t lane_count);

    std::atomic<bool> enabled_{false};
    std::atomic<bool> in_gameplay_{false};

    uint32_t last_score_addr_ = 0;
    uint32_t last_chart_addr_ = 0;
    uint32_t last_now_ = 0;
    bool has_last_now_ = false;
    // Consecutive filter calls that saw the chart time unchanged. The chart
    // clock stops while the game is paused (and before the song starts); the
    // pause menu is driven by the same lane buttons, so autoplay steps aside.
    uint32_t stalled_calls_ = 0;
    bool logged_unsupported_type_ = false;

    std::array<uint32_t, autoplay_layout::kMaxLanes> lane_cursor_{};
    std::array<bool, autoplay_layout::kMaxLanes> lane_pressed_prev_{};
    std::array<bool, autoplay_layout::kMaxLanes> lane_holding_{};
    std::array<uint64_t, autoplay_layout::kMaxLanes> lane_hold_until_{};

    uint32_t analog_cursor_ = 0;
    bool analog_pressed_prev_ = false;
    bool analog_holding_ = false;
    uint64_t analog_hold_until_ = 0;
    bool analog_hold_left_next_ = false;
};

} // namespace redmp2::app

#endif // REDMP2_APP_AUTOPLAY_H
