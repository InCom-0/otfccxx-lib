
[otfccxx-lib](https://github.com/InCom-0/otfccxx-lib) is a wrapper around (part of) the functionality of [harfbuzz-subset](https://github.com/harfbuzz/harfbuzz) and [otfcc-lib_cmake](https://github.com/InCom-0/otfcc-lib_cmake), which is itself a forked version of [otfcc](https://github.com/caryll/otfcc).<br><br>

[otfcc-lib_cmake](https://github.com/InCom-0/otfcc-lib_cmake) cleaved off the 'library' portion of the original project, modernized the build system to modern CMake (as of 2025), ensured cross-platform and (all major) modern compiler compatibility.<br><br>

[otfccxx-lib](https://github.com/InCom-0/otfccxx-lib) packages both font subsetting capability from harfbuzz-subset and some common aspects of font modifications (notably adjustments of font size, font contours and advance widths) enabled through the underlying font data model found in otfcc.

The primary usecase is the ability to produce 'minified' (web)fonts that can be embedded into html/css (ie. fonts that only include the glyphs actually present in the html document) and that are at the same time 'size-compatibile' with each other ... and being able to do that 'on the fly' in a reasonably performant way.