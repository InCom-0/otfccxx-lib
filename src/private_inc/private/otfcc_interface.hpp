#pragma once

#include <algorithm>
#include <concepts>
#include <functional>
#include <ranges>


#include <otfccxx-lib/fmem_file.hpp>
#include <otfccxx-lib/otfccxx-lib.hpp>

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

template <typename T>
concept has_VI_wrap = requires { typename trait_registry::_VI_wrap<T>; };

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
    { v.items } -> std::same_as<T *>;
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
public:
    using vector_type = typename Traits::vector_type;
    using value_type  = typename Traits::value_type;
    using size_type   = std::size_t;

    using reference =
        std::conditional_t<trait_registry::has_VI_wrap<value_type>,
                           caryllVector_view<traits_carrylVector<value_type, typename value_type::value_type>>,
                           value_type &>;
    using const_reference =
        std::conditional_t<trait_registry::has_VI_wrap<value_type>,
                           caryllVector_view<traits_carrylVector<const value_type, typename value_type::value_type>>,
                           const value_type &>;

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


// -------------------------------------------------
// Caryll Element related
// -------------------------------------------------
template <typename T, auto *Interface>
struct traits_caryllElement {
    using value_type = T;

    static constexpr auto *
    iFP() noexcept {
        return Interface;
    }
};

template <typename Traits>
concept has_caryllElementTraits = requires {
    typename Traits::value_type;
    { Traits::iObj() };
};

template <typename Traits>
concept caryll_T_traits =
    has_caryllElementTraits<Traits> &&
    requires(typename Traits::value_type *p, const typename Traits::value_type *cp, typename Traits::value_type v) {
        typename Traits::value_type;
        { Traits::iObj() };
        Traits::iObj()->init(p);
        Traits::iObj()->copy(p, cp);
        Traits::iObj()->move(p, p);
        Traits::iObj()->dispose(p);
        Traits::iObj()->replace(p, v);
        Traits::iObj()->copyReplace(p, v);
    };

template <typename Traits>
concept caryll_VT_traits = caryll_T_traits<Traits> && requires(const typename Traits::value_type v) {
    { Traits::iObj()->empty() } -> std::same_as<typename Traits::value_type>;

    { Traits::iObj()->dup(v) } -> std::same_as<typename Traits::value_type>;
};

template <typename Traits>
concept caryll_RT_traits = caryll_T_traits<Traits> && requires {
    { Traits::iObj()->create() } -> std::same_as<typename Traits::value_type *>;

    Traits::iObj()->free(std::declval<typename Traits::value_type *>());
};


template <caryll_VT_traits Traits>
class caryll_element {
public:
    using value_type = typename Traits::value_type;

    // ----------------------------
    // Construction / destruction
    // ----------------------------

    caryll_element() { Traits::iObj()->init(&value_); }

    ~caryll_element() { Traits::iObj()->dispose(&value_); }

    // ----------------------------
    // Copy semantics
    // ----------------------------

    caryll_element(const caryll_element &other) { Traits::iObj()->copy(&value_, &other.value_); }

    caryll_element &
    operator=(const caryll_element &other) {
        if (this != &other) { Traits::iObj()->copyReplace(&value_, other.value_); }
        return *this;
    }

    // ----------------------------
    // Move semantics
    // ----------------------------

    caryll_element(caryll_element &&other) noexcept { Traits::iObj()->move(&value_, &other.value_); }

    caryll_element &
    operator=(caryll_element &&other) noexcept {
        if (this != &other) { Traits::iObj()->replace(&value_, std::move(other.value_)); }
        return *this;
    }

    // ----------------------------
    // Access
    // ----------------------------

    value_type *
    get() noexcept {
        return &value_;
    }
    const value_type *
    get() const noexcept {
        return &value_;
    }

    value_type &
    operator*() noexcept {
        return value_;
    }
    const value_type &
    operator*() const noexcept {
        return value_;
    }

    value_type *
    operator->() noexcept {
        return &value_;
    }
    const value_type *
    operator->() const noexcept {
        return &value_;
    }

private:
    value_type value_;
};

template <caryll_RT_traits Traits>
class caryll_owned_element {
public:
    using value_type = typename Traits::value_type;

    caryll_owned_element() : ptr_(Traits::iObj()->create()) {}

    ~caryll_owned_element() {
        if (ptr_) { Traits::iObj()->free(ptr_); }
    }

    caryll_owned_element(const caryll_owned_element &) = delete;
    caryll_owned_element &
    operator=(const caryll_owned_element &) = delete;

    caryll_owned_element(caryll_owned_element &&other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}

    caryll_owned_element &
    operator=(caryll_owned_element &&other) noexcept {
        if (this != &other) {
            reset();
            ptr_ = std::exchange(other.ptr_, nullptr);
        }
        return *this;
    }

    value_type *
    get() noexcept {
        return ptr_;
    }
    const value_type *
    get() const noexcept {
        return ptr_;
    }

    value_type &
    operator*() noexcept {
        return *ptr_;
    }
    const value_type &
    operator*() const noexcept {
        return *ptr_;
    }

    value_type *
    operator->() noexcept {
        return ptr_;
    }
    const value_type *
    operator->() const noexcept {
        return ptr_;
    }

private:
    void
    reset() {
        if (ptr_) {
            Traits::iObj()->free(ptr_);
            ptr_ = nullptr;
        }
    }

    value_type *ptr_ = nullptr;
};


template <caryll_T_traits Traits>
class caryll_element_ref {
public:
    using value_type = typename Traits::value_type;

    explicit caryll_element_ref(value_type *ptr) noexcept : ptr_(ptr) {}

    // ---------------------------------
    // Core access
    // ---------------------------------

    value_type *
    get() noexcept {
        return ptr_;
    }
    const value_type *
    get() const noexcept {
        return ptr_;
    }

    value_type &
    raw() noexcept {
        return *ptr_;
    }
    const value_type &
    raw() const noexcept {
        return *ptr_;
    }

    // ---------------------------------
    // Caryll-style operations
    // ---------------------------------

    void
    copy_from(const value_type &other) {
        Traits::iObj()->copy(ptr_, &other);
    }

    void
    move_from(value_type &other) {
        Traits::iObj()->move(ptr_, &other);
    }

    void
    replace(value_type &&other) {
        Traits::iObj()->replace(ptr_, std::move(other));
    }

    void
    dispose() {
        Traits::iObj()->dispose(ptr_);
    }

    // ---------------------------------
    // Implicit raw access (important)
    // ---------------------------------

    operator value_type &() noexcept { return *ptr_; }
    operator const value_type &() const noexcept { return *ptr_; }

private:
    value_type *ptr_;
};

} // namespace detail

// Caryll vector wrapper

template <typename Vec, typename T>
using CV_wrapper = detail::caryllVector_view<detail::traits_carrylVector<Vec, T>>;


template <typename CEI>
class CE_wrapper {};
} // namespace wrappers

class iFont {


    static std::expected<bool, err_modifier>
    aaa(otfcc_Font *font);
};
} // namespace otfccxx
