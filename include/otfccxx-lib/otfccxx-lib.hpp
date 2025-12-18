#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <memory>
#include <span>
#include <stdio.h>
#include <utility>

#include <hb-subset.hh>
#include <json-builder.h>
#include <nlohmann/json.hpp>
#include <otfcc/font.h>
#include <otfcc/options.h>
#include <otfcc/sfnt.h>

#include <otfccxx-lib/fmem_file.hpp>

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
};

// Simply wraps otfcc_Options
class otfccxx_Options {

public:
    explicit otfccxx_Options(otfcc_Options &obj) noexcept : ptr_(&obj) {}

    explicit otfccxx_Options() noexcept { ptr_ = otfcc_newOptions(); }
    explicit otfccxx_Options(uint8_t optLevel) noexcept {
        ptr_ = otfcc_newOptions();
        otfcc_Options_optimizeTo(ptr_, optLevel);
    }

    otfccxx_Options(const otfccxx_Options &) = delete;
    otfccxx_Options &
    operator=(const otfccxx_Options &) = delete;

    ~otfccxx_Options() { otfcc_deleteOptions(ptr_); }

    otfcc_Options *
    operator->() const noexcept {
        return ptr_;
    }
    otfcc_Options &
    operator*() const noexcept {
        return *ptr_;
    }

    // Optional explicit access
    otfcc_Options &
    get() const noexcept {
        return *ptr_;
    }

private:
    otfcc_Options *ptr_; // non-owning
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

class Modifier {
private:
    class Impl;

public:
    Modifier();
    Modifier(std::span<const std::byte> raw_ttfFont, otfccxx_Options const &opts);

    std::expected<bool, err_subset>
    change_unitsPerEm(uint32_t newEmSize);
    std::expected<bool, err_subset>
    change_makeMonospaced(uint32_t newAdvWidth);

    std::expected<std::vector<font_raw>, err_subset>
    exportResult() {
        if (! pimpl) { return std::unexpected(err_subset::unexpectedNullptr); }

        return std::unexpected(err_subset::unknownError);
    };

private:
    std::unique_ptr<Impl> pimpl;
};

inline std::expected<nlohmann::ordered_json, err>
dump_toNLMJSON(std::span<const std::byte> raw_ttfFont, otfccxx_Options const &opts) {

    otfcc_SplineFontContainer *sfnt;
    otfcc_Font                *curFont;
    json_value                *root;

    std::expected<nlohmann::ordered_json, err> res = std::unexpected(err::unknownError);

    fmem_file fmf(raw_ttfFont);

    // Read sfnt
    {
        sfnt = otfcc_readSFNT(fmf.get());
        if (! sfnt) {
            res = std::unexpected(err::SFNT_cannotReadSFNT);
            goto RET;
        }
        if (0 >= sfnt->count) {
            res = std::unexpected(err::SFNT_subfontIndexOutOfRange);
            goto RET;
        }
    }

    // Build otfcc representation of font
    {
        otfcc_IFontBuilder *reader = otfcc_newOTFReader();
        curFont                    = reader->read(sfnt, 0, opts.operator->());
        reader->free(reader);
        if (! curFont) {
            res = std::unexpected(err::SFNT_fontStructureBrokenOrCorrupted);
            goto RET;
        }
    }

    // otfcc Consolidate font
    otfcc_iFont.consolidate(curFont, opts.operator->());

    //  otfcc create JSON value representation from otfcc_Font
    {
        otfcc_IFontSerializer *dumper = otfcc_newJsonWriter();
        root                          = (json_value *)dumper->serialize(curFont, opts.operator->());
        dumper->free(dumper);
        if (! root) {
            res = std::unexpected(err::SFNT_fontStructureBrokenOrCorrupted);
            goto RET;
        }
    }

    //  Write serialized JSON data into a buffer
    char  *buf;
    size_t buflen;
    {
        json_serialize_opts jsonOptions;
        jsonOptions.mode        = json_serialize_mode_packed;
        jsonOptions.opts        = 0;
        jsonOptions.indent_size = 4;

        buflen = json_measure_ex(root, jsonOptions);
        buf    = (char *)calloc(1, buflen);
        json_serialize_ex(buf, root, jsonOptions);
    }

    // Create nlmJson representation
    res = nlohmann::ordered_json::parse(std::span<char>(buf, buflen));
    free(buf);

RET:
    if (root) { json_builder_free(root); }
    if (curFont) { otfcc_iFont.free(curFont); }
    if (sfnt) { otfcc_deleteSFNT(sfnt); }

    return res;
};

inline std::expected<font_raw, err>
build_fromNLMJSON(nlohmann::ordered_json const &nlmJson, otfccxx_Options const &opts) {
    return {};
};

class Font {
    using NLMjson = nlohmann::ordered_json;

public:
    Font(std::string_view const &jsonFont) : font_(NLMjson::parse(jsonFont)) {}
    Font(NLMjson &&jsonFont) : font_(std::move(jsonFont)) {}

    // Glyph metric modification
    static std::expected<bool, err>
    transform_glyphByAW(NLMjson &out_glyph, double const newWidth) {

        auto aw_iter = out_glyph.find("advanceWidth");
        if (aw_iter == out_glyph.end()) { return std::unexpected(err::jsonAdvanceWidthKeyNotFound); }

        // If the difference is small enough, we don't transform
        double const curAdvW = static_cast<double>(*aw_iter);
        if (std::abs(newWidth - curAdvW) < 0.5) { return false; }
        int const moveBy = std::round((newWidth - curAdvW) / 2);

        auto contours_iter = out_glyph.find("contours");

        if (contours_iter != out_glyph.end()) {
            for (auto &[_, contGrp] : contours_iter->items()) {
                for (auto &[__, contPoint] : contGrp.items()) {
                    // Move each contour point on X axis
                    contPoint.at("x") = static_cast<int>(contPoint.at("x")) + moveBy;
                }
            }
        }

        auto references_iter = out_glyph.find("references");
        if (references_iter != out_glyph.end()) {
            for (auto &[_, oneRef] : references_iter->items()) {
                oneRef.at("x") = static_cast<int>(oneRef.at("x")) + moveBy;
            }
        }

        (*aw_iter) = static_cast<int>(std::round(newWidth));
        return true;
    }

    // func must accept NLMjson & as a first (out)parameter for its call operator
    // Returns number of non-transformed glyphs
    std::expected<size_t, err>
    transform_allGlyphs(auto &&func) {
        auto glyf_table = font_.find("glyf");
        if (glyf_table == font_.end()) { return std::unexpected(err::jsonFontMissingGlyfTable); }

        size_t res = 0;
        for (auto &[name, oneGlyf] : glyf_table->items()) {
            if (auto curRes = func(oneGlyf); curRes.has_value()) { res += (not curRes.value()); }
            else { return std::unexpected(curRes.error()); }
        }
        return res;
    }

    // Erase methods
    std::expected<bool, err>
    erase_glyphOrder() {
        return true;
    }

private:
    NLMjson font_;
};

} // namespace otfccxx