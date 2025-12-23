#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

#include <json-builder.h>
#include <json.h>

#include <hb-set.hh>
#include <hb-subset.h>

#include <otfccxx-lib/fmem_file.hpp>
#include <otfccxx-lib/otfccxx-lib.hpp>

#include <otfcc/otfcc_api.h>

namespace otfccxx {

namespace detail {
struct _hb_face_uptr_deleter {
    void
    operator()(hb_face_t *f) const noexcept {
        if (f) { hb_face_destroy(f); }
    }
};
struct _hb_blob_uptr_deleter {
    void
    operator()(hb_blob_t *b) const noexcept {
        if (b) { hb_blob_destroy(b); }
    }
};
struct _hb_set_uptr_deleter {
    void
    operator()(hb_set_t *s) const noexcept {
        if (s) { hb_set_destroy(s); }
    }
};
struct _hb_subset_input_uptr_deleter {
    void
    operator()(hb_subset_input_t *s) const noexcept {
        if (s) { hb_subset_input_destroy(s); }
    }
};

struct _json_value_uptr_deleter {
    void
    operator()(json_value *j) const noexcept {
        if (j) { json_builder_free(j); }
    }
};
struct _otfcc_opt_uptr_deleter {
    void
    operator()(otfcc_Options *o) const noexcept {
        if (o) { otfcc_deleteOptions(o); }
    }
};


} // namespace detail

using hb_face_uptr         = std::unique_ptr<hb_face_t, detail::_hb_face_uptr_deleter>;
using hb_blob_uptr         = std::unique_ptr<hb_blob_t, detail::_hb_blob_uptr_deleter>;
using hb_set_uptr          = std::unique_ptr<hb_set_t, detail::_hb_set_uptr_deleter>;
using hb_subset_input_uptr = std::unique_ptr<hb_subset_input_t, detail::_hb_subset_input_uptr_deleter>;

using json_value_uptr = std::unique_ptr<json_value, detail::_json_value_uptr_deleter>;
using otfcc_opt_uptr  = std::unique_ptr<otfcc_Options, detail::_otfcc_opt_uptr_deleter>;

struct AccessInfo {
    bool readable;
    bool writable;
};

static std::expected<AccessInfo, std::filesystem::file_type>
check_access(const std::filesystem::path &p) {
    namespace fs = std::filesystem;
    std::error_code ec;

    // Resolve symlink (follows by default)
    auto ft = fs::status(p, ec).type();
    if (ec || ft == fs::file_type::not_found) { return std::unexpected(fs::file_type::not_found); }

    // Only allow normal files and directories
    if (ft != fs::file_type::regular && ft != fs::file_type::directory) { return std::unexpected(ft); }

    AccessInfo res{false, false};

    // ---- Regular file ----
    if (ft == fs::file_type::regular) {
        res.readable = std::ifstream(p).good();
        res.writable = std::ofstream(p, std::ios::app).good();
    }

    // ---- Directory ----
    else if (ft == fs::file_type::directory) {
        // Check directory readability by attempting to iterate
        fs::directory_iterator it(p, ec);
        res.readable = ! ec;

        // Check directory writability by trying to create a temp file
        auto          testfile = p / ".fs_test.tmp";
        std::ofstream ofs(testfile);
        if (ofs.good()) {
            res.writable = true;
            ofs.close();
            fs::remove(testfile, ec);
        }
    }

    return res;
}

std::expected<bool, std::filesystem::file_type>
write_bytesToFile(std::filesystem::path const &p, std::span<const std::byte> bytes) {
    if (not p.has_filename()) { return std::unexpected(std::filesystem::file_type::not_found); }
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) { return false; }
    }
    else { return std::unexpected(std::filesystem::file_type::not_found); }

    if (auto exp_accInfo = check_access(p.parent_path()); not exp_accInfo.has_value()) {
        return std::unexpected(exp_accInfo.error());
    }
    else if (not exp_accInfo.value().writable) { return false; }

    std::ofstream outs(p, std::ios::binary);
    if (! outs) { return false; }

    outs.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size_bytes()));
    outs.flush();
    return outs.good();
}

class Options::Impl {
    friend class Modifier;

public:
    Impl() : _opts(otfcc_newOptions()) {}
    Impl(uint8_t optLevel) : _opts(otfcc_newOptions()) {
        otfcc_Options_optimizeTo(_opts.get(), optLevel);
        _opts->logger = otfcc_newLogger(otfcc_newStdErrTarget());
        _opts->logger->indent(_opts->logger, "[missing]");
        _opts->decimal_cmap = true;
    }

private:
    otfcc_opt_uptr _opts;
};


