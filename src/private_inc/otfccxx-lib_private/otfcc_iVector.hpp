#pragma once

#include <algorithm>
#include <concepts>
#include <type_traits>


#include <otfccxx-lib/otfccxx-lib.hpp>
#include <otfccxx-lib_private/otfcc_iElement.hpp>

#include <otfcc/otfcc_api.h>


namespace otfccxx {

namespace wrappers {
namespace detail {
// -------------------------------------------------
// C++23 Machinery
// -------------------------------------------------

// -------------------------------------------------
// Caryll Vector related
// -------------------------------------------------

namespace trait_registry {
template <typename Vec>
struct _VI_wrap; // only specialize


//  From glyf.h
template <>
struct _VI_wrap<table_glyf> {
    static constexpr auto *interfaceObjPointer = &table_iGlyf;
};
template <>
struct _VI_wrap<glyf_ReferenceList> {
    static constexpr auto *interfaceObjPointer = &glyf_iReferenceList;
};
template <>
struct _VI_wrap<glyf_MaskList> {
    static constexpr auto *interfaceObjPointer = &glyf_iMaskList;
};
template <>
struct _VI_wrap<glyf_StemDefList> {
    static constexpr auto *interfaceObjPointer = &glyf_iStemDefList;
};
template <>
struct _VI_wrap<glyf_ContourList> {
    static constexpr auto *interfaceObjPointer = &glyf_iContourList;
};
template <>
struct _VI_wrap<glyf_Contour> {
    static constexpr auto *interfaceObjPointer = &glyf_iContour;
};
template <>
struct _VI_wrap<const glyf_Contour> {
    static constexpr auto *interfaceObjPointer = &glyf_iContour;
};

template <typename T>
concept has_VI_wrap = requires {
    { trait_registry::_VI_wrap<T>::interfaceObjPointer };
};

} // namespace trait_registry

template <typename VEC, typename T>
requires trait_registry::has_VI_wrap<VEC>
struct traits_carrylVector {
    using vector_type = VEC;
    using value_type  = T;

    static constexpr auto
    iObj() {
        return trait_registry::_VI_wrap<VEC>::interfaceObjPointer;
    }
};

template <typename VEC, typename T>
concept has_caryllVectorStorage = requires(VEC v) {
    { v.length } -> std::convertible_to<std::size_t>;
    { v.capacity } -> std::convertible_to<std::size_t>;
    { v.items } -> std::same_as<std::add_lvalue_reference_t<T *>>;
};

template <typename VEC, typename T, typename Interface>
concept has_caryllVectorInterface = requires(Interface iface, VEC *vec, std::size_t n, T val, const T *ctp, void *env,
                                             bool (*pred)(const T *, void *), int (*cmp)(const T *, const T *)) {
    iface->initN(vec, n);
    iface->initCapN(vec, n);
    { iface->createN(n) } -> std::same_as<VEC *>;
    iface->fill(vec, n);
    iface->clear(vec);
    iface->push(vec, val);
    iface->shrinkToFit(vec);
    { iface->pop(vec) } -> std::same_as<T>;
    iface->disposeItem(vec, n);
    iface->filterEnv(vec, pred, env);
    iface->sort(vec, cmp);
};

template <typename Traits>
concept has_caryllVectorTraits =
    requires {
        typename Traits::vector_type;
        typename Traits::value_type;
        { Traits::iObj() };
    } && has_caryllVectorStorage<typename Traits::vector_type, typename Traits::value_type> &&
    has_caryllVectorInterface<typename Traits::vector_type, typename Traits::value_type, decltype(Traits::iObj())>;

template <typename T>
struct vector_traits_for; // only specialized for vector types

template <typename CMP, typename T>
concept is_sortComparator = requires(CMP cmp, T a, T b) {
    { cmp(a, b) } -> std::convertible_to<bool>;
};

template <has_caryllVectorTraits Traits>
class caryllVector_view {
private:
    struct _typeHLPR {
        friend class caryllVector_view;

    private:
        static consteval auto
        select_vr_type() {
            if constexpr (trait_registry::has_VI_wrap<value_type>) {
                return std::type_identity<caryllVector_view<
                    traits_carrylVector<value_type, std::remove_pointer_t<decltype(value_type::items)>>>>{};
            }
            else { return std::type_identity<value_type &>{}; }
        }
        static consteval auto
        select_iter_type() {
            if constexpr (trait_registry::has_VI_wrap<value_type>) {
                return std::type_identity<caryllVector_view<
                    traits_carrylVector<value_type, std::remove_pointer_t<decltype(value_type::items)>>>>{};
            }
            else { return std::type_identity<value_type *>{}; }
        }

        using value_reference = typename decltype(select_vr_type())::type;
        using iterator        = typename decltype(select_vr_type())::type;
    };

public:
    using vector_type = typename Traits::vector_type;
    using value_type  = typename Traits::value_type;
    using size_type   = std::size_t;

