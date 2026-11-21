#ifndef REDMP2_DMP2_PROFILE_H
#define REDMP2_DMP2_PROFILE_H

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace redmp2
{

constexpr const char* kTitleName = "DJ Max Portable 2";
constexpr const char* kExpectedDiscId = "ULKS-46116";
constexpr const char* kExpectedParamSfoDiscId = "ULKS46116";
constexpr const char* kExpectedDiscVersion = "1.09";

struct Variant
{
    std::string disc_id;
    std::string disc_version;
    std::string title;
    std::string executable;
    std::string sha1;
    size_t size{0};
};

enum class IdentifyStatus
{
    Success = 0,
    MissingDirectory = 1,
    MissingParamSfo = 2,
    InvalidParamSfo = 3,
    MissingExecutable = 4,
    UnregisteredVariant = 5,
};

struct IdentifyResult
{
    IdentifyStatus status{IdentifyStatus::MissingDirectory};
    std::string message;
    std::string disc_id;
    std::string disc_version;
    std::string title;
    std::string executable_name;
    std::string executable_sha1;
    size_t executable_size{0};
    const Variant* matched_variant{nullptr};
};

bool LoadVariants(const std::filesystem::path& json_path, std::vector<Variant>& out_variants);
bool LoadVariantsFromString(const std::string& json_str, std::vector<Variant>& out_variants);
const Variant* FindVariant(const std::vector<Variant>& variants,
                           const std::string& disc_id,
                           const std::string& disc_version,
                           const std::string& sha1);
IdentifyResult IdentifyGameDirectory(const std::filesystem::path& game_dir,
                                     const std::vector<Variant>& variants);

} // namespace redmp2

#endif // REDMP2_DMP2_PROFILE_H
