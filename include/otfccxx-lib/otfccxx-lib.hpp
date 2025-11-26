#pragma once


#include <algorithm>
#include <expected>
#include <nlohmann/json.hpp>
#include <otfcc/font.h>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <ranges>


namespace otfccxx {
enum class err : int {
    noError = 0,
    uknownError,
    jsonFontMissingCmapTable,
    jsonFontMissingGlyfTable,
    jsonFontCorrupted,
    someRequiredCodePointsNotPresent
};

class FontMerger;

class Font {
    friend FontMerger;

    using NLMjson = nlohmann::ordered_json;

public:
    Font(NLMjson &&jsonFont) : font_(std::move(jsonFont)) {}


    // Filtering
    std::expected<std::vector<int>, err> filter_glyphs_inPlace(std::vector<int> const &charCodes_toKeep) {

        // Instead of deleting we will insert into a 'cleaned' copy only those charCodes that we want/need
        NLMjson res = font_;

        auto cmapIT = res.find("cmap");
        if (cmapIT == res.end()) { return std::unexpected(err::jsonFontMissingCmapTable); }
        auto &cmp = (*cmapIT);
        cmp.clear();

        auto glyfIT = res.find("glyf");
        if (glyfIT == res.end()) { return std::unexpected(err::jsonFontMissingGlyfTable); }
        auto &glf = (*glyfIT);
        glf.clear();

        // Those CCs not found in the original font be kept here and returned
        std::vector<int> toKeep_butMissing;

        auto &orig_cmp = *(font_.find("cmap"));
        auto &orig_glf = *(font_.find("glyf"));

        std::unordered_set<std::string> alreadyAdded;
        for (auto const one_toKeep : charCodes_toKeep) {
            auto item_it = orig_cmp.find(std::to_string(one_toKeep));

            std::vector<std::pair<std::string, std::string>>        toAddCmp;
            std::vector<std::pair<std::string, decltype(*item_it)>> toAddGlyf;
            std::unordered_set<std::string>                         alreadyExplored;

            auto adder = [&](this auto &self, decltype(item_it) const &orig_cmp_iter) -> bool {
                // It must not have been added before
                // It must not have been already explored (protects against cyclic dependendencies)
                if (not alreadyAdded.contains(orig_cmp_iter.key()) &&
                    alreadyExplored.insert(orig_cmp_iter.key()).second) {

                    auto orig_glf_iter = orig_glf.find(*orig_cmp_iter);
                    // There must be a glyf associated
                    if (orig_glf_iter != orig_glf.end()) {
                        if (auto ref_iter = orig_glf_iter->find("references"); ref_iter != orig_glf_iter->end()) {
                            // If the glyf contains some references to other glyfs
                            for (auto &[_, val] : ref_iter->items()) {
                                if (auto refName_iter = val.find("glyph"); refName_iter != val.end()) {
                                    // Find whether a referenced glyf exists in CMap
                                    auto cmpToRef_iter =
                                        std::find_if(orig_cmp.begin(), orig_cmp.end(),
                                                     [&](auto const &elem) { return elem == (*refName_iter); });
                                    // If the referenced glyf is not found in the Cmap then false all the way up
                                    if (cmpToRef_iter == orig_cmp.end()) { return false; }
                                    // If the recursion returns false then also return false
                                    if (not self(cmpToRef_iter)) { return false; }
                                }
                            }
                        }
                        toAddCmp.push_back({orig_cmp_iter.key(), *orig_cmp_iter});
                        toAddGlyf.push_back({orig_glf_iter.key(), *orig_glf_iter});
                        alreadyAdded.insert(orig_cmp_iter.key());
                    }
                    else { return false; }
                }
                return true;
            };

            if (item_it != orig_cmp.end() && adder(item_it)) {
                for (auto rIter = toAddCmp.rbegin(); rIter != toAddCmp.rend(); ++rIter) {
                    cmp.push_back({rIter->first, rIter->second});
                }
                for (auto rIter = toAddGlyf.rbegin(); rIter != toAddGlyf.rend(); ++rIter) {
                    glf.push_back({rIter->first, rIter->second});
                }
            }
            else { toKeep_butMissing.push_back(one_toKeep); }
        }

        // Replace with a filtered copy
        font_ = res;
        return toKeep_butMissing;
    }


private:
    NLMjson font_;
};


} // namespace otfccxx