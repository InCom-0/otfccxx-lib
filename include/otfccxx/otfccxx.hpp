#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <utility>

#include <cstddef>
#include <vector>


#if defined(OTFCCXX_SHARED)
#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(OTFCCXX_EXPORTS)
#define OTFCCXX_API __declspec(dllexport)
#else
#define OTFCCXX_API __declspec(dllimport)
#endif
#else
#define OTFCCXX_API __attribute__((visibility("default")))
#endif

#else
// Static library
#define OTFCCXX_API
#endif


namespace otfccxx {

// #####################################################################
// ### Forward declarations ###
// #####################################################################
class Modifier;
class Subsetter;
class Options;


// #####################################################################
// ### Type aliases ###
// #####################################################################
using Bytes    = std::vector<std::byte>;
using ByteSpan = std::span<const std::byte>;


// #####################################################################
// ### Public enums ###
// #####################################################################
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
    ratioAdvWidthToEmSize_cannotBeNegative,
    ratioAdvWidthToEmSize_cannotBeOver2,
    newEmSize_outsideValidValueRange,
    otfccHandle_notIndex,
};
enum class err_converter : size_t {
    unknownError = 1,
    unexpectedNullptr,
    woff2_dataInvalid,
    woff2_decompressionFailed
};

OTFCCXX_API std::expected<bool, std::filesystem::file_type>
            write_bytesToFile(std::filesystem::path const &p, ByteSpan bytes);


// #####################################################################
// ### Classes forming the public interface ###
// #####################################################################

// Simply wraps otfcc_Options
class OTFCCXX_API Options {
private:
public:
    explicit Options() noexcept;
    explicit Options(uint8_t const optLevel, bool const removeTTFhints = true) noexcept;

    Options(const Options &) = delete;
    Options &
    operator=(const Options &) = delete;

    ~Options();

private:
    friend class Modifier;

    class Impl;
    std::unique_ptr<Impl> pimpl;
};

// 'Waterfall' subsetter that subsets a collection of fonts in a priority waterfall fashion based on the requested
// unicode codepoints. Has 'builder pattern' - like interface.
class OTFCCXX_API Subsetter {
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
    // unicode points that weren't found in any font
    std::expected<std::vector<Bytes>, err_subset>
    execute();
    std::expected<std::pair<std::vector<Bytes>, std::vector<uint32_t>>, err_subset>
    execute_bestEffort();

    bool
    is_inError();
    err_subset
    get_error();

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};


// Provides high-level modification capability for TTF fonts. The functionality is very limited in scope.
// TTF hints are always removed from the font
class OTFCCXX_API Modifier {
public:
    Modifier(ByteSpan raw_ttfFont, uint32_t ttcindex = 0, Options const &opts = otfccxx::Options(1, true));
    Modifier(std::filesystem::path const &pth, uint32_t ttcindex = 0, Options const &opts = otfccxx::Options(1, true));

    Modifier() = delete;
    ~Modifier();


    // Changing dimensions of glyphs
    std::expected<bool, err_modifier>
    change_unitsPerEm(uint32_t newEmSize);
    std::expected<bool, err_modifier>
    change_makeMonospaced(uint32_t const targetAdvWidth);
    std::expected<bool, err_modifier>
    change_makeMonospaced_byEmRatio(double const emRatio);

    // Filtering of font content (ie. deleting parts of the font)
    void
    delete_fontTable(const uint32_t tag);


    // Modifications of other values and properties
    std::expected<bool, err_modifier>
    remove_ttfHints();


    // Export
    std::expected<Bytes, err_modifier>
    exportResult(Options const &opts = otfccxx::Options(1));

private:
    class Impl;
    std::unique_ptr<Impl> pimpl;
};


class OTFCCXX_API Converter {
public:
    static size_t
    max_compressed_size(ByteSpan data);
    static size_t
    max_compressed_size(ByteSpan data, const std::string &extended_metadata);

    [[nodiscard]] static std::expected<Bytes, err_converter>
    encode_Woff2(ByteSpan ttf);
    [[nodiscard]] static std::expected<Bytes, err_converter>
    decode_Woff2(ByteSpan ttf);
    
    [[nodiscard]] static std::expected<std::string, err_converter>
    encode_base64(ByteSpan ttf);
};

} // namespace otfccxx