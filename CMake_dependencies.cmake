include(cmake/CPM.cmake)
set(CPM_USE_LOCAL_PACKAGES_ORIG ${CPM_USE_LOCAL_PACKAGES})
set(CPM_USE_LOCAL_PACKAGES ON)

CPMAddPackage("gh:InCom-0/fmem#master")
CPMAddPackage("gh:InCom-0/otfcc-lib_cmake#master")
# CPMAddPackage(
#   NAME nlohmann_json
#   URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
#   URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
#   EXCLUDE_FROM_ALL TRUE
# )

set(HB_BUILD_UTILS OFF)
# CPMAddPackage(
#   NAME     harfbuzz
#   URL      https://github.com/harfbuzz/harfbuzz/releases/download/12.2.0/harfbuzz-12.2.0.tar.xz
#   URL_HASH SHA256=ecb603aa426a8b24665718667bda64a84c1504db7454ee4cadbd362eea64e545
#   EXCLUDE_FROM_ALL TRUE
# )

set(CPM_USE_LOCAL_PACKAGES OFF)
CPMAddPackage("gh:InCom-0/harfbuzz#symbolDedup")

set(CPM_USE_LOCAL_PACKAGES ${CPM_USE_LOCAL_PACKAGES_ORIG})
