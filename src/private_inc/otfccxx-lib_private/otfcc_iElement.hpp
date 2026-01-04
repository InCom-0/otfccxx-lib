#pragma once

#include <algorithm>
#include <concepts>


#include <otfccxx-lib/otfccxx-lib.hpp>

#include <otfcc/otfcc_api.h>


namespace otfccxx {

namespace wrappers {
namespace detail {

// -------------------------------------------------
// C++23 Machinery
// -------------------------------------------------


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


template <typename CEI>
class CE_wrapper {};
} // namespace wrappers

} // namespace otfccxx