Options::Options() noexcept : pimpl(std::make_unique<Impl>()) {}
Options::Options(uint8_t optLevel) noexcept : pimpl(std::make_unique<Impl>(optLevel)) {}

Options::~Options() = default;


class Subsetter::Impl {
    friend class Subsetter;

public:
    Impl() : toKeep_unicodeCPs(hb_set_create()) {}

private:
    void
    add_ff_toSubset(std::span<const char> &buf, unsigned int const faceIndex) {
        if (auto toInsert = make_ff(buf, faceIndex); toInsert.has_value()) {
            ffs_toSubset.push_back(std::move(toInsert.value()));
        }
    }
    void
    add_ff_categoryBackup(std::span<const char> &buf, unsigned int const faceIndex) {
        if (auto toInsert = make_ff(buf, faceIndex); toInsert.has_value()) {
            ffs_categoryBackup.push_back(std::move(toInsert.value()));
        }
    }
    void
    add_ff_lastResort(std::span<const char> &buf, unsigned int const faceIndex) {
        if (auto toInsert = make_ff(buf, faceIndex); toInsert.has_value()) {
            ffs_lastResort.push_back(std::move(toInsert.value()));
        }
    }

    void
    add_ff_toSubset(std::filesystem::path const &pth, unsigned int const faceIndex) {
        if (auto exp_access = check_access(pth); exp_access.has_value()) {
            if (exp_access->readable) {
                std::ifstream file(pth, std::ios::binary);
                if (! file) { goto RET; }

                std::vector<char> const data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                if (data.empty()) { goto RET; }

                if (auto toInsert = make_ff(data, faceIndex); toInsert.has_value()) {
                    ffs_toSubset.push_back(std::move(toInsert.value()));
                }
            }
        }
    RET:
        return;
    }
    void
    add_ff_categoryBackup(std::filesystem::path const &pth, unsigned int const faceIndex) {
        if (auto exp_access = check_access(pth); exp_access.has_value()) {
            if (exp_access->readable) {
                std::ifstream file(pth, std::ios::binary);
                if (! file) { goto RET; }

                std::vector<char> const data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                if (data.empty()) { goto RET; }

                if (auto toInsert = make_ff(data, faceIndex); toInsert.has_value()) {
                    ffs_categoryBackup.push_back(std::move(toInsert.value()));
                }
            }
        }
    RET:
        return;
    }
    void
    add_ff_lastResort(std::filesystem::path const &pth, unsigned int const faceIndex) {
        if (auto exp_access = check_access(pth); exp_access.has_value()) {
            if (exp_access->readable) {
                std::ifstream file(pth, std::ios::binary);
                if (! file) { goto RET; }

                std::vector<char> const data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                if (data.empty()) { goto RET; }

                if (auto toInsert = make_ff(data, faceIndex); toInsert.has_value()) {
                    ffs_lastResort.push_back(std::move(toInsert.value()));
                }
            }
        }
    RET:
        return;
    }

    std::expected<hb_face_uptr, err_subset>
    make_ff(std::span<const char> const &buf, unsigned int const faceIndex) {
        hb_blob_uptr blob(
            hb_blob_create_or_fail(buf.data(), buf.size_bytes(), HB_MEMORY_MODE_DUPLICATE, nullptr, nullptr));
        if (! blob) { return std::unexpected(err_subset::hb_blob_t_createFailure); }

        // Create face from blob
        hb_face_uptr face(hb_face_create_or_fail(blob.get(), faceIndex));
        if (! face) { return std::unexpected(err_subset::hb_face_t_createFailure); }

        return face;
    }

