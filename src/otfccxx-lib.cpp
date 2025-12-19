#include <expected>
#include <filesystem>
#include <fstream>
#include <json.h>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <nlohmann/json.hpp>
#include <otfccxx-lib/fmem_file.hpp>
#include <otfccxx-lib/otfccxx-lib.hpp>

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
} // namespace detail

using hb_face_uptr         = std::unique_ptr<hb_face_t, detail::_hb_face_uptr_deleter>;
using hb_blob_uptr         = std::unique_ptr<hb_blob_t, detail::_hb_blob_uptr_deleter>;
using hb_set_uptr          = std::unique_ptr<hb_set_t, detail::_hb_set_uptr_deleter>;
using hb_subset_input_uptr = std::unique_ptr<hb_subset_input_t, detail::_hb_subset_input_uptr_deleter>;

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
    return outs.good();
}

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

    // Must receive refMovesOfOther which specifies how much reference glyphs
    // have/will be moved horizontally
    static std::expected<bool, err_modifier>
    transform_glyphByAW(json_value &out_glyph, double const newWidth,
                        std::unordered_map<std::string, HLPR_glyphByAW> const &refMovesOfOther) {

        return std::unexpected(err_modifier::unknownError);
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
                    else { return std::unexpected(err_modifier::referenceHasCorruptedStructure); }
                    i++;
                }
            }
            return true;
        }
        return false;
    }

    std::expected<std::unordered_map<std::string, HLPR_glyphByAW>, err_modifier>
    _compute_refMoves(json_int_t const newWidth) {
        std::unordered_map<std::string, HLPR_glyphByAW> res{};
        std::unordered_set<std::string>                 cycleChecker{};

        auto gt_ptr = JSON_Access::get(_jsonFont, "glyf");
        if (gt_ptr == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (gt_ptr->type != json_type::json_object) { return std::unexpected(err_modifier::unexpectedJSONValueType); }
        auto const &glyfs = *gt_ptr;

        std::unordered_map<std::string, struct _json_value *> mapOfRefs;
        for (auto const &glyf : glyfs.u.object) {
            mapOfRefs.insert({std::string(glyf.name, glyf.name_length), glyf.value});
        }


        auto recSolver = [&](this auto const &self, std::string const &toSolve) -> std::expected<bool, err_modifier> {
            if (cycleChecker.contains(toSolve)) { return std::unexpected(err_modifier::cyclicGlyfReferencesFound); }
            else if (res.contains(toSolve)) { return false; }

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

            // Get the contours (if N/A then skip)
            auto contoursObj = JSON_Access::get(*solveObj, "contours");
            if (contoursObj != nullptr) {
                json_int_t leftBearing = std::numeric_limits<json_int_t>::max();

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
                        // Update minimum value
                        leftBearing = std::min(leftBearing, cp_xPos->u.integer);
                    }
                }

                // Update res;
            }

            // Get the references (if N/A then skip)
            auto refesObj = JSON_Access::get(*solveObj, "references");
            if (refesObj != nullptr) {
                if (contoursObj != nullptr) {
                    { return std::unexpected(err_modifier::glyphHasBothCountoursAndReferences); }
                }


                cycleChecker.insert(toSolve);

                self("next");

                if (cycleChecker.erase(toSolve) != 1uz) { return std::unexpected(err_modifier::unknownError); }


                // Update res;
            }

            return true;
        };


        auto ttt = recSolver("aa");
        if (not ttt.has_value()) { return std::unexpected(ttt.error()); }


        return res;
    }

private:
    json_value _jsonFont;
};

Modifier::Modifier() : pimpl(std::make_unique<Impl>()) {}

Modifier::Modifier(std::span<const std::byte> raw_ttfFont, otfccxx_Options const &opts) {

    otfccxx::fmem_file memfile(raw_ttfFont);

    // FILE *f = memfile.get();
}

// PRIVATE METHODS

} // namespace otfccxx