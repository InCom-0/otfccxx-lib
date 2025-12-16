#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>


#include <hb-subset.hh>
// #include <nlohmann/json.hpp>
#include <otfcc/font.h>
#include <otfcc/sfnt.h>


namespace fontsmith {
using font_raw = std::vector<std::byte>;

enum class err : size_t {
    uknownError = 1,
    unexpectedNullptr,
    hb_blob_t_createFailure,
    hb_face_t_createFailure,
    execute_someRequestedGlyphsAreMissing,
    subsetInput_failedToCreate,
    hb_subset_executeFailure,
    make_subset_noIntersectingGlyphs,
    jsonAdvanceWidthKeyNotFound,
    jsonFontMissingGlyfTable,
};


std::expected<bool, std::filesystem::file_type> write_bytesToFile(std::filesystem::path const &p,
                                                                  std::span<const std::byte>   bytes);

class Subsetter {
private:
    class Impl;

public:
    Subsetter();                                 // defined in the implementation file

    ~Subsetter();                                // defined in the implementation file, where impl is a complete type
    Subsetter(Subsetter &&) noexcept;            // defined in the implementation file
    Subsetter(const Subsetter &) = delete;
    Subsetter &operator=(Subsetter &&) noexcept; // defined in the implementation file
    Subsetter &operator=(const Subsetter &) = delete;


    Subsetter &add_ff_toSubset(std::span<const char> buf, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_categoryBackup(std::span<const char> buf, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_lastResort(std::span<const char> buf, unsigned int const faceIndex = 0u);

    Subsetter &add_ff_toSubset(std::filesystem::path const &pth, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_categoryBackup(std::filesystem::path const &pth, unsigned int const faceIndex = 0u);
    Subsetter &add_ff_lastResort(std::filesystem::path const &pth, unsigned int const faceIndex = 0u);

    // Subsetter &add_ff_toSubset(hb_face_t *ptr, unsigned int const faceIndex = 0u);
    // Subsetter &add_ff_categoryBackup(hb_face_t *ptr, unsigned int const faceIndex = 0u);
    // Subsetter &add_ff_lastResort(hb_face_t *ptr, unsigned int const faceIndex = 0u);

    Subsetter &add_toKeep_CP(uint32_t cp);
    Subsetter &add_toKeep_CPs(std::span<const uint32_t> cps);

    // 1) execute() - Get 'waterfall of font faces'
    // 2) execute_bestEffort() - Get 'waterfall of font faces' + set a unicode points that weren't found in any font
    std::expected<std::vector<font_raw>, err>                                   execute();
    std::expected<std::pair<std::vector<font_raw>, std::vector<uint32_t>>, err> execute_bestEffort();


    bool is_inError();
    err  get_error();


private:
    std::unique_ptr<Impl> pimpl;
};


class Modifier {
private:
    class Impl;

public:
    Modifier();
    Modifier(std::span<const std::byte> rawFont_ttf);


    std::expected<bool, err> change_unitsPerEm(uint32_t newEmSize);
    std::expected<bool, err> change_makeMonospaced(uint32_t newAdvWidth);


private:
    std::unique_ptr<Impl> pimpl;
};

} // namespace fontsmith