    std::expected<hb_face_uptr, err_subset>
    make_subset(hb_face_t *ff) {
        hb_set_uptr unicodes_toKeep_in_ff(hb_set_create());
        hb_face_collect_unicodes(ff, unicodes_toKeep_in_ff.get());

        hb_set_intersect(unicodes_toKeep_in_ff.get(), toKeep_unicodeCPs.get());
        if (hb_set_is_empty(unicodes_toKeep_in_ff.get())) {
            return std::unexpected(err_subset::make_subset_noIntersectingGlyphs);
        }

        // Set the unicodes to keep in the subsetted font
        hb_subset_input_uptr si(hb_subset_input_create_or_fail());
        if (! si) { return std::unexpected(err_subset::subsetInput_failedToCreate); }

        hb_set_t *si_inputUCCPs = hb_subset_input_unicode_set(si.get());
        hb_set_set(si_inputUCCPs, unicodes_toKeep_in_ff.get());

        // Set subsetting flags
        hb_subset_input_set_flags(si.get(), HB_SUBSET_FLAGS_DEFAULT);

        // Execute subsetting
        hb_face_uptr res(hb_subset_or_fail(ff, si.get()));
        if (! res) { return std::unexpected(err_subset::hb_subset_executeFailure); }

        // Only keep the remaining unicodeCPs by 'filtering' the ones we use from
        // 'ff'
        hb_set_symmetric_difference(toKeep_unicodeCPs.get(), unicodes_toKeep_in_ff.get());
        return res;
    }
    std::expected<bool, err_subset>
    should_include_category(hb_face_t *ff) {
        hb_set_uptr unicodes_toKeep_in_ff(hb_set_create());
        hb_face_collect_unicodes(ff, unicodes_toKeep_in_ff.get());

        hb_set_intersect(unicodes_toKeep_in_ff.get(), toKeep_unicodeCPs.get());

        bool res = not hb_set_is_empty(unicodes_toKeep_in_ff.get());

        // Only keep the remaining unicodeCPs by 'filtering' the ones we use from
        // 'ff'
        hb_set_symmetric_difference(toKeep_unicodeCPs.get(), unicodes_toKeep_in_ff.get());
        return res;
    }

    hb_set_uptr toKeep_unicodeCPs;

    // 1) ffs_toSubset - Main font(s) to subset
    // 2) ffs_categoryBackup - Fonts that may be included as a whole (the intended
    // usecase is for already minified fonts include eg. one unicode character
    // category only) 3) ffs_lastResort - If after going through the above we
    // still have some unicodeCPs to keep (because they are NOT in either of the
    // above) ... font faces with large unicode CP coverage are good here (ie.
    // Iosevka)
    std::vector<hb_face_uptr> ffs_toSubset;
    std::vector<hb_face_uptr> ffs_categoryBackup;
    std::vector<hb_face_uptr> ffs_lastResort;

    std::optional<err_subset> inError = std::nullopt;
};

Subsetter::Subsetter() : pimpl(std::make_unique<Impl>()) {};
Subsetter::~Subsetter()                     = default;
Subsetter::Subsetter(Subsetter &&) noexcept = default;
Subsetter &
Subsetter::operator=(Subsetter &&) noexcept = default;

// Adding FontFaces
Subsetter &
Subsetter::add_ff_toSubset(std::span<const char> buf, unsigned int const faceIndex) {
    pimpl->add_ff_toSubset(buf, faceIndex);
    return *this;
}
Subsetter &
Subsetter::add_ff_categoryBackup(std::span<const char> buf, unsigned int const faceIndex) {
    pimpl->add_ff_categoryBackup(buf, faceIndex);
    return *this;
}
Subsetter &
Subsetter::add_ff_lastResort(std::span<const char> buf, unsigned int const faceIndex) {
    pimpl->add_ff_lastResort(buf, faceIndex);
    return *this;
}

Subsetter &
Subsetter::add_ff_toSubset(std::filesystem::path const &pth, unsigned int const faceIndex) {
    pimpl->add_ff_toSubset(pth, faceIndex);
    return *this;
}
Subsetter &
Subsetter::add_ff_categoryBackup(std::filesystem::path const &pth, unsigned int const faceIndex) {
    pimpl->add_ff_categoryBackup(pth, faceIndex);
    return *this;
}
Subsetter &
Subsetter::add_ff_lastResort(std::filesystem::path const &pth, unsigned int const faceIndex) {
    pimpl->add_ff_lastResort(pth, faceIndex);
    return *this;
}

// Subsetter &Subsetter::add_ff_toSubset(hb_face_t *ptr, unsigned int const
// faceIndex) {
//     if (ptr) { ffs_toSubset.push_back(hb_face_uptr(ptr)); }
//     return *this;
// }

// Subsetter &Subsetter::add_ff_categoryBackup(hb_face_t *ptr, unsigned int
// const faceIndex) {
//     if (ptr) { ffs_categoryBackup.push_back(hb_face_uptr(ptr)); }
//     return *this;
// }

// Subsetter &Subsetter::add_ff_lastResort(hb_face_t *ptr, unsigned int const
// faceIndex) {
//     if (ptr) { ffs_lastResort.push_back(hb_face_uptr(ptr)); }
//     return *this;
// }

// Adding unicode character points and/or glyph IDs

Subsetter &
Subsetter::add_toKeep_CP(hb_codepoint_t const cp) {
    hb_set_add(pimpl->toKeep_unicodeCPs.get(), cp);
    return *this;
}

