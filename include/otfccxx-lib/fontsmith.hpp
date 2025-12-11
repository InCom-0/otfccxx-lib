#pragma once

#include "hb-subset.h"
#include "hb.h"
#include <expected>
#include <filesystem>

#include <optional>
#include <span>


#include <hb-subset.hh>
// #include <nlohmann/json.hpp>
#include <otfcc/font.h>
#include <otfcc/sfnt.h>


namespace fontsmith {

enum class err : size_t {
    uknownError = 1,
    hb_blob_t_createFailure,
    hb_face_t_createFailure,
    execute_someRequestedGlyphsAreMissing,
    subsetInput_failedToCreate,
    hb_subset_executeFailure,
    make_subset_noIntersectingGlyphs,
    unexpectedNullptr,
};
using hb_face_uptr = std::unique_ptr<hb_face_t, decltype([](hb_face_t *f) noexcept {
                                         if (f) { hb_face_destroy(f); }
                                     })>;
using hb_blob_uptr = std::unique_ptr<hb_blob_t, decltype([](hb_blob_t *b) noexcept {
                                         if (b) { hb_blob_destroy(b); }
                                     })>;

using hb_set_uptr = std::unique_ptr<hb_set_t, decltype([](hb_set_t *s) noexcept {
                                        if (s) { hb_set_destroy(s); }
                                    })>;

using hb_subset_input_uptr = std::unique_ptr<hb_subset_input_t, decltype([](hb_subset_input_t *si) noexcept {
                                                 if (si) { hb_subset_input_destroy(si); }
                                             })>;


std::expected<bool, std::filesystem::file_type> write_bytesToFile(std::filesystem::path const &p,
                                                                  std::span<const std::byte>   bytes);

class Subsetter {

public:
    Subsetter &add_ff_toSubset(std::span<const char> buf, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_categoryBackup(std::span<const char> buf, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_lastResort(std::span<const char> buf, unsigned int const faceIndex = 0u);

    Subsetter &add_ff_toSubset(std::filesystem::path const &pth, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_categoryBackup(std::filesystem::path const &pth, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_lastResort(std::filesystem::path const &pth, unsigned int const faceIndex = 0u);

    Subsetter &add_ff_toSubset(hb_face_t *ptr, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_categoryBackup(hb_face_t *ptr, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_lastResort(hb_face_t *ptr, unsigned int const faceIndex = 0u);

    Subsetter &add_toKeep_CP(hb_codepoint_t cp);
    Subsetter &add_toKeep_CPs(std::span<const hb_codepoint_t> cps);

    // 1) execute() - Get 'waterfall of font faces'
    // 2) execute_bestEffort() - Get 'waterfall of font faces' + set a unicode points that weren't found in any font
    std::expected<std::vector<hb_face_uptr>, err>                         execute();
    std::expected<std::pair<std::vector<hb_face_uptr>, hb_set_uptr>, err> execute_bestEffort();


    bool is_inError() { return inError.has_value(); }
    err  get_error() { return inError.value(); }


private:
    std::expected<hb_face_uptr, err> make_ff(std::span<const char> const &buf, unsigned int const faceIndex);

    std::expected<hb_face_uptr, err> make_subset(hb_face_t *ff);
    std::expected<bool, err>         should_include_category(hb_face_t *ff);

    hb_set_uptr toKeep_unicodeCPs;

    // 1) ffs_toSubset - Main font(s) to subset
    // 2) ffs_categoryBackup - Fonts that may be included as a whole (the intended usecase is for already minified fonts
    // include eg. one unicode character category only)
    // 3) ffs_lastResort - If after going through the above we still have some unicodeCPs to keep (because they are NOT
    // in either of the above) ... font faces with large unicode CP coverage are good here (ie. Iosevka)
    std::vector<hb_face_uptr> ffs_toSubset;
    std::vector<hb_face_uptr> ffs_categoryBackup;
    std::vector<hb_face_uptr> ffs_lastResort;


    std::optional<err> inError = std::nullopt;
};


class CompatMaker {
public:
private:
};

} // namespace fontsmith