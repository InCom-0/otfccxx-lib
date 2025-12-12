include(cmake/CPM.cmake)

CPMAddPackage("gh:InCom-0/otfcc-lib_cmake#master")

CPMAddPackage(
  NAME     nlh_json
  URL      https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
  URL_HASH SHA256=42f6e95cad6ec532fd372391373363b62a14af6d771056dbfc86160e6dfff7aa
  EXCLUDE_FROM_ALL TRUE
)

set(HB_BUILD_UTILS ON)
CPMAddPackage(
  NAME     harfbuzz
  URL      https://github.com/harfbuzz/harfbuzz/releases/download/12.2.0/harfbuzz-12.2.0.tar.xz
  URL_HASH SHA256=ecb603aa426a8b24665718667bda64a84c1504db7454ee4cadbd362eea64e545
  EXCLUDE_FROM_ALL TRUE
)