Subsetter &
Subsetter::add_toKeep_CPs(std::span<const hb_codepoint_t> const cps) {
    for (auto const &cp : cps) { hb_set_add(pimpl->toKeep_unicodeCPs.get(), cp); }
    return *this;
}

// Execution
std::expected<std::vector<font_raw>, err_subset>
Subsetter::execute() {
    if (auto res = execute_bestEffort(); res.has_value()) {
        if (res.value().second.empty()) { return std::move(res.value().first); }
        else { return std::unexpected(err_subset::execute_someRequestedGlyphsAreMissing); }
    }
    else { return std::unexpected(res.error()); }
}

std::expected<std::pair<std::vector<font_raw>, std::vector<uint32_t>>, err_subset>
Subsetter::execute_bestEffort() {
    std::vector<hb_blob_uptr> res;

    for (auto &ff_to : pimpl->ffs_toSubset) {
        if (hb_set_is_empty(pimpl->toKeep_unicodeCPs.get())) { goto RET; }

        auto exp_ff = pimpl->make_subset(ff_to.get());
        if (not exp_ff.has_value()) {
            if (exp_ff.error() == err_subset::make_subset_noIntersectingGlyphs) { continue; }
            else { return std::unexpected(exp_ff.error()); }
        }
        else { res.push_back(hb_blob_uptr(hb_face_reference_blob(exp_ff.value().get()))); }
    }

    for (auto &ff_to : pimpl->ffs_categoryBackup) {
        if (hb_set_is_empty(pimpl->toKeep_unicodeCPs.get())) { goto RET; }

        auto exp_ff = pimpl->should_include_category(ff_to.get());
        if (not exp_ff.has_value()) {
            if (exp_ff.error() == err_subset::make_subset_noIntersectingGlyphs) { continue; }
            else { return std::unexpected(exp_ff.error()); }
        }
        else { res.push_back(hb_blob_uptr(hb_face_reference_blob(ff_to.get()))); }
    }

    for (auto &ff_to : pimpl->ffs_lastResort) {
        if (hb_set_is_empty(pimpl->toKeep_unicodeCPs.get())) { goto RET; }

        auto exp_ff = pimpl->make_subset(ff_to.get());
        if (not exp_ff.has_value()) {
            if (exp_ff.error() == err_subset::make_subset_noIntersectingGlyphs) { continue; }
            else { return std::unexpected(exp_ff.error()); }
        }
        else { res.push_back(hb_blob_uptr(hb_face_reference_blob(exp_ff.value().get()))); }
    }

RET:
    std::vector<uint32_t> resVec;
    for (auto const &item : *pimpl->toKeep_unicodeCPs.get()) { resVec.push_back(item); }

    return std::make_pair(
        std::vector<font_raw>(std::from_range, res | std::views::transform([](auto const &item) {
                                                   unsigned int length;
                                                   const char  *data = hb_blob_get_data(item.get(), &length);
                                                   return font_raw(
                                                       std::from_range,
                                                       std::span(reinterpret_cast<const std::byte *>(data), length));
                                               })),
        std::move(resVec));
}

bool
Subsetter::is_inError() {
    return pimpl->inError.has_value();
}
err_subset
Subsetter::get_error() {
    return pimpl->inError.value();
}

class Modifier::Impl {
    friend class Modifier;

public:
    Impl() {}
    Impl(font_raw const &ttf) {}
    Impl(std::span<const std::byte> raw_ttfFont, Options const &opts, uint32_t ttcindex = 0) {
        otfccxx::fmem_file memfile(raw_ttfFont);

        otfcc_SplineFontContainer *sfnt = otfcc_readSFNT(memfile.get());
        if (! sfnt || sfnt->count == 0) { std::exit(1); }
        if (ttcindex >= sfnt->count) { std::exit(1); }

        // Build font
        otfcc_IFontBuilder *reader = otfcc_newOTFReader();
        otfcc_Font         *font   = reader->read(sfnt, ttcindex, opts.pimpl.get()->_opts.get());
        if (! font) { std::exit(1); }

        // Free no longer needed stuff
        reader->free(reader);
        if (sfnt) { otfcc_deleteSFNT(sfnt); }

        // Consolidate
        otfcc_iFont.consolidate(font, opts.pimpl.get()->_opts.get());


        otfcc_IFontSerializer *dumper = otfcc_newJsonWriter();
        _jsonFont                     = json_value_uptr((json_value *)dumper->serialize(font, opts.pimpl.get()->_opts.get()));

        if (! _jsonFont) { std::exit(1); }
        dumper->free(dumper);
    }

    ~Impl() = default;

private:
    class JSON_Access {
        friend class Modifier::Impl;

