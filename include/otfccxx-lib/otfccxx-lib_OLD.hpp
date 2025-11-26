#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <otfcc/font.h>
#include <otfcc/options.h>
#include <otfcc/primitives.h>
#include <otfcc/table/cmap.h>
#include <otfcc/table/glyf.h>
#include <otfcc/vf/vq.h>
#include <ranges>
#include <sds.h>
#include <span>
#include <utility>
#include <vector>

namespace otfccxx_OLD {
enum class err : int {
    noError = 0,
    uknownError,
    attemptToAccessNullptr,
    someRequiredCodePointsNotPresent
};

namespace detail {
// Caryll vector. Namespace providing modernish C++ interface to caryll vector based types
namespace cv {
inline auto get_rngInterface(auto &caryll_vector_t) {
    return std::span<decltype(caryll_vector_t.items)>(&caryll_vector_t.items, caryll_vector_t.length);
};
} // namespace cv


template <typename T>
struct lambda_traits : lambda_traits<decltype(&T::operator())> {};

// For non-const operator()
template <typename C, typename R, typename... Args>
struct lambda_traits<R (C::*)(Args...)> {
    using result_type = R;
    using arg_types   = std::tuple<Args...>;
};

// For const operator()
template <typename C, typename R, typename... Args>
struct lambda_traits<R (C::*)(Args...) const> {
    using result_type = R;
    using arg_types   = std::tuple<Args...>;
};

// --- Wrapper storing lambda and producing callback + env ---

template <typename LAM, typename T>
struct CCallbackWrapper {
    LAM lambda;

    static bool thunk(const T *a, void *env) {
        auto *self = static_cast<CCallbackWrapper *>(env);
        return self->lambda(a);
    }
};

// Factory returning {thunk, env}
template <typename LAM>
auto make_cLike_unaryPredicate(LAM &&lambda) {
    using traits  = lambda_traits<std::decay_t<LAM>>;
    using T       = std::tuple_element_t<0, typename traits::arg_types>;
    using Pointee = std::remove_pointer_t<T>;
    using Wrapper = CCallbackWrapper<std::decay_t<LAM>, Pointee>;

    // Ensure lambda taken only one argument
    static_assert(std::tuple_size_v<typename traits::arg_types> == 1,
                  "Lambda must unary predicate lambda must take one and only one argument");

    // Ensure lambda looks like bool(const T*)
    static_assert(std::is_pointer_v<T> || std::is_const_v<std::remove_pointer_t<T>>,
                  "Lambda must take (const T*) as first argument");


    struct cLike_unaryPred {
        bool    (*lamCallOp_fn)(const Pointee *, void *);
        void   *captured;
        Wrapper storage;
    };

    cLike_unaryPred r{&Wrapper::thunk, nullptr, {std::forward<LAM>(lambda)}};
    r.captured = &r.storage;
    return r;
}

} // namespace detail

namespace glyph {
inline void Transform(glyf_Glyph &glyph, double a, double b, double c, double d, double dx, double dy) {

    auto adj_VQstill = [](VQ &VQ_out, auto const &func) {
        func(VQ_out.kernel);
        for (size_t j = 0; j < VQ_out.shift.length; j++) {
            if (VQ_out.shift.items[j].type == VQ_STILL) { func(VQ_out.shift.items[j].val.still); }
        }
    };
    auto multStill = [&](pos_t &out_still) { out_still = std::round(a * out_still); };
    adj_VQstill(glyph.advanceWidth, multStill);
    adj_VQstill(glyph.advanceHeight, multStill);

    auto transform_porr = [&](auto &out_porr) {
        auto &one_gp_ref = (*out_porr);
        VQ    xCpy       = iVQ.dup(one_gp_ref.x);

        one_gp_ref.x.kernel = static_cast<int>(a * xCpy.kernel + b * one_gp_ref.y.kernel + dx);
        for (size_t j = 0; j < one_gp_ref.x.shift.length; j++) {
            if (one_gp_ref.x.shift.items[j].type == VQ_STILL) {
                one_gp_ref.x.shift.items[j].val.still = static_cast<int>(
                    a * xCpy.shift.items[j].val.still + b * one_gp_ref.y.shift.items[j].val.still + dx);
            }
        }

        one_gp_ref.y.kernel = static_cast<int>(c * xCpy.kernel + d * one_gp_ref.y.kernel + dy);
        for (size_t j = 0; j < one_gp_ref.y.shift.length; j++) {
            if (one_gp_ref.y.shift.items[j].type == VQ_STILL) {
                one_gp_ref.y.shift.items[j].val.still = static_cast<int>(
                    c * xCpy.shift.items[j].val.still + d * one_gp_ref.y.shift.items[j].val.still + dy);
            }
        }
    };

    for (auto one_cont : detail::cv::get_rngInterface(glyph.contours)) {
        for (auto one_gp : detail::cv::get_rngInterface((*one_cont))) { transform_porr(one_gp); }
    }
    for (auto &one_refer : detail::cv::get_rngInterface(glyph.references)) { transform_porr(one_refer); }
}
} // namespace glyph


class Font {
    friend class FontMerger;

public:
    explicit Font() : handle_{otfcc_iFont.create()} {
        // if (! handle_) { throw std::runtime_error("Failed to create CThing"); }
    }
    explicit Font(otfcc_SplineFontContainer &sfnt, uint32_t index, otfcc_Options const &options)
        : handle_(Ctor_(sfnt, index, options)) {}

