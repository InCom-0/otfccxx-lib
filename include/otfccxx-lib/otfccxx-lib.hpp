#pragma once

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <expected>

#include <span>
#include <stdio.h>
#include <utility>

#include <json-builder.h>
#include <nlohmann/json.hpp>
#include <otfcc/font.h>
#include <otfcc/options.h>
#include <otfcc/sfnt.h>

#include <otfccxx-lib/fmem_file.hpp>


namespace otfccxx {
using font_raw = std::vector<std::byte>;

enum class err : size_t {
    uknownError = 1,
    unexpectedNullptr,
    jsonAdvanceWidthKeyNotFound,
    jsonFontMissingGlyfTable,
    SFNT_cannotReadSFNT,
    SFNT_subfontIndexOutOfRange,
    SFNT_fontStructureBrokenOrCorrupted,
};

class otfccxx_Options {

public:
    explicit otfccxx_Options(otfcc_Options &obj) noexcept : ptr_(&obj) {}

    explicit otfccxx_Options() noexcept { ptr_ = otfcc_newOptions(); }
    explicit otfccxx_Options(uint8_t optLevel) noexcept {
        ptr_ = otfcc_newOptions();
        otfcc_Options_optimizeTo(ptr_, optLevel);
    }

    otfccxx_Options(const otfccxx_Options &)            = delete;
    otfccxx_Options &operator=(const otfccxx_Options &) = delete;

    ~otfccxx_Options() { otfcc_deleteOptions(ptr_); }


    otfcc_Options *operator->() const noexcept { return ptr_; }
    otfcc_Options &operator*() const noexcept { return *ptr_; }

    // Optional explicit access
    otfcc_Options &get() const noexcept { return *ptr_; }

private:
    otfcc_Options *ptr_; // non-owning
};

inline std::expected<nlohmann::ordered_json, err> dump_toNLMJSON(std::span<const std::byte> raw_ttfFont,
                                                                 otfccxx_Options const     &opts) {

    otfcc_SplineFontContainer *sfnt;
    otfcc_Font                *curFont;
    json_value                *root;

    std::expected<nlohmann::ordered_json, err> res = err::uknownError;

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

inline std::expected<font_raw, err> build_fromNLMJSON(nlohmann::ordered_json const &nlmJson,
                                                      otfccxx_Options const        &opts) {
    return {};
};


class Font {
    using NLMjson = nlohmann::ordered_json;

public:
    Font(std::string_view const &jsonFont) : font_(NLMjson::parse(jsonFont)) {}
    Font(NLMjson &&jsonFont) : font_(std::move(jsonFont)) {}

    // Glyph metric modification
    static std::expected<bool, err> transform_glyphSize(NLMjson &out_glyph, double const a, double const b,
                                                        double const c, double const d, double const dx,
                                                        double const dy) {

        auto aw_iter = out_glyph.find("advanceWidth");
        if (aw_iter == out_glyph.end()) { return std::unexpected(err::jsonAdvanceWidthKeyNotFound); }

        (*aw_iter) = static_cast<int>(round(a * static_cast<double>(out_glyph.at("advanceWidth"))));

        if (auto ah_iter = out_glyph.find("advanceHeight"); ah_iter != out_glyph.end()) {
            (*ah_iter) = static_cast<int>(round(d * static_cast<double>(*ah_iter)));
        }
        if (auto vo_iter = out_glyph.find("verticalOrigin"); vo_iter != out_glyph.end()) {
            (*vo_iter) = static_cast<int>(round(d * static_cast<double>(*vo_iter)));
        }

        if (out_glyph.find("contours") != out_glyph.end()) {
            for (auto &contour : out_glyph["contours"]) {
                for (auto &point : contour) {
                    double const x = point.at("x");
                    double const y = point.at("y");
                    point["x"]     = static_cast<int>(a * x + b * y + dx);
                    point["y"]     = static_cast<int>(c * x + d * y + dy);
                }
            }
        }
        if (out_glyph.find("references") != out_glyph.end()) {
            for (auto &reference : out_glyph["references"]) {
                double const x = reference.at("x");
                double const y = reference.at("y");
                reference["x"] = static_cast<int>(a * x + b * y + dx);
                reference["y"] = static_cast<int>(c * x + d * y + dy);
            }
        }
        return true;
    }

    static std::expected<bool, err> transform_glyphByAW(NLMjson &out_glyph, double const newWidth) {

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
    std::expected<size_t, err> transform_allGlyphs(auto &&func) {
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
    std::expected<bool, err> erase_glyphOrder() { return true; }


private:
    NLMjson font_;
};


} // namespace otfccxx