    private:
        static inline struct _json_value *
        get(json_value const &rf, const char *key) {
            if (rf.type == json_object) {
                for (unsigned int i = 0; i < rf.u.object.length; ++i) {
                    if (! strcmp(rf.u.object.values[i].name, key)) { return rf.u.object.values[i].value; }
                }
            }
            return nullptr;
        }

        static inline struct _json_value *
        get(json_value const &rf, int index) {
            if (rf.type != json_array || index < 0 || ((unsigned int)index) >= rf.u.array.length) { return nullptr; }
            return rf.u.array.values[index];
        }
    };

    struct HLPR_glyphByAW {
        json_int_t origLSB = 0;
        json_int_t movedBy = 0;
    };

    struct _Detail {
        static constexpr auto default_ksADW = [](const _json_value &glyphObj) -> bool {
            auto adwObj = JSON_Access::get(glyphObj, "advanceWidth");
            if (adwObj->u.integer == 0) { return true; }
            return false;
        };
    };


    // Glyph metric modification
    static std::expected<bool, err_modifier>
    transform_glyphSize(json_value &out_glyph, double const a, double const b, double const c, double const d,
                        double const dx, double const dy) {
        auto const adw = [&]() -> std::expected<bool, err_modifier> { return _pureChange_ADW(out_glyph, a); };
        auto const adh = [&](bool const) -> std::expected<bool, err_modifier> { return _pureChange_ADH(out_glyph, d); };
        auto const vertO = [&](bool const) -> std::expected<bool, err_modifier> {
            return _pureChange_VertO(out_glyph, d);
        };
        auto const cps = [&](bool const) -> std::expected<bool, err_modifier> {
            return _pureChange_CPs(out_glyph, a, b, c, d, dx, dy);
        };
        auto const refAnch = [&](bool const) -> std::expected<bool, err_modifier> {
            return _pureChange_RefAnchors(out_glyph, a, b, c, d, dx, dy);
        };

        return adw().and_then(adh).and_then(vertO).and_then(cps).and_then(refAnch);
    }