    using reference       = _typeHLPR::value_reference;
    using const_reference = const reference;

    using pointer       = value_type *;
    using const_pointer = const value_type *;

    using iterator       = value_type *;
    using const_iterator = const value_type *;

    // ─── Constructors ───────────────────────────────
    explicit caryllVector_view(vector_type &v) noexcept : vec_(&v) {}


    // -------------------------------------------------
    // Element access
    // -------------------------------------------------
    reference
    operator[](size_type pos) noexcept {
        if constexpr (trait_registry::has_VI_wrap<value_type>) { return reference{vec_->items[pos]}; }
        else { return vec_->items[pos]; }
    }

    const_reference
    operator[](size_type pos) const noexcept {
        if constexpr (trait_registry::has_VI_wrap<value_type>) { return const_reference{vec_->items[pos]}; }
        else { return vec_->items[pos]; }
    }

    reference
    at(size_type pos) {
        if (pos >= size()) { throw std::out_of_range("vector_view::at"); }
        if constexpr (trait_registry::has_VI_wrap<value_type>) { return reference{vec_->items[pos]}; }
        else { return vec_->items[pos]; }
    }

    const_reference
    at(size_type pos) const {
        if (pos >= size()) { throw std::out_of_range("vector_view::at"); }
        if constexpr (trait_registry::has_VI_wrap<value_type>) { return const_reference{vec_->items[pos]}; }
        else { return vec_->items[pos]; }
    }

    reference
    front() noexcept {
        assert(! empty());
        if constexpr (trait_registry::has_VI_wrap<value_type>) { return reference{vec_->items[0]}; }
        else { return vec_->items[0]; }
    }

    const_reference
    front() const noexcept {
        assert(! empty());
        if constexpr (trait_registry::has_VI_wrap<value_type>) { return const_reference{vec_->items[0]}; }
        else { return vec_->items[0]; }
    }

    reference
    back() noexcept {
        assert(! empty());
        if constexpr (trait_registry::has_VI_wrap<value_type>) { return reference{vec_->items[size() - 1]}; }
        else { return vec_->items[size() - 1]; }
    }

    const_reference
    back() const noexcept {
        assert(! empty());
        if constexpr (trait_registry::has_VI_wrap<value_type>) { return const_reference{vec_->item[size() - 1]}; }
        else { return vec_->items[size() - 1]; }
    }

    pointer
    data() noexcept {
        return vec_->items;
    }

    const_pointer
    data() const noexcept {
        return vec_->items;
    }

    // -------------------------------------------------
    // Range interface
    // -------------------------------------------------
    std::span<value_type>
    elements() noexcept {
        return std::span{vec_->items, vec_->length};
    }

    iterator
    begin() noexcept {
        return vec_->items;
    }

    iterator
    end() noexcept {
        return vec_->items + vec_->length;
    }

    const_iterator
    begin() const noexcept {
        return vec_->items;
    }

    const_iterator
    end() const noexcept {
        return vec_->items + vec_->length;
    }

    size_t
    size() const noexcept {
        return vec_->length;
    }

    bool
    empty() const noexcept {
        return size() == 0;
    }


    // -------------------------------------------------
    // Vector operations
    // -------------------------------------------------
    void
    push(value_type v) {
        Traits::iObj()->push(vec_, std::move(v));
    }

    value_type
    pop() {
        return Traits::iObj()->pop(vec_);
    }

    void
    clear() noexcept {
        Traits::iObj()->clear(vec_);
    }

    void
    shrink_to_fit() {
        Traits::iObj()->shrinkToFit(vec_);
    }

    template <typename PRED>
    requires std::predicate<PRED, value_type &>
    void
    erase_if(PRED &&pred) {
        auto s       = elements();
        auto it      = std::ranges::remove_if(s, pred);
        vec_->length = std::distance(s.begin(), it.begin());
    }

    // template <typename CMP>
    // requires is_sortComparator<CMP, value_type>
    // void
    // sort(CMP &&cmp) {
    //     std::ranges::sort(elements(), std::forward<CMP>(cmp));
    // }

    // template <typename Pred>
    // void
    // filter(Pred &&pred) {
    //     Traits::iObj()->filterEnv(
    //         vec_, [](const value_type *x, void *env) -> bool { return (*static_cast<Pred *>(env))(*x); }, &pred);
    // }

    // template <typename Cmp>
    // void
    // sort(Cmp &&cmp) {
    //     cmp_ = std::forward<Cmp>(cmp);
    //     Traits::iObj()->sort(vec_, &sort_trampoline);
    // }


private:
    vector_type *vec_;
};

} // namespace detail

// Caryll vector wrapper

template <typename Vec, typename T>
using CV_wrapper = detail::caryllVector_view<detail::traits_carrylVector<Vec, T>>;


} // namespace wrappers

} // namespace otfccxx
