#include <concepts>
#include <cstdlib>
#include <expected>
#include <fstream>
#include <limits>
#include <ranges>
#include <unordered_map>
#include <unordered_set>
#include <utility>


#include <woff2/decode.h>
#include <woff2/encode.h>
#include <woff2/output.h>

#include <otfccxx-lib/otfccxx-lib.hpp>

#include <otfccxx-lib_private/fmem_file.hpp>
#include <otfccxx-lib_private/json_ext.hpp>
#include <otfccxx-lib_private/otfcc_enum.hpp>
#include <otfccxx-lib_private/otfcc_iVector.hpp>


namespace otfccxx {
using namespace std::literals;

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
write_bytesToFile(std::filesystem::path const &p, ByteSpan bytes) {
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


// #####################################################################
// ### Options implementaion ###
// #####################################################################

class Options::Impl {
    friend class Modifier;

public:
    Impl() : _opts(otfcc_newOptions()) {}
    Impl(uint8_t const optLevel, bool const removeTTFhints) : _opts(otfcc_newOptions()) {
        otfcc_Options_optimizeTo(_opts.get(), optLevel);
        _opts->logger = otfcc_newLogger(otfcc_newStdErrTarget());
        _opts->logger->indent(_opts->logger, "[missing]");
        _opts->decimal_cmap = true;
        _opts->ignore_hints = removeTTFhints;
    }

private:
    otfcc_opt_uptr _opts;
};


Options::Options() noexcept : pimpl(std::make_unique<Impl>()) {}
Options::Options(uint8_t const optLevel, bool const removeTTFhints) noexcept
    : pimpl(std::make_unique<Impl>(optLevel, removeTTFhints)) {}

Options::~Options() = default;


// #####################################################################
// ### Subsetter implementation ###
// #####################################################################

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
std::expected<std::vector<Bytes>, err_subset>
Subsetter::execute() {
    if (auto res = execute_bestEffort(); res.has_value()) {
        if (res.value().second.empty()) { return std::move(res.value().first); }
        else { return std::unexpected(err_subset::execute_someRequestedGlyphsAreMissing); }
    }
    else { return std::unexpected(res.error()); }
}

std::expected<std::pair<std::vector<Bytes>, std::vector<uint32_t>>, err_subset>
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
        std::vector<Bytes>(std::from_range, res | std::views::transform([](auto const &item) {
                                                unsigned int length;
                                                const char  *data = hb_blob_get_data(item.get(), &length);
                                                return Bytes(
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


// #####################################################################
// ### Modifier implementation ###
// #####################################################################

class Modifier::Impl {
    friend class Modifier;

public:
    Impl() = delete;
    // Impl(Bytes const &ttf) {}
    Impl(ByteSpan raw_ttfFont, Options const &opts, uint32_t ttcindex) {
        otfccxx::fmem_file memfile(raw_ttfFont);

        otfcc_SplineFontContainer *sfnt = otfcc_readSFNT(memfile.get());
        if (! sfnt || sfnt->count == 0) { std::exit(1); }
        if (ttcindex >= sfnt->count) { std::exit(1); }

        // Build font
        otfcc_IFontBuilder *reader = otfcc_newOTFReader();
        _font                      = otfcc_Font_uptr(reader->read(sfnt, ttcindex, opts.pimpl.get()->_opts.get()));
        if (! _font) { std::exit(1); }

        // Free no longer needed stuff
        reader->free(reader);
        if (sfnt) { otfcc_deleteSFNT(sfnt); }

        // Consolidate
        otfcc_iFont.consolidate(_font.get(), opts.pimpl.get()->_opts.get());
    }

    ~Impl() = default;

private:
    struct HLPR_glyphByAW {
        int32_t origLSB  = 0;
        int32_t movedByH = 0;
    };

    struct _Detail {
        static constexpr auto default_ksADW = [](const glyf_Glyph &glyph) -> bool {
            if (glyph.advanceWidth.kernel == 0) { return true; }
            return false;
        };
    };


    // Glyph metric modification
    static std::expected<bool, err_modifier>
    transform_glyphSize(glyf_GlyphPtr out_glyph, double const a, double const b, double const c, double const d,
                        double const dx, double const dy) {
        auto const adw = [&]() -> std::expected<bool, err_modifier> { return _pureScale_ADW(out_glyph, a); };
        auto const adh = [&](bool const) -> std::expected<bool, err_modifier> { return _pureScale_ADH(out_glyph, d); };
        auto const vertO = [&](bool const) -> std::expected<bool, err_modifier> {
            return _pureScale_VertO(out_glyph, d);
        };
        auto const cps = [&](bool const) -> std::expected<bool, err_modifier> {
            return _pureAdjust_CPs(out_glyph, a, b, c, d, dx, dy);
        };
        auto const refAnch = [&](bool const) -> std::expected<bool, err_modifier> {
            return _pureAdjust_RefAnchors(out_glyph, a, b, c, d, dx, dy);
        };

        return adw().and_then(adh).and_then(vertO).and_then(cps).and_then(refAnch);
    }

    std::expected<size_t, err_modifier>
    transform_allGlyphsSize(uint32_t newEmSize) {
        if (not _font) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (not _font->head) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (not _font->glyf) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (newEmSize < 0) { return std::unexpected(err_modifier::newEmSize_outsideValidValueRange); }

        double const a = (static_cast<double>(newEmSize) / _font->head->unitsPerEm), b = 0, c = 0, d = a, dx = 0,
                     dy         = 0;
        _font->head->unitsPerEm = newEmSize;

        auto   glyfVec = wrappers::CV_wrapper<table_glyf, glyf_GlyphPtr>(*_font->glyf);
        size_t res     = 0uz;
        for (auto one_oe_glyph : glyfVec) {
            if (auto oneTransformRes = transform_glyphSize(one_oe_glyph, a, b, c, d, dx, dy);
                not oneTransformRes.has_value()) {
                return std::unexpected(oneTransformRes.error());
            }
            else { res += oneTransformRes.value(); }
        }

        if (auto res_loc = _pureScale_AscDescLG(a); not res_loc.has_value()) {
            return std::unexpected(res_loc.error());
        }
        return res;
    }

    template <typename P>
    requires std::predicate<P, const glyf_Glyph &>
    std::expected<std::unordered_map<glyphid_t, int32_t>, err_modifier>
    transform_allGlyphsByAW(int32_t const newWidth, P const pred_keepSameADW) {

        if (not _font) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (not _font->head) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (not _font->glyf) { return std::unexpected(err_modifier::unexpectedNullptr); }

        std::unordered_map<glyphid_t, int32_t>       res{};
        std::unordered_set<glyphid_t>                cycleChecker{};
        std::unordered_map<glyphid_t, glyf_GlyphPtr> mapOfRefs{};

        auto glyfVec = wrappers::CV_wrapper<table_glyf, glyf_GlyphPtr>(*_font->glyf);

        for (size_t id = 0; auto glyf : glyfVec) { mapOfRefs.emplace(id++, glyf); }

        auto recSolver = [&](this auto const &self, glyphid_t const toSolve) -> std::expected<int32_t, err_modifier> {
            if (cycleChecker.contains(toSolve)) { return std::unexpected(err_modifier::cyclicGlyfReferencesFound); }

            // If it has been solved already, we just return the result stored in res
            if (auto found = res.find(toSolve); found != res.end()) { return found->second; }

            // Get the object we are supposed to be solving
            auto ite = mapOfRefs.find(toSolve);
            if (ite == mapOfRefs.end()) { return std::unexpected(err_modifier::missingGlyphInGlyfTable); }
            auto solveObj = ite->second;
            if (solveObj == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }

            // Get advanceWidth
            auto         &adwCur      = solveObj->advanceWidth.kernel;
            bool          keepSameADW = pred_keepSameADW(*solveObj);
            int32_t const moveBy      = keepSameADW ? 0 : (newWidth - static_cast<int32_t>(adwCur)) / 2;

            // Get the contours (if N/A then skip)
            auto countoursObj = wrappers::CV_wrapper<glyf_ContourList, glyf_Contour>(solveObj->contours);
            if (not countoursObj.empty()) {
                for (auto &countr : countoursObj) {
                    auto oneCont = wrappers::CV_wrapper<glyf_Contour, glyf_Point>(countr);
                    for (auto &oneContPoint : oneCont) {
                        // Move the countour points by moveBy
                        oneContPoint.x.kernel += moveBy;
                    }
                }
            }

            // Get the references (if N/A then skip)
            auto refesObj = wrappers::CV_wrapper<glyf_ReferenceList, glyf_ComponentReference>(solveObj->references);
            if (not refesObj.empty()) {

                cycleChecker.insert(toSolve);
                std::vector<int32_t> glyphHLPRs;

                for (auto const &oneRef : refesObj) {
                    if (oneRef.glyph.state != handle_state::HANDLE_STATE_CONSOLIDATED &&
                        oneRef.glyph.state != handle_state::HANDLE_STATE_INDEX) {
                        return std::unexpected(err_modifier::otfccHandle_notIndex);
                    }

                    // Recursive call
                    auto refGlyphHLPR = self(oneRef.glyph.index);
                    if (not refGlyphHLPR.has_value()) { return std::unexpected(refGlyphHLPR.error()); }

                    glyphHLPRs.push_back(refGlyphHLPR.value());
                }

                for (size_t i = 0; auto &oneRef : refesObj) {
                    // Move the anchors for references by moveBy but exclude the move already done inside the refed
                    // glyph
                    oneRef.x.kernel += (moveBy - glyphHLPRs.at(i));
                    i++;
                }

                if (cycleChecker.erase(toSolve) != 1uz) { return std::unexpected(err_modifier::unknownError); }
            }

            // Update the actual advance width value in the glyph
            if (not keepSameADW) { adwCur = newWidth; }

            // Update res;
            if (auto inserted = res.insert({toSolve, moveBy}); inserted.second == false) {
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

    // Other modifications
    std::expected<bool, err_modifier>
    remove_tableByTag(const uint32_t tag) {
        if (not _font) { return std::unexpected(err_modifier::unexpectedNullptr); }
        otfcc_iFont.deleteTable(_font.get(), tag);
        return true;
    }

    std::expected<bool, err_modifier>
    remove_ttfHints_all() {
        if (not _font) { return std::unexpected(err_modifier::unexpectedNullptr); }
        auto glyfsVec = wrappers::CV_wrapper<table_glyf, glyf_GlyphPtr>(*_font->glyf);

        for (auto oneGlyph : glyfsVec) {
            if (not oneGlyph) { return std::unexpected(err_modifier::unexpectedNullptr); }
            oneGlyph->instructionsLength = 0;

            // Yuck
            if (oneGlyph->instructions != nullptr) {
                if (oneGlyph->instructions != NULL) {
                    free(oneGlyph->instructions);
                    oneGlyph->instructions = NULL;
                }
            }
        }

        std::expected<bool, err_modifier> res;
        res = remove_tableByTag(std::to_underlying(otfcc_glyfTable_nameMapping::fpgm));
        res = remove_tableByTag(std::to_underlying(otfcc_glyfTable_nameMapping::prep));
        res = remove_tableByTag(std::to_underlying(otfcc_glyfTable_nameMapping::cvt));
        res = remove_tableByTag(std::to_underlying(otfcc_glyfTable_nameMapping::gasp));
        return true;
    }


    // Export
    std::expected<Bytes, err_modifier>
    exportResult(Options const &opts) {

        // 'Finalize' font for export. IE. Do the things that the underlying otfcc library doesn't do
        auto preExp_res = _preExport_finalize();
        if (not preExp_res.has_value()) { return std::unexpected(preExp_res.error()); }

        if (! _font) { return std::unexpected(err_modifier::unexpectedNullptr); }

        otfcc_iFont.consolidate(_font.get(), opts.pimpl.get()->_opts.get());

        otfcc_IFontSerializer *writer = otfcc_newOTFWriter();
        caryll_Buffer         *otf    = (caryll_Buffer *)writer->serialize(_font.get(), opts.pimpl.get()->_opts.get());
        if (! otf) { return std::unexpected(err_modifier::unexpectedNullptr); }

        Bytes res(otf->size);
        std::memcpy(res.data(), otf->data, otf->size);

        return res;
    }


    // 'Doubly' private not really for use by any other class
    static std::expected<bool, err_modifier>
    _pureScale_ADW(glyf_GlyphPtr out_glyph, double const a) {
        if (out_glyph == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
        iVQ.inplaceScale(&out_glyph->advanceWidth, a);
        return true;
    }
    static std::expected<bool, err_modifier>
    _pureScale_ADH(glyf_GlyphPtr out_glyph, double const d) {
        if (out_glyph == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
        iVQ.inplaceScale(&out_glyph->advanceHeight, d);
        return true;
    }
    static std::expected<bool, err_modifier>
    _pureScale_VertO(glyf_GlyphPtr out_glyph, double const d) {
        if (out_glyph == nullptr) { return std::unexpected(err_modifier::unexpectedNullptr); }
        iVQ.inplaceScale(&out_glyph->verticalOrigin, d);
        return true;
    }
    static std::expected<bool, err_modifier>
    _pureAdjust_CPs(glyf_GlyphPtr out_glyph, double const a, double const b, double const c, double const d,
                    double const dx, double const dy) {
        auto contours = wrappers::CV_wrapper<glyf_ContourList, glyf_Contour>(out_glyph->contours);

        for (auto oneContr : contours | std::views::transform([](auto &&item) {
                                 return wrappers::CV_wrapper<glyf_Contour, glyf_Point>(item);
                             })) {
            for (auto &contPoint : oneContr) {
                double const origX = contPoint.x.kernel;
                double const origY = contPoint.y.kernel;
                contPoint.x.kernel = (a * origX + b * origY + dx);
                contPoint.y.kernel = (c * origX + d * origY + dy);
            }
        }
        return true;
    }
    static std::expected<bool, err_modifier>
    _pureAdjust_RefAnchors(glyf_GlyphPtr out_glyph, double const a, double const b, double const c, double const d,
                           double const dx, double const dy) {

        auto referencedGlyphs =
            wrappers::CV_wrapper<glyf_ReferenceList, glyf_ComponentReference>(out_glyph->references);

        for (auto &oneRef : referencedGlyphs) {
            double const origX = oneRef.x.kernel;
            double const origY = oneRef.y.kernel;
            oneRef.x.kernel    = (a * origX + b * origY + dx);
            oneRef.y.kernel    = (c * origX + d * origY + dy);
        }
        return true;
    }

    std::expected<bool, err_modifier>
    _pureScale_AscDescLG(double const multiplier) {

        if (not _font->hhea) { return std::unexpected(err_modifier::unexpectedNullptr); }
        if (not _font->OS_2) { return std::unexpected(err_modifier::unexpectedNullptr); }

        auto adjustOne_v2 = [&](auto &out_toAdjust) -> void { out_toAdjust *= multiplier; };

        adjustOne_v2(_font->hhea->ascender);
        adjustOne_v2(_font->hhea->descender);
        adjustOne_v2(_font->hhea->lineGap);

        adjustOne_v2(_font->OS_2->sTypoAscender);
        adjustOne_v2(_font->OS_2->sTypoDescender);
        adjustOne_v2(_font->OS_2->sTypoLineGap);
        adjustOne_v2(_font->OS_2->usWinAscent);
        adjustOne_v2(_font->OS_2->usWinDescent);

        return true;
    }


    // MANDATORY TO IMPLEMENT BEFORE RELEASE
    std::expected<size_t, err_modifier>
    _preExport_finalize() {
        return 0uz;
    }


private:
    otfcc_Font_uptr _font;
};


Modifier::Modifier(ByteSpan raw_ttfFont, uint32_t ttcindex, Options const &opts)
    : pimpl(std::make_unique<Impl>(raw_ttfFont, opts, ttcindex)) {}

Modifier::~Modifier() = default;

// Changing dimensions of glyphs
std::expected<bool, err_modifier>
Modifier::change_unitsPerEm(uint32_t newEmSize) {
    auto exp_res = pimpl->transform_allGlyphsSize(newEmSize);
    if (not exp_res.has_value()) { return std::unexpected(exp_res.error()); }
    return true;
}

std::expected<bool, err_modifier>
Modifier::change_makeMonospaced(uint32_t const targetAdvWidth) {
    auto exp_res = pimpl->transform_allGlyphsByAW(targetAdvWidth, Modifier::Impl::_Detail::default_ksADW);
    if (not exp_res.has_value()) { return std::unexpected(exp_res.error()); }
    return true;
}
std::expected<bool, err_modifier>
Modifier::change_makeMonospaced_byEmRatio(double const emRatio) {
    if (emRatio > 2.0) { return std::unexpected(err_modifier::ratioAdvWidthToEmSize_cannotBeOver2); }
    if (emRatio < 0.0) { return std::unexpected(err_modifier::ratioAdvWidthToEmSize_cannotBeNegative); }

    if (not pimpl->_font) { return std::unexpected(err_modifier::unexpectedNullptr); }
    if (not pimpl->_font->head) { return std::unexpected(err_modifier::unexpectedNullptr); }

    return change_makeMonospaced(static_cast<uint32_t>(pimpl->_font->head->unitsPerEm * emRatio));
}

// Filtering of font content (ie. deleting parts of the font)


// Modifications of other values and properties

// THIS FUNCTION IS FAKE
std::expected<bool, err_modifier>
Modifier::remove_ttfHints() {
    return pimpl->remove_ttfHints_all();
}

// Export
std::expected<Bytes, err_modifier>
Modifier::exportResult(Options const &opts) {
    if (! pimpl) { return std::unexpected(err_modifier::unexpectedNullptr); }
    else { return pimpl->exportResult(opts); }
}


// #####################################################################
// ### Converter implementation ###
// #####################################################################

size_t
Converter::max_compressed_size(ByteSpan data) {
    return woff2::MaxWOFF2CompressedSize(reinterpret_cast<const uint8_t *>(data.data()), data.size());
}

size_t
Converter::max_compressed_size(ByteSpan data, const std::string &extended_metadata) {
    return woff2::MaxWOFF2CompressedSize(reinterpret_cast<const uint8_t *>(data.data()), data.size(),
                                         extended_metadata);
}


std::expected<Bytes, err_converter>
Converter::encode_Woff2(ByteSpan ttf) {
    size_t max_size = max_compressed_size(ttf);
    Bytes  output(max_size);

    size_t actual_size = max_size;
    bool   ok          = woff2::ConvertTTFToWOFF2(reinterpret_cast<const uint8_t *>(ttf.data()), ttf.size(),
                                                  reinterpret_cast<uint8_t *>(output.data()), &actual_size);
    if (! ok) { return std::unexpected(err_converter::unknownError); }

    output.resize(actual_size);
    return output;
}

std::expected<Bytes, err_converter>
Converter::decode_Woff2(ByteSpan ttf) {
    const size_t final_size = woff2::ComputeWOFF2FinalSize(reinterpret_cast<const uint8_t *>(ttf.data()), ttf.size());
    if (final_size == 0) { return std::unexpected(err_converter::woff2_dataInvalid); }

    Bytes                 output(final_size);
    woff2::WOFF2MemoryOut out(reinterpret_cast<uint8_t *>(output.data()), output.size());

    const bool ok = ConvertWOFF2ToTTF(reinterpret_cast<const uint8_t *>(ttf.data()), ttf.size(), &out);
    if (! ok) { return std::unexpected(err_converter::woff2_decompressionFailed); }

    output.resize(out.Size());
    return output;
}


} // namespace otfccxx