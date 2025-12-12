#include <iostream>

#include <otfccxx-lib/fontsmith.hpp>

int main(int argc, char *argv[]) {


    fontsmith::Subsetter subsetter_1;

    subsetter_1.add_toKeep_CP(65);
    std::cout << "Get lost \n";
    return 0;
}