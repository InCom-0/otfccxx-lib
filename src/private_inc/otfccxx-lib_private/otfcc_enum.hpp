#pragma once

#include <cstdint>


namespace otfccxx {

enum class otfcc_glyfTable_nameMapping : uint32_t {
    head = 0x68656164, // 'head'
    hhea = 0x68686561, // 'hhea'
    maxp = 0x6D617870, // 'maxp'

    OS_2 = 0x4F535F32, // 'OS_2'

    name = 0x6E616D65, // 'name'
    meta = 0x6D657461, // 'meta'
    hmtx = 0x686D7478, // 'hmtx'
    vmtx = 0x766D7478, // 'vmtx'
    post = 0x706F7374, // 'post'

    vhea = 0x76686561, // 'vhea'
    fpgm = 0x6670676D, // 'fpgm'
    prep = 0x70726570, // 'prep'

    cvt = 0x6376745F,  // 'cvt_'

    gasp = 0x67617370, // 'gasp'

    CFF = 0x4346465F,  // 'CFF_'

    glyf = 0x676C7966, // 'glyf'
    cmap = 0x636D6170, // 'cmap'
    LTSH = 0x4C545348, // 'LTSH'

    GSUB = 0x47535542, // 'GSUB'
    GPOS = 0x47504F53, // 'GPOS'
    GDEF = 0x47444546, // 'GDEF'
    BASE = 0x42415345, // 'BASE'
    VORG = 0x564F5247, // 'VORG'

    CPAL = 0x4350414C, // 'CPAL'
    COLR = 0x434F4C52, // 'COLR'

    SVG = 0x53564720,  // 'SVG '

    TSI0 = 0x54534930, // 'TSI0'
    TSI1 = 0x54534931, // 'TSI1'
    TSI2 = 0x54534932, // 'TSI2'
    TSI3 = 0x54534933, // 'TSI3'
    TSI5 = 0x54534935  // 'TSI5'
};
} // namespace otfccxx