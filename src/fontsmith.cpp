#pragma once

#include <expected>
#include <filesystem>
#include <fstream>


#include <otfccxx-lib/fontsmith.hpp>
#include <system_error>
#include <utility>


namespace fontsmith {
struct AccessInfo {
    bool readable;
    bool writable;
};
inline std::expected<AccessInfo, std::filesystem::file_type> check_access(const std::filesystem::path &p) {
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

std::expected<bool, std::filesystem::file_type> write_bytesToFile(std::filesystem::path const &p,
                                                                         std::span<const std::byte>   bytes) {
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


// Adding FontFaces
Subsetter &Subsetter::add_ff_toSubset(std::span<const char> buf, unsigned int const faceIndex) {
    if (auto toInsert = make_ff(buf, faceIndex); toInsert.has_value()) {
        ffs_toSubset.push_back(std::move(toInsert.value()));
    }
    return *this;
}
Subsetter &Subsetter::add_ff_categoryBackup(std::span<const char> buf, unsigned int const faceIndex) {
    if (auto toInsert = make_ff(buf, faceIndex); toInsert.has_value()) {
        ffs_categoryBackup.push_back(std::move(toInsert.value()));
    }
    return *this;
}
Subsetter &Subsetter::add_ff_lastResort(std::span<const char> buf, unsigned int const faceIndex) {
    if (auto toInsert = make_ff(buf, faceIndex); toInsert.has_value()) {
        ffs_lastResort.push_back(std::move(toInsert.value()));
    }
    return *this;
}


Subsetter &Subsetter::add_ff_toSubset(std::filesystem::path const &pth, unsigned int const faceIndex) {

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
    return *this;
}
Subsetter &Subsetter::add_ff_categoryBackup(std::filesystem::path const &pth, unsigned int const faceIndex) {
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
    return *this;
}
Subsetter &Subsetter::add_ff_lastResort(std::filesystem::path const &pth, unsigned int const faceIndex) {
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
    return *this;
}


Subsetter &Subsetter::add_ff_toSubset(hb_face_t *ptr, unsigned int const faceIndex) {
    if (ptr) { ffs_toSubset.push_back(hb_face_uptr(ptr)); }
    return *this;
}
Subsetter &Subsetter::add_ff_categoryBackup(hb_face_t *ptr, unsigned int const faceIndex) {
    if (ptr) { ffs_categoryBackup.push_back(hb_face_uptr(ptr)); }
    return *this;
}
Subsetter &Subsetter::add_ff_lastResort(hb_face_t *ptr, unsigned int const faceIndex) {
    if (ptr) { ffs_lastResort.push_back(hb_face_uptr(ptr)); }
    return *this;
}


// Adding unicode character points and/or glyph IDs

Subsetter &Subsetter::add_toKeep_CP(hb_codepoint_t const cp) {
    hb_set_add(toKeep_unicodeCPs.get(), cp);
    return *this;
}

Subsetter &Subsetter::add_toKeep_CPs(std::span<const hb_codepoint_t> const cps) {
    for (auto const &cp : cps) { hb_set_add(toKeep_unicodeCPs.get(), cp); }
    return *this;
}


// Execution

std::expected<std::vector<hb_face_uptr>, err> Subsetter::execute() {
    if (auto res = execute_bestEffort(); res.has_value()) {
        if (hb_set_is_empty(res.value().second.get())) { return std::move(res.value().first); }
        else { return std::unexpected(err::execute_someRequestedGlyphsAreMissing); }
    }
    else { return std::unexpected(res.error()); }
}


std::expected<std::pair<std::vector<hb_face_uptr>, hb_set_uptr>, err> Subsetter::execute_bestEffort() {
    std::vector<hb_face_uptr> res;

    for (auto &ff_to : ffs_toSubset) {
        if (hb_set_is_empty(toKeep_unicodeCPs.get())) { goto RET; }

        auto exp_ff = make_subset(ff_to.get());
        if (not exp_ff.has_value()) {
            if (exp_ff.error() == err::make_subset_noIntersectingGlyphs) { continue; }
            else { return std::unexpected(exp_ff.error()); }
        }
        else { res.push_back(std::move(exp_ff.value())); }
    }

    for (auto &ff_to : ffs_categoryBackup) {
        if (hb_set_is_empty(toKeep_unicodeCPs.get())) { goto RET; }

        auto exp_ff = should_include_category(ff_to.get());
        if (not exp_ff.has_value()) {
            if (exp_ff.error() == err::make_subset_noIntersectingGlyphs) { continue; }
            else { return std::unexpected(exp_ff.error()); }
        }
        else { res.push_back(hb_face_uptr(hb_face_reference(ff_to.get()))); }
    }

    for (auto &ff_to : ffs_lastResort) {
        if (hb_set_is_empty(toKeep_unicodeCPs.get())) { goto RET; }

        auto exp_ff = make_subset(ff_to.get());
        if (not exp_ff.has_value()) {
            if (exp_ff.error() == err::make_subset_noIntersectingGlyphs) { continue; }
            else { return std::unexpected(exp_ff.error()); }
        }
        else { res.push_back(std::move(exp_ff.value())); }
    }


RET:
    return std::make_pair(std::move(res), hb_set_uptr(hb_set_copy(toKeep_unicodeCPs.get())));
}


// PRIVATE METHODS
std::expected<hb_face_uptr, err> Subsetter::make_ff(std::span<const char> const &buf, unsigned int const faceIndex) {
    hb_blob_uptr blob(hb_blob_create_or_fail(buf.data(), buf.size_bytes(), HB_MEMORY_MODE_DUPLICATE, nullptr, nullptr));
    if (! blob) { return std::unexpected(err::hb_blob_t_createFailure); }

    // Create face from blob
    hb_face_uptr face(hb_face_create_or_fail(blob.get(), faceIndex));
    if (! face) { return std::unexpected(err::hb_face_t_createFailure); }

    return face;
}


std::expected<hb_face_uptr, err> Subsetter::make_subset(hb_face_t *ff) {
    hb_set_uptr unicodes_toKeep_in_ff(hb_set_create());
    hb_face_collect_unicodes(ff, unicodes_toKeep_in_ff.get());

    hb_set_intersect(unicodes_toKeep_in_ff.get(), toKeep_unicodeCPs.get());
    if (hb_set_is_empty(unicodes_toKeep_in_ff.get())) { return std::unexpected(err::make_subset_noIntersectingGlyphs); }

    // Set the unicodes to keep in the subsetted font
    hb_subset_input_uptr si(hb_subset_input_create_or_fail());
    if (! si) { return std::unexpected(err::subsetInput_failedToCreate); }

    hb_set_t *si_inputUCCPs = hb_subset_input_unicode_set(si.get());
    hb_set_set(si_inputUCCPs, unicodes_toKeep_in_ff.get());

    // Set subsetting flags
    hb_subset_input_set_flags(si.get(), HB_SUBSET_FLAGS_DEFAULT);

    // Execute subsetting
    hb_face_uptr res(hb_subset_or_fail(ff, si.get()));
    if (! res) { return std::unexpected(err::hb_subset_executeFailure); }

    // Only keep the remaining unicodeCPs by 'filtering' the ones we use from 'ff'
    hb_set_symmetric_difference(toKeep_unicodeCPs.get(), unicodes_toKeep_in_ff.get());
    return res;
}

std::expected<bool, err> Subsetter::should_include_category(hb_face_t *ff) {
    hb_set_uptr unicodes_toKeep_in_ff(hb_set_create());
    hb_face_collect_unicodes(ff, unicodes_toKeep_in_ff.get());

    hb_set_intersect(unicodes_toKeep_in_ff.get(), toKeep_unicodeCPs.get());

    bool res = not hb_set_is_empty(unicodes_toKeep_in_ff.get());

    // Only keep the remaining unicodeCPs by 'filtering' the ones we use from 'ff'
    hb_set_symmetric_difference(toKeep_unicodeCPs.get(), unicodes_toKeep_in_ff.get());
    return res;
}


} // namespace fontsmith