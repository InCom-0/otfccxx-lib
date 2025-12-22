#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <otfccxx-lib/otfccxx-lib.hpp>

int
main(int argc, char *argv[]) {

    std::vector<uint32_t> keepThese{65, 75, 85, 97};

    otfccxx::Subsetter subsetter_1;
    subsetter_1.add_toKeep_CPs(keepThese).add_ff_toSubset(std::filesystem::path("./IosevkaNerdFont-Regular.ttf"));

    std::filesystem::path outFile = "./subsetted.ttf";
    auto                  res     = subsetter_1.execute();

    otfccxx::Modifier modifier_1(res->front());
    if (auto rrr = modifier_1.change_unitsPerEm(2048); not rrr) { std::exit(1); }

    auto res2 = modifier_1.exportResult();
    if (not res2.has_value()) { std::exit(1); }


    std::ofstream out(outFile, std::ios::binary);


    // out.write(reinterpret_cast<const char *>(res.value().front().data()),
    //           static_cast<std::streamsize>(res.value().front().size()));

    out.write(reinterpret_cast<const char *>(res2.value().data()), static_cast<std::streamsize>(res2.value().size()));

    out.flush();

    std::cout << "Run finished\n";
    return 0;
}