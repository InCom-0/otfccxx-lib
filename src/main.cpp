#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>

#include <otfccxx-lib/otfccxx-lib.hpp>

int
main(int argc, char *argv[]) {

    std::vector<uint32_t> keepThese{65, 75, 85, 97};

    otfccxx::Subsetter subsetter_1;
    subsetter_1.add_toKeep_CPs(keepThese);
    subsetter_1.add_ff_toSubset(std::filesystem::path("./IosevkaNerdFont-Regular.ttf"));

    std::filesystem::path outFile = "./subsetted.ttf";


    std::ofstream out(outFile, std::ios::binary);

    auto res = subsetter_1.execute();
    out.write(reinterpret_cast<const char *>(res.value().front().data()),
              static_cast<std::streamsize>(res.value().front().size()));

    out.flush();

    std::cout << "Run finished\n";
    return 0;
}