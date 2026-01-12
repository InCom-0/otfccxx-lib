#pragma once

#include <json-builder.h>
#include <json.h>


#ifdef OTFCCXX_HARFBUZZ_BUILDFROMSOURCE
#include <hb-subset.h>
#include <hb.h>

#else
#include <harfbuzz/hb-subset.h>
#include <harfbuzz/hb.h>
#endif


#include <otfcc/otfcc_api.h>

namespace otfccxx {
using namespace std::literals;

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
struct _otfcc_Font_uptr_deleter {
    void
    operator()(otfcc_Font *f) const noexcept {
        if (f) { otfcc_iFont.free(f); }
    }
};


struct Default_FNs {
    static constexpr auto ret_onObjRemoval = [](const json_object_entry &json_oe) -> std::string {
        if (json_oe.name == nullptr) { return std::string{""}; }
        return std::string(json_oe.name, json_oe.name_length);
    };
};
} // namespace detail

using hb_face_uptr         = std::unique_ptr<hb_face_t, detail::_hb_face_uptr_deleter>;
using hb_blob_uptr         = std::unique_ptr<hb_blob_t, detail::_hb_blob_uptr_deleter>;
using hb_set_uptr          = std::unique_ptr<hb_set_t, detail::_hb_set_uptr_deleter>;
using hb_subset_input_uptr = std::unique_ptr<hb_subset_input_t, detail::_hb_subset_input_uptr_deleter>;

using json_value_uptr = std::unique_ptr<json_value, detail::_json_value_uptr_deleter>;
using otfcc_opt_uptr  = std::unique_ptr<otfcc_Options, detail::_otfcc_opt_uptr_deleter>;
using otfcc_Font_uptr = std::unique_ptr<otfcc_Font, detail::_otfcc_Font_uptr_deleter>;
} // namespace otfccxx
