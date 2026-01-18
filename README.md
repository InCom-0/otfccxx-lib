
[otfccxx](https://github.com/InCom-0/otfccxx) is a wrapper around (part of) the functionality of [harfbuzz-subset](https://github.com/harfbuzz/harfbuzz) and [otfcc-lib_cmake](https://github.com/InCom-0/otfcc-lib_cmake), which is itself a forked version of [otfcc](https://github.com/caryll/otfcc).<br><br>

[otfcc-lib_cmake](https://github.com/InCom-0/otfcc-lib_cmake) added the underlying 'library portion' of the original [otfcc](https://github.com/caryll/otfcc) as 'otfcc_lib' first class target in CMake, modernized the build system to modern CMake (as of 2025), ensured cross-platform and (all major) modern compiler compatibility.<br><br>

[otfccxx](https://github.com/InCom-0/otfccxx) packages both font subsetting capability from harfbuzz-subset and some common aspects of font modifications (notably adjustments of font size, font contours and advance widths) enabled through the underlying font data model found in [otfcc](https://github.com/caryll/otfcc).<br><br>

The primary usecase is the ability to produce 'minified' (web)fonts that can be embedded into html/css (ie. fonts that only include the glyphs actually present in the html document) and that are at the same time 'size-compatibile' with each other ... and being able to do that 'on the fly' in a reasonably performant way.

Explanatory note: The goal is to enable a way to use several fonts together in a way so that they complement each other in Unicode codepoint coverage (because fonts often include only the glyphs for the most common Unicode codepoints such as the latin letter) by adjusting and modifying the fonts (the glyphs themselves, the font metrics and so on) in such a way that they will be visually compatible once their glyphs are rendered (for example by a web browser).
This is as opposed to somehow 'splicing' fonts together which is (in the author's opinion) borderline impossible to do programmatically.

Note: The results obtained using such programmatic modifications of fonts are not expected to be 'typographically perfect'. However, anecdotal testing has shown that the results can be more than acceptable and are often very good (author's opinion). 

Also note: Some of the more advanced features that can appear in fonts (such as most versions of ligatures) are not supported.