    // Non-copyable, but movable
    Font(Font &&) noexcept            = default;
    Font &operator=(Font &&) noexcept = default;

    Font copy_deep() const {
        Font res = Font();
        if (handle_) {
            otfcc_Font *CpyPtr;
            otfcc_iFont.copy(CpyPtr, handle_.get());
            res.handle_.reset(CpyPtr);
        }
        return res;
    }

    // Maintenance
    void consolidate(uint8_t optimize_level = 0) const {
        auto options = otfcc_newOptions();
        otfcc_Options_optimizeTo(options, optimize_level);
        otfcc_iFont.consolidate(handle_.get(), options);
        otfcc_deleteOptions(options);
    }


    // Table modifications
    bool add_toCmap(int codePoint, sds const name) {
        sds nameCpy = sdsdup(name);
        return table_iCmap.encodeByName(handle_->cmap, codePoint, nameCpy);
    }


    // Filtering
    std::optional<err> filter_cmap(auto const &&filterFunc) {
        if (not handle_->cmap) { return std::nullopt; }
        auto &cmapRef = *(handle_->cmap);

        std::vector<int> ids_toUnmap;
        for (cmap_Entry const *item = cmapRef.unicodes; item != nullptr; item = (cmap_Entry *)item->hh.next) {
            cmap_Entry const &ref = *item;
            if (not filterFunc(ref)) { ids_toUnmap.push_back(ref.unicode); }
        }

        for (auto const toUnmap : ids_toUnmap) { table_iCmap.unmap(handle_->cmap, toUnmap); }
        return std::nullopt;
    }

    std::expected<std::vector<int>, err> filter_glyphs_inPlace(std::vector<int> const &charCodes_toKeep) const {
        if (handle_->cmap == nullptr) { return std::unexpected(err::attemptToAccessNullptr); }
        auto &cmapRef = *(handle_->cmap);

        std::vector<sds> namesToKeep;
        std::vector<int> toKeep_butMissing;
        for (cmap_Entry const *item = cmapRef.unicodes; item != nullptr; item = (cmap_Entry *)item->hh.next) {
            if (std::ranges::find(charCodes_toKeep, item->unicode) != charCodes_toKeep.end()) {
                namesToKeep.push_back(sdsdup(item->glyph.name));
            }
            else { toKeep_butMissing.push_back(item->unicode); }
        }

        auto filt = [&](glyf_Glyph *const *glyph_ptr) -> bool {
            if (std::ranges::find(namesToKeep, (*glyph_ptr)->name) != namesToKeep.end()) { return true; }
            return false;
        };

        auto cLike_ff = detail::make_cLike_unaryPredicate(filt);
        table_iGlyf.filterEnv(handle_->glyf, cLike_ff.lamCallOp_fn, cLike_ff.captured);

        consolidate(0);
        return toKeep_butMissing;
    }

    std::optional<err> filter_glyphs_inPlace(auto const &&filterFunc, otfcc_Options const &options) const {
        if (handle_->glyf == nullptr) { return err::attemptToAccessNullptr; }

        auto cLike_ff = detail::make_cLike_unaryPredicate(filterFunc);
        table_iGlyf.filterEnv(handle_->glyf, cLike_ff.lamCallOp_fn, cLike_ff.captured);

        return std::nullopt;
    }


private:
    static otfcc_Font *Ctor_(otfcc_SplineFontContainer const &sfnt, uint32_t index, otfcc_Options const &options) {
        otfcc_IFontBuilder *reader = otfcc_newOTFReader();
        auto                res = reader->read((otfcc_SplineFontContainer *)(&sfnt), index, (otfcc_Options *)&options);
        reader->free(reader);
        return res;
    }

