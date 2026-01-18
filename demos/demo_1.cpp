#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>

#include <otfccxx/otfccxx.hpp>

int
main(int argc, char *argv[]) {

    std::vector<uint32_t> keepThese{65, 75, 85, 97, 113, 117, 99, 105, 107, 84, 102, 108, 10495};

    otfccxx::Subsetter subsetter_1;
    subsetter_1.add_toKeep_CPs(keepThese).add_ff_toSubset(
        std::filesystem::path("../../../../IosevkaNerdFont-Regular.ttf"));
    auto vecOfResFonts = subsetter_1.execute();

    if (not vecOfResFonts.has_value()) { std::exit(1); }

    for (auto &oneSubsFont : vecOfResFonts.value()) {

        // otfccxx::Modifier modi_1(std::filesystem::path("../../../../iosev_2.ttf"));
        otfccxx::Modifier modi_1(oneSubsFont);
        if (auto rrr = modi_1.remove_ttfHints(); not rrr) { std::exit(1); }
        if (auto rrr = modi_1.change_unitsPerEm(2048); not rrr) { std::exit(1); }
        if (auto rrr = modi_1.change_makeMonospaced_byEmRatio(0.6); not rrr) { std::exit(1); }

        auto res = modi_1.exportResult();
        if (not res.has_value()) { std::exit(1); }
        oneSubsFont = std::move(res.value());
    }

    auto wf2 = otfccxx::Converter::encode_Woff2(vecOfResFonts->front()).and_then(otfccxx::Converter::encode_base64);

    if (wf2) { std::print("{}\n", wf2.value()); }


    std::filesystem::path outFile = "./iosev_2.ttf";
    std::ofstream         out2(outFile, std::ios::binary);

    if (not otfccxx::write_bytesToFile(outFile, vecOfResFonts->front()).has_value()) { std::exit(1); }

    std::cout << "Run finished\n";
    return 0;
}