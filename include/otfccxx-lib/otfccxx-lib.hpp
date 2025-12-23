#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <utility>

#include <cstddef>
#include <vector>


namespace otfccxx {
using font_raw = std::vector<std::byte>;

enum class err : size_t {
    unknownError = 1,
    unexpectedNullptr,
    jsonAdvanceWidthKeyNotFound,
    jsonFontMissingGlyfTable,
    SFNT_cannotReadSFNT,
    SFNT_subfontIndexOutOfRange,
    SFNT_fontStructureBrokenOrCorrupted,
};
enum class err_subset : size_t {
    unknownError = 1,
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
enum class err_modifier : size_t {
    unknownError = 1,
    unexpectedNullptr,
    missingJSONKey,
    unexpectedJSONValueType,
    counterPointHasCorruptedStructure,
    referenceHasCorruptedStructure,
    cyclicGlyfReferencesFound,
    missingGlyphInGlyfTable,
    glyphHasBothCountoursAndReferences,
};


std::expected<bool, std::filesystem::file_type>
write_bytesToFile(std::filesystem::path const &p, std::span<const std::byte> bytes);

class Subsetter {
private:
    class Impl;

public:
    Subsetter();                      // defined in the implementation file

    ~Subsetter();                     // defined in the implementation file, where impl is a complete
                                      // type
    Subsetter(Subsetter &&) noexcept; // defined in the implementation file
    Subsetter(const Subsetter &) = delete;
    Subsetter &
    operator=(Subsetter &&) noexcept; // defined in the implementation file
    Subsetter &
    operator=(const Subsetter &) = delete;

    Subsetter &
    add_ff_toSubset(std::span<const char> buf, unsigned int const faceIndex = 0u);
    Subsetter &
    add_ff_categoryBackup(std::span<const char> buf, unsigned int const faceIndex = 0u);
    Subsetter &
    add_ff_lastResort(std::span<const char> buf, unsigned int const faceIndex = 0u);

    Subsetter &
    add_ff_toSubset(std::filesystem::path const &pth, unsigned int const faceIndex = 0u);
    Subsetter &
    add_ff_categoryBackup(std::filesystem::path const &pth, unsigned int const faceIndex = 0u);
    Subsetter &
    add_ff_lastResort(std::filesystem::path const &pth, unsigned int const faceIndex = 0u);

    // Subsetter &add_ff_toSubset(hb_face_t *ptr, unsigned int const faceIndex =
    // 0u); Subsetter &add_ff_categoryBackup(hb_face_t *ptr, unsigned int const
    // faceIndex = 0u); Subsetter &add_ff_lastResort(hb_face_t *ptr, unsigned int
    // const faceIndex = 0u);

    Subsetter &
    add_toKeep_CP(uint32_t cp);
    Subsetter &
    add_toKeep_CPs(std::span<const uint32_t> cps);

    // 1) execute() - Get 'waterfall of font faces'
    // 2) execute_bestEffort() - Get 'waterfall of font faces' + set(in a vector)
    // a unicode points that weren't found in any font
    std::expected<std::vector<font_raw>, err_subset>
    execute();
    std::expected<std::pair<std::vector<font_raw>, std::vector<uint32_t>>, err_subset>
    execute_bestEffort();

    bool
    is_inError();
    err_subset
    get_error();

private:
    std::unique_ptr<Impl> pimpl;
};


// Forward declaration to be able to be a friend of 'Options'
class Modifier;

// Simply wraps otfcc_Options
class Options {
private:
    class Impl;
    friend class Modifier;

public:
    explicit Options() noexcept;
    explicit Options(uint8_t optLevel) noexcept;

    Options(const Options &) = delete;
    Options &
    operator=(const Options &) = delete;

    ~Options();

private:
    std::unique_ptr<Impl> pimpl;
};

class Modifier {
private:
    class Impl;

public:
    Modifier();
    Modifier(std::span<const std::byte> raw_ttfFont, Options const &opts = otfccxx::Options(1), uint32_t ttcindex = 0);
    ~Modifier();

    // Changing dimensions of glyphs
    std::expected<bool, err_modifier>
    change_unitsPerEm(uint32_t newEmSize);
    std::expected<bool, err_modifier>
    change_makeMonospaced(uint32_t targetAdvWidth);

    // Filtering of font content (ie. deleting parts of the font)


    // Modifications of other values and properties


    // Export
    std::expected<font_raw, err_modifier>
    exportResult(Options const &opts = otfccxx::Options(1));

private:
    std::unique_ptr<Impl> pimpl;
};

} // namespace otfccxx