    struct Deleter {
        void operator()(otfcc_Font *ptr) const noexcept { otfcc_iFont.free(ptr); }
    };

    std::unique_ptr<otfcc_Font, Deleter> handle_;
};


class FontMerger {
public:
    static std::optional<err> merge_intoBase_inPlace(Font &out_base, std::vector<Font> &out_toMerge) {
        FontMerger fm(out_base, out_toMerge);
        if (auto errOpt = fm._merge(); errOpt.has_value()) { return errOpt; }
        else { return std::nullopt; }
        std::unreachable();
    }
    static std::expected<Font, err> merge_intoBase(Font const &out_base, std::vector<Font> const &toMerge) {
        // Make copies, because we are not doing it inPlace
        Font base_res = out_base.copy_deep();

        std::vector<Font> toMerge_cpy;
        for (auto const &mrg_font : toMerge) { toMerge_cpy.push_back(mrg_font.copy_deep()); }
        FontMerger fm(base_res, toMerge_cpy);

        if (auto errOpt = fm._merge(); errOpt.has_value()) { return std::unexpected(errOpt.value()); }
        else { return base_res; }
        std::unreachable();
    }


    static std::optional<err> merge_intoBase_inPlace(Font &out_base, std::vector<Font> &out_toMerge,
                                                     std::vector<int> const &codePointsToKeep) {
        FontMerger fm(out_base, out_toMerge);
        if (auto exp = fm._merge(codePointsToKeep); not exp.has_value()) { return exp.error(); }
        else { return std::nullopt; }
        std::unreachable();
    }
    static std::expected<Font, err> merge_intoBase(Font const &out_base, std::vector<Font> const &toMerge,
                                                   std::vector<int> const &codePointsToKeep) {
        // Make copies, because we are not doing it inPlace
        Font base_res = out_base.copy_deep();

        std::vector<Font> toMerge_cpy;
        for (auto const &mrg_font : toMerge) { toMerge_cpy.push_back(mrg_font.copy_deep()); }
        FontMerger fm(base_res, toMerge_cpy);

        if (auto exp = fm._merge(codePointsToKeep); not exp.has_value()) { return std::unexpected(exp.error()); }
        else { return base_res; }
        std::unreachable();
    }

private:
    std::optional<err> _merge() {


        _consolidate_fonts();
        return err::noError;
    }

    std::expected<std::vector<size_t>, err> _merge(std::vector<int> const &numsOfCodePointsToKeep) {
        auto res = _preMerge_filter(numsOfCodePointsToKeep);
        if (res.has_value()) {
            for (size_t id_otherFonts = 0; auto const &numOfCP : std::ranges::views::drop(res.value(), 1)) {
                if (numOfCP == 0) { break; }

                for (cmap_Entry const *item = others.at(id_otherFonts).handle_->cmap->unicodes; item != nullptr;
                     item                   = (cmap_Entry *)item->hh.next) {
                    base.add_toCmap(item->unicode, item->glyph.name);
                }
                // table_iCmap.encodeByName(base.,0, "aa")
            }
        }


        return res;
    }


    std::expected<std::vector<size_t>, err> _preMerge_filter(std::vector<int> const &codePointsToKeep) {
        // res is how many glyphs will be used from each font
        std::vector<size_t> res(others.size() + 1, 0);
        res.front() = codePointsToKeep.size();

        auto cptk_fromOthers = base.filter_glyphs_inPlace(codePointsToKeep);
        if (not cptk_fromOthers.has_value()) { return std::unexpected(cptk_fromOthers.error()); }
        res.front() -= cptk_fromOthers.value().size();

        for (size_t id = 1; auto &oneOF : others) {
            res.at(id) = cptk_fromOthers.value().size();

            cptk_fromOthers = oneOF.filter_glyphs_inPlace(cptk_fromOthers.value());
            if (not cptk_fromOthers.has_value()) { return std::unexpected(cptk_fromOthers.error()); }
            res.at(id) -= cptk_fromOthers.value().size();

            if (cptk_fromOthers.value().size() == 0) { return res; }
        }

        return std::unexpected(err::someRequiredCodePointsNotPresent);
    }


    // Prep
    void _consolidate_fonts() const {
        base.consolidate();
        for (auto const &oneFont : others) { oneFont.consolidate(); }
    }

    FontMerger(Font &base, std::vector<Font> &toMerge) : base{base}, others(toMerge) {}

    Font              &base;
    std::vector<Font> &others;
};


} // namespace otfccxx