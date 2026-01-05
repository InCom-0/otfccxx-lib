#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <otfccxx-lib/otfccxx-lib.hpp>
#include <woff2/encode.h>

int
main(int argc, char *argv[]) {

    std::vector<uint32_t> keepThese{65, 75, 85, 97, 113, 117, 99, 105, 107, 84, 102, 108};

    otfccxx::Subsetter subsetter_1;
    subsetter_1.add_toKeep_CPs(keepThese).add_ff_toSubset(std::filesystem::path("./JetBrainsMonoNerdFont-Regular.ttf"));
    auto vecOfFonts_subsS = subsetter_1.execute();


    otfccxx::Modifier_V2 modifier_2(vecOfFonts_subsS->front());
    if (auto rrr = modifier_2.remove_ttfHints(); not rrr) { std::exit(1); }
    if (auto rrr = modifier_2.change_unitsPerEm(1000); not rrr) { std::exit(1); }
    if (auto rrr = modifier_2.change_makeMonospaced_byEmRatio(0.6); not rrr) { std::exit(1); }

    auto res = modifier_2.exportResult();
    if (not res.has_value()) { std::exit(1); }

    // auto res3 = otfccxx::Converter::encode_Woff2(res2.value());
    // if (not res3.has_value()) { std::exit(1); }


    std::filesystem::path outFile = "./jb_2.ttf";
    std::ofstream         out2(outFile, std::ios::binary);

    if (not otfccxx::write_bytesToFile(outFile, res.value()).has_value()) { std::exit(1); }

    std::cout << "Run finished\n";
    return 0;
}