    std::expected<size_t, err_modifier>
    transform_glyphsSize(uint32_t newEmSize) {
        if (not _jsonFont) { return std::unexpected(err_modifier::unexpectedNullptr); }
        auto ht_ptr = JSON_Access::get(*_jsonFont, "head");
        if (ht_ptr == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (ht_ptr->type != json_type::json_object) { return std::unexpected(err_modifier::unexpectedJSONValueType); }

        auto upem = JSON_Access::get(*ht_ptr, "unitsPerEm");
        if (upem == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (upem->type != json_type::json_integer) { return std::unexpected(err_modifier::unexpectedJSONValueType); }

        double const a = (static_cast<double>(newEmSize) / upem->u.integer), b = 0, c = 0,
                     d = (static_cast<double>(newEmSize) / upem->u.integer), dx = 0, dy = 0;
        upem->u.integer = newEmSize;

        auto gt_ptr = JSON_Access::get(*_jsonFont, "glyf");
        if (gt_ptr == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (gt_ptr->type != json_type::json_object) { return std::unexpected(err_modifier::unexpectedJSONValueType); }
        auto const &glyfs = *gt_ptr;

        size_t res = 0uz;
        for (auto const &one_oe_glyph : glyfs.u.object) {
            if (one_oe_glyph.value == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
            if (one_oe_glyph.value->type != json_type::json_object) {
                return std::unexpected(err_modifier::unexpectedJSONValueType);
            }

            if (auto oneTransformRes = transform_glyphSize(*one_oe_glyph.value, a, b, c, d, dx, dy);
                not oneTransformRes.has_value()) {
                return std::unexpected(oneTransformRes.error());
            }
            else { res += oneTransformRes.value(); }
        }
        return res;
    }


    // Must receive refMovesOfOther which specifies how much reference glyphs
    // have/will be moved horizontally
    std::expected<std::unordered_map<std::string, HLPR_glyphByAW>, err_modifier>
    transform_glyphsByAW(json_int_t const newWidth, auto const &pred_keepSameADW) {
        if (not _jsonFont) { return std::unexpected(err_modifier::unexpectedNullptr); }

        std::unordered_map<std::string, HLPR_glyphByAW> res{};
        std::unordered_set<std::string>                 cycleChecker{};

        auto gt_ptr = JSON_Access::get(*_jsonFont, "glyf");
        if (gt_ptr == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (gt_ptr->type != json_type::json_object) { return std::unexpected(err_modifier::unexpectedJSONValueType); }
        auto const &glyfs = *gt_ptr;

        std::unordered_map<std::string, struct _json_value *> mapOfRefs;
        for (auto const &glyf : glyfs.u.object) {
            mapOfRefs.insert({std::string(glyf.name, glyf.name_length), glyf.value});
        }

        auto recSolver = [&](this auto const   &self,
                             std::string const &toSolve) -> std::expected<HLPR_glyphByAW, err_modifier> {
            if (cycleChecker.contains(toSolve)) { return std::unexpected(err_modifier::cyclicGlyfReferencesFound); }
            if (auto found = res.find(toSolve); found != res.end()) { return found->second; }

            // Get the object we are supposed to be solving
            auto ite = mapOfRefs.find(toSolve);
            if (ite == mapOfRefs.end()) { return std::unexpected(err_modifier::missingGlyphInGlyfTable); }
            auto solveObj = ite->second;
            if (solveObj == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }

            // Get advanceWidth (that must be there)
            auto adwObj = JSON_Access::get(*solveObj, "advanceWidth");
            if (adwObj == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
            if (adwObj->type != json_type::json_integer) {
                return std::unexpected(err_modifier::unexpectedJSONValueType);
            }
            bool keepSameADW = pred_keepSameADW(*solveObj);

            // Prep common fields
            json_int_t leftBearing  = std::numeric_limits<json_int_t>::max();
            json_int_t rightBearing = std::numeric_limits<json_int_t>::min();
            json_int_t moveBy       = 0;

            // Get the contours (if N/A then skip)
            auto contoursObj = JSON_Access::get(*solveObj, "contours");
            if (contoursObj != nullptr) {
                if (contoursObj->type != json_type::json_array) {
                    return std::unexpected(err_modifier::unexpectedJSONValueType);
                }
                for (auto countr : contoursObj->u.array) {
                    if (countr->type != json_type::json_array) {
                        return std::unexpected(err_modifier::unexpectedJSONValueType);
                    }
                    for (auto cp : countr->u.array) {
                        if (cp->type != json_type::json_object) {
                            return std::unexpected(err_modifier::unexpectedJSONValueType);
                        }
                        auto cp_xPos = JSON_Access::get(*cp, "x");
                        if (cp_xPos->type != json_type::json_integer) {
                            return std::unexpected(err_modifier::unexpectedJSONValueType);
                        }
                        // Update left and right bearings
                        leftBearing  = std::min(leftBearing, cp_xPos->u.integer);
                        rightBearing = std::max(rightBearing, cp_xPos->u.integer);
                    }
                }

                if (keepSameADW) { moveBy = 0; }
                else {
                    moveBy = (newWidth - adwObj->u.integer) *
                             (leftBearing / (leftBearing + (adwObj->u.integer - rightBearing)));
                }
            }

            // Get the references (if N/A then skip)
            auto refesObj = JSON_Access::get(*solveObj, "references");
            if (refesObj != nullptr) {
                if (contoursObj != nullptr) {
                    { return std::unexpected(err_modifier::glyphHasBothCountoursAndReferences); }
                }
                if (refesObj->type != json_type::json_array) {
                    return std::unexpected(err_modifier::unexpectedJSONValueType);
                }

                cycleChecker.insert(toSolve);
                std::vector<HLPR_glyphByAW> glyphHLPRs;

                for (auto const oneRef : refesObj->u.array) {
                    if (oneRef->type != json_type::json_object) {
                        return std::unexpected(err_modifier::unexpectedJSONValueType);
                    }

                    auto refName = JSON_Access::get(*oneRef, "glyph");
                    if (refName == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
                    if (refName->type != json_type::json_string) {
                        return std::unexpected(err_modifier::unexpectedJSONValueType);
                    }
                    auto refXpos = JSON_Access::get(*oneRef, "x");
                    if (refXpos == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
                    if (refXpos->type != json_type::json_integer) {
                        return std::unexpected(err_modifier::unexpectedJSONValueType);
                    }

                    // Recursive call
                    auto refGlyphHLPR = self(std::string(refName->u.string.ptr, refName->u.string.length));
                    if (not refGlyphHLPR.has_value()) { return std::unexpected(refGlyphHLPR.error()); }

                    glyphHLPRs.push_back(refGlyphHLPR.value());
                    leftBearing  = std::min(leftBearing, refGlyphHLPR.value().origLSB + refXpos->u.integer);
                    rightBearing = std::max(rightBearing, refGlyphHLPR.value().origLSB + refXpos->u.integer);
                }

                if (keepSameADW) { moveBy = 0; }
                else {
                    moveBy = (newWidth - adwObj->u.integer) *
                             (leftBearing / (leftBearing + (adwObj->u.integer - rightBearing)));
                }

                for (size_t i = 0; auto const oneRef : refesObj->u.array) {
                    auto refXpos        = JSON_Access::get(*oneRef, "x");
                    // Move the anchors for references by moveBy but exclude the move already done inside the refed
                    // glyph
                    refXpos->u.integer += (moveBy - glyphHLPRs.at(i).movedBy);
                    i++;
                }

                if (cycleChecker.erase(toSolve) != 1uz) { return std::unexpected(err_modifier::unknownError); }
            }


            // Update res;
            if (auto inserted = res.insert({toSolve, {leftBearing, moveBy}}); inserted.second == false) {
                return std::unexpected(err_modifier::unknownError);
            }
            else { return inserted.first->second; }
            std::unreachable();
        };

        // EXECUTING SOLVER
        for (auto onePair = mapOfRefs.begin(); onePair != mapOfRefs.end(); ++onePair) {
            // Exec for one glyph name
            auto solveRes = recSolver(onePair->first);
            if (not solveRes.has_value()) { return std::unexpected(solveRes.error()); }
        }

        return res;
    }


    std::expected<font_raw, err_modifier>
    exportResult(Options const &opts) {

        otfcc_Font         *font;
        otfcc_IFontBuilder *parser = otfcc_newJsonReader();
        font                       = parser->read(_jsonFont.get(), 0, opts.pimpl.get()->_opts.get());

        parser->free(parser);
        if (! font) { return std::unexpected(err_modifier::unexpectedNullptr); }

        otfcc_iFont.consolidate(font, opts.pimpl.get()->_opts.get());


        otfcc_IFontSerializer *writer = otfcc_newOTFWriter();
        caryll_Buffer         *otf    = (caryll_Buffer *)writer->serialize(font, opts.pimpl.get()->_opts.get());
        if (! otf) { return std::unexpected(err_modifier::unexpectedNullptr); }

        font_raw res(otf->size);
        std::memcpy(res.data(), otf->data, otf->size);

        return res;
    }


    // 'Doubly' private not really for use by any other class
    static std::expected<bool, err_modifier>
    _pureChange_ADW(json_value &out_glyph, double const a) {
        auto adW = JSON_Access::get(out_glyph, "advanceWidth");
        if (adW == nullptr) { return std::unexpected(err_modifier::missingJSONKey); }
        else if (adW->type != json_type::json_integer) {
            return std::unexpected(err_modifier::unexpectedJSONValueType);
        }
        adW->u.integer = static_cast<json_int_t>(round(a * adW->u.integer));
        return true;
    }
    static std::expected<bool, err_modifier>
    _pureChange_ADH(json_value &out_glyph, double const d) {
        auto adH = JSON_Access::get(out_glyph, "advanceHeight");
        if (adH != nullptr) {
            if (adH->type != json_type::json_integer) { return std::unexpected(err_modifier::unexpectedJSONValueType); }
            adH->u.integer = static_cast<json_int_t>(round(d * adH->u.integer));
            return true;
        }
        return false;
    }
    static std::expected<bool, err_modifier>
    _pureChange_VertO(json_value &out_glyph, double const d) {
        auto vertO = JSON_Access::get(out_glyph, "verticalOrigin");
        if (vertO != nullptr) {
            if (vertO->type != json_type::json_integer) {
                return std::unexpected(err_modifier::unexpectedJSONValueType);
            }
            vertO->u.integer = static_cast<json_int_t>(round(d * vertO->u.integer));
            return true;
        }
        return false;
    }

    static std::expected<bool, err_modifier>
    _pureChange_CPs(json_value &out_glyph, double const a, double const b, double const c, double const d,
                    double const dx, double const dy) {
        auto countours = JSON_Access::get(out_glyph, "contours");
        if (countours != nullptr) {
            if (countours->type != json_type::json_array) {
                return std::unexpected(err_modifier::unexpectedJSONValueType);
            }
            for (auto &contr : countours->u.array) {
                if (contr->type != json_type::json_array) {
                    return std::unexpected(err_modifier::unexpectedJSONValueType);
                }
                for (auto &contPoint : contr->u.array) {
                    if (contPoint->type != json_type::json_object) {
                        return std::unexpected(err_modifier::unexpectedJSONValueType);
                    }
                    double origX = 0.0;
                    double origY = 0.0;
                    for (size_t i = 0uz; auto &contPoint_val : contPoint->u.object) {
                        if (i == 0 && contPoint_val.value->type == json_type::json_integer) {
                            origX = contPoint_val.value->u.integer;
                        }
                        else if (i == 1 && contPoint_val.value->type == json_type::json_integer) {
                            origY = contPoint_val.value->u.integer;
                        }
                        else if (i == 2 && contPoint_val.value->type == json_type::json_boolean) {}
                        else { return std::unexpected(err_modifier::counterPointHasCorruptedStructure); }
                        i++;
                    }
                    for (size_t i = 0; auto &contPoint_val_2 : contPoint->u.object) {
                        if (i == 0 && contPoint_val_2.value->type == json_type::json_integer) {
                            contPoint_val_2.value->u.integer = static_cast<json_int_t>(a * origX + b * origY + dx);
                        }
                        else if (i == 1 && contPoint_val_2.value->type == json_type::json_integer) {
                            contPoint_val_2.value->u.integer = static_cast<json_int_t>(c * origX + d * origY + dy);
                        }
                        else if (i == 2 && contPoint_val_2.value->type == json_type::json_boolean) {}
                        else { return std::unexpected(err_modifier::counterPointHasCorruptedStructure); }
                        i++;
                    }
                }
            }
            return true;
        }
        return false;
    }

    static std::expected<bool, err_modifier>
    _pureChange_RefAnchors(json_value &out_glyph, double const a, double const b, double const c, double const d,
                           double const dx, double const dy) {
        auto references = JSON_Access::get(out_glyph, "references");
        if (references != nullptr) {
            if (references->type != json_type::json_array) {
                return std::unexpected(err_modifier::unexpectedJSONValueType);
            }
            for (auto &oneRef : references->u.array) {
                if (oneRef->type != json_type::json_object) {
                    return std::unexpected(err_modifier::unexpectedJSONValueType);
                }
                double origX = 0.0;
                double origY = 0.0;
                for (size_t i = 0; auto &oneRef_item : oneRef->u.object) {
                    if (i == 0 && oneRef_item.value->type == json_type::json_string) {}
                    else if (i == 1 && oneRef_item.value->type == json_type::json_integer) {
                        origX = oneRef_item.value->u.integer;
                    }
                    else if (i == 2 && oneRef_item.value->type == json_type::json_integer) {
                        origY = oneRef_item.value->u.integer;
                    }
                    else if (i > 2) { break; }
                    else { return std::unexpected(err_modifier::referenceHasCorruptedStructure); }
                    i++;
                }
                for (size_t i = 0; auto &oneRef_item : oneRef->u.object) {
                    if (i == 0 && oneRef_item.value->type == json_type::json_string) {}
                    else if (i == 1 && oneRef_item.value->type == json_type::json_integer) {
                        oneRef_item.value->u.integer = static_cast<json_int_t>(a * origX + b * origY + dx);
                    }
                    else if (i == 2 && oneRef_item.value->type == json_type::json_integer) {
                        oneRef_item.value->u.integer = static_cast<json_int_t>(c * origX + d * origY + dy);
                    }
                    else if (i > 2) { break; }
                    else { return std::unexpected(err_modifier::referenceHasCorruptedStructure); }
                    i++;
                }
            }
            return true;
        }
        return false;
    }


private:
    json_value_uptr _jsonFont;
};

Modifier::Modifier() : pimpl(std::make_unique<Impl>()) {}

Modifier::Modifier(std::span<const std::byte> raw_ttfFont, Options const &opts, uint32_t ttcindex)
    : pimpl(std::make_unique<Impl>(raw_ttfFont, opts, ttcindex)) {}

Modifier::~Modifier() = default;

// Changing dimensions of glyphs
std::expected<bool, err_modifier>
Modifier::change_unitsPerEm(uint32_t newEmSize) {
    auto exp_res = pimpl->transform_glyphsSize(newEmSize);
    if (not exp_res.has_value()) { return std::unexpected(exp_res.error()); }
    return true;
}
std::expected<bool, err_modifier>
Modifier::change_makeMonospaced(uint32_t targetAdvWidth) {
    auto exp_res = pimpl->transform_glyphsByAW(targetAdvWidth, Modifier::Impl::_Detail::default_ksADW);
    if (not exp_res.has_value()) { return std::unexpected(exp_res.error()); }
    return true;
}

// Filtering of font content (ie. deleting parts of the font)


// Modifications of other values and properties


// Export
std::expected<font_raw, err_modifier>
Modifier::exportResult(Options const &opts) {
    if (! pimpl) { return std::unexpected(err_modifier::unexpectedNullptr); }
    else { return pimpl->exportResult(opts); }
};


// PRIVATE METHODS

} // namespace otfccxx