#include "StorageFontRegistry.h"

#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstring>

#include "CrossPointSettings.h"
#include "ExternalFontIds.h"
#include "FontCacheManager.h"
#include "GfxRenderer.h"

namespace {
constexpr char FONT_MAGIC[] = {'E', 'P', 'F', '2'};
constexpr uint16_t FONT_PACK_VERSION = 2;

#pragma pack(push, 1)

struct FontPackHeader {
  char magic[4];
  uint16_t version;
  uint16_t flags;
  uint32_t bitmapSize;
  uint32_t glyphCount;
  uint32_t intervalCount;
  uint32_t groupCount;
  uint32_t kernLeftCount;
  uint32_t kernRightCount;
  uint32_t kernMatrixCount;
  uint32_t ligatureCount;
  int16_t advanceY;
  int16_t ascender;
  int16_t descender;
  uint8_t is2Bit;
  uint8_t kernLeftClassCount;
  uint8_t kernRightClassCount;
  uint8_t reserved[5];
};

struct PackedGlyph {
  uint8_t width;
  uint8_t height;
  uint16_t advanceX;
  int16_t left;
  int16_t top;
  uint16_t dataLength;
  uint32_t dataOffset;
};

struct PackedInterval {
  uint32_t first;
  uint32_t last;
  uint32_t offset;
};

struct PackedGroup {
  uint32_t compressedOffset;
  uint32_t compressedSize;
  uint32_t uncompressedSize;
  uint16_t glyphCount;
  uint16_t reserved;
  uint32_t firstGlyphIndex;
};
#pragma pack(pop)

StorageFontPack tc12Pack;
StorageFontPack tc8Pack;
StorageFontPack tc10Pack;
StorageFontPack tc14Pack;
StorageFontPack tc16Pack;
StorageFontPack tc18Pack;
StorageFontPack tc8BoldPack;
StorageFontPack tc8ItalicPack;
StorageFontPack tc8BoldItalicPack;
StorageFontPack tc10BoldPack;
StorageFontPack tc10ItalicPack;
StorageFontPack tc10BoldItalicPack;
StorageFontPack tc12BoldPack;
StorageFontPack tc12ItalicPack;
StorageFontPack tc12BoldItalicPack;
StorageFontPack tc14BoldPack;
StorageFontPack tc14ItalicPack;
StorageFontPack tc14BoldItalicPack;
StorageFontPack tc16BoldPack;
StorageFontPack tc16ItalicPack;
StorageFontPack tc16BoldItalicPack;
StorageFontPack tc18BoldPack;
StorageFontPack tc18ItalicPack;
StorageFontPack tc18BoldItalicPack;

struct StorageFontFamilySlot {
  StorageFontPack* regular;
  StorageFontPack* bold;
  StorageFontPack* italic;
  StorageFontPack* boldItalic;
  std::unique_ptr<EpdFontFamily> family;
  bool loaded = false;

  void reset() {
    regular->reset();
    bold->reset();
    italic->reset();
    boldItalic->reset();
    family.reset();
    loaded = false;
  }
};

StorageFontFamilySlot tc8Family{&tc8Pack, &tc8BoldPack, &tc8ItalicPack, &tc8BoldItalicPack};
StorageFontFamilySlot tc10Family{&tc10Pack, &tc10BoldPack, &tc10ItalicPack, &tc10BoldItalicPack};
StorageFontFamilySlot tc12Family{&tc12Pack, &tc12BoldPack, &tc12ItalicPack, &tc12BoldItalicPack};
StorageFontFamilySlot tc14Family{&tc14Pack, &tc14BoldPack, &tc14ItalicPack, &tc14BoldItalicPack};
StorageFontFamilySlot tc16Family{&tc16Pack, &tc16BoldPack, &tc16ItalicPack, &tc16BoldItalicPack};
StorageFontFamilySlot tc18Family{&tc18Pack, &tc18BoldPack, &tc18ItalicPack, &tc18BoldItalicPack};

constexpr TraditionalChineseFontPacks kTraditionalChineseFontPacks = {{
    {"Noto Sans CJK 8", "/.mofei/fonts/notosans_tc_8.epf", CrossPointSettings::EXTRA_SMALL, 8, NOTOSANS_TC_8_FONT_ID},
    {"Noto Sans CJK 10", "/.mofei/fonts/notosans_tc_10.epf", CrossPointSettings::SMALL, 10, NOTOSANS_TC_10_FONT_ID},
    {"Noto Sans CJK 12", "/.mofei/fonts/notosans_tc_12.epf", CrossPointSettings::SMALL, 12, NOTOSANS_TC_12_FONT_ID},
    {"Noto Sans CJK 14", "/.mofei/fonts/notosans_tc_14.epf", CrossPointSettings::MEDIUM, 14, NOTOSANS_TC_14_FONT_ID},
    {"Noto Sans CJK 16", "/.mofei/fonts/notosans_tc_16.epf", CrossPointSettings::LARGE, 16, NOTOSANS_TC_16_FONT_ID},
    {"Noto Sans CJK 18", "/.mofei/fonts/notosans_tc_18.epf", CrossPointSettings::EXTRA_LARGE, 18,
     NOTOSANS_TC_18_FONT_ID},
}};

constexpr TraditionalChineseFontFaces kTraditionalChineseFontFaces = {{
    {"Noto Sans CJK 8", "/.mofei/fonts/notosans_tc_8.epf", CrossPointSettings::EXTRA_SMALL, 8, EpdFontFamily::REGULAR},
    {"Noto Sans CJK 8 Bold", "/.mofei/fonts/notosans_tc_8_bold.epf", CrossPointSettings::EXTRA_SMALL, 8,
     EpdFontFamily::BOLD},
    {"Noto Sans CJK 8 Italic", "/.mofei/fonts/notosans_tc_8_italic.epf", CrossPointSettings::EXTRA_SMALL, 8,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 8 Bold Italic", "/.mofei/fonts/notosans_tc_8_bolditalic.epf", CrossPointSettings::EXTRA_SMALL, 8,
     EpdFontFamily::BOLD_ITALIC},
    {"Noto Sans CJK 10", "/.mofei/fonts/notosans_tc_10.epf", CrossPointSettings::SMALL, 10, EpdFontFamily::REGULAR},
    {"Noto Sans CJK 10 Bold", "/.mofei/fonts/notosans_tc_10_bold.epf", CrossPointSettings::SMALL, 10,
     EpdFontFamily::BOLD},
    {"Noto Sans CJK 10 Italic", "/.mofei/fonts/notosans_tc_10_italic.epf", CrossPointSettings::SMALL, 10,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 10 Bold Italic", "/.mofei/fonts/notosans_tc_10_bolditalic.epf", CrossPointSettings::SMALL, 10,
     EpdFontFamily::BOLD_ITALIC},
    {"Noto Sans CJK 12", "/.mofei/fonts/notosans_tc_12.epf", CrossPointSettings::SMALL, 12, EpdFontFamily::REGULAR},
    {"Noto Sans CJK 12 Bold", "/.mofei/fonts/notosans_tc_12_bold.epf", CrossPointSettings::SMALL, 12,
     EpdFontFamily::BOLD},
    {"Noto Sans CJK 12 Italic", "/.mofei/fonts/notosans_tc_12_italic.epf", CrossPointSettings::SMALL, 12,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 12 Bold Italic", "/.mofei/fonts/notosans_tc_12_bolditalic.epf", CrossPointSettings::SMALL, 12,
     EpdFontFamily::BOLD_ITALIC},
    {"Noto Sans CJK 14", "/.mofei/fonts/notosans_tc_14.epf", CrossPointSettings::MEDIUM, 14, EpdFontFamily::REGULAR},
    {"Noto Sans CJK 14 Bold", "/.mofei/fonts/notosans_tc_14_bold.epf", CrossPointSettings::MEDIUM, 14,
     EpdFontFamily::BOLD},
    {"Noto Sans CJK 14 Italic", "/.mofei/fonts/notosans_tc_14_italic.epf", CrossPointSettings::MEDIUM, 14,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 14 Bold Italic", "/.mofei/fonts/notosans_tc_14_bolditalic.epf", CrossPointSettings::MEDIUM, 14,
     EpdFontFamily::BOLD_ITALIC},
    {"Noto Sans CJK 16", "/.mofei/fonts/notosans_tc_16.epf", CrossPointSettings::LARGE, 16, EpdFontFamily::REGULAR},
    {"Noto Sans CJK 16 Bold", "/.mofei/fonts/notosans_tc_16_bold.epf", CrossPointSettings::LARGE, 16,
     EpdFontFamily::BOLD},
    {"Noto Sans CJK 16 Italic", "/.mofei/fonts/notosans_tc_16_italic.epf", CrossPointSettings::LARGE, 16,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 16 Bold Italic", "/.mofei/fonts/notosans_tc_16_bolditalic.epf", CrossPointSettings::LARGE, 16,
     EpdFontFamily::BOLD_ITALIC},
    {"Noto Sans CJK 18", "/.mofei/fonts/notosans_tc_18.epf", CrossPointSettings::EXTRA_LARGE, 18,
     EpdFontFamily::REGULAR},
    {"Noto Sans CJK 18 Bold", "/.mofei/fonts/notosans_tc_18_bold.epf", CrossPointSettings::EXTRA_LARGE, 18,
     EpdFontFamily::BOLD},
    {"Noto Sans CJK 18 Italic", "/.mofei/fonts/notosans_tc_18_italic.epf", CrossPointSettings::EXTRA_LARGE, 18,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 18 Bold Italic", "/.mofei/fonts/notosans_tc_18_bolditalic.epf", CrossPointSettings::EXTRA_LARGE, 18,
     EpdFontFamily::BOLD_ITALIC},
}};

struct PackRuntime {
  const TraditionalChineseFontPackInfo* info;
  StorageFontFamilySlot* family;
};

const std::array<PackRuntime, 6> kPackRuntimes = {{
    {&kTraditionalChineseFontPacks[0], &tc8Family},
    {&kTraditionalChineseFontPacks[1], &tc10Family},
    {&kTraditionalChineseFontPacks[2], &tc12Family},
    {&kTraditionalChineseFontPacks[3], &tc14Family},
    {&kTraditionalChineseFontPacks[4], &tc16Family},
    {&kTraditionalChineseFontPacks[5], &tc18Family},
}};

// Mofei 的 PSRAM 很紧张，启动只加载 TC_8；Reader 需要更大的字号时再热切换到
// TC_10，并且切换前必须卸载旧 pack，避免同时常驻多个 CJK 字体。
constexpr int kStartupTraditionalChineseFontId = NOTOSANS_TC_8_FONT_ID;
constexpr int kSmallTraditionalChineseFontId = NOTOSANS_TC_10_FONT_ID;
constexpr int kReaderExtraSmallTraditionalChineseFontId = NOTOSANS_TC_8_FONT_ID;

// Memory pressure mitigation: only TC_8 and TC_10 may live in PSRAM. Other
// sizes are explicitly rejected so a misroute never silently loads a 2-3 MB
// pack that fragments the heap.
constexpr bool isAllowedTraditionalChineseFontId(int fontId) {
  return fontId == NOTOSANS_TC_8_FONT_ID || fontId == NOTOSANS_TC_10_FONT_ID;
}

void logTcMemory(const char* event, const char* path) {
  LOG_INF("TCFONT_MEM", "%s path=%s heap_free=%u heap_max=%u psram_free=%u psram_max=%u", event,
          path != nullptr ? path : "-", ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getFreePsram(),
          ESP.getMaxAllocPsram());
}

template <typename T>
bool readArray(FsFile& file, std::vector<T>& out, size_t count) {
  out.resize(count);
  if (count == 0) return true;
  const size_t bytes = sizeof(T) * count;
  int read_bytes = file.read(out.data(), bytes);
  if (read_bytes != static_cast<int>(bytes)) {
    LOG_ERR("TCFONT", "readArray failed: requested %zu, read %d", bytes, read_bytes);
    return false;
  }
  return true;
}

const PackRuntime* getRuntimeByFontId(const int fontId) {
  const auto it = std::find_if(kPackRuntimes.begin(), kPackRuntimes.end(),
                               [fontId](const PackRuntime& runtime) { return runtime.info->fontId == fontId; });
  return it == kPackRuntimes.end() ? nullptr : &*it;
}

const PackRuntime* getRuntimeByFontSize(const uint8_t fontSize) {
  // PSRAM-tight policy: only TC_8 and TC_10 are loadable. EXTRA_SMALL routes to
  // TC_8; every larger reader font size maps to TC_10. Reader text will render
  // smaller than its nominal size, but fits memory and avoids fragmentation.
  const int targetFontId = (fontSize == CrossPointSettings::EXTRA_SMALL) ? kReaderExtraSmallTraditionalChineseFontId
                                                                         : kSmallTraditionalChineseFontId;
  const auto* preferred = getRuntimeByFontId(targetFontId);
  if (preferred != nullptr && Storage.exists(preferred->info->path)) {
    return preferred;
  }
  // Fallback to the other allowed pack so something still renders if the
  // primary EPF file is missing on the SD card.
  const int alternateFontId = (targetFontId == kReaderExtraSmallTraditionalChineseFontId)
                                  ? kSmallTraditionalChineseFontId
                                  : kReaderExtraSmallTraditionalChineseFontId;
  const auto* alternate = getRuntimeByFontId(alternateFontId);
  return alternate != nullptr ? alternate : preferred;
}

const PackRuntime* getCurrentLoadedRuntime() {
  const auto it = std::find_if(kPackRuntimes.begin(), kPackRuntimes.end(),
                               [](const PackRuntime& runtime) { return runtime.family->loaded; });
  return it == kPackRuntimes.end() ? nullptr : &*it;
}

const PackRuntime* getLoadableRuntimeByFontId(const int fontId) {
  const auto* runtime = getRuntimeByFontId(fontId);
  if (runtime == nullptr) {
    return nullptr;
  }
  if (isAllowedTraditionalChineseFontId(runtime->info->fontId)) {
    return runtime;
  }

  LOG_ERR("TCFONT", "Refused unsupported TC pack fontId=%d pointSize=%u; only 8pt and 10pt are loadable", fontId,
          runtime->info->pointSize);
  return nullptr;
}

void unloadTraditionalChineseFont(GfxRenderer& renderer, const PackRuntime& runtime) {
  logTcMemory("unload_begin", runtime.info->path);
  renderer.removeFont(runtime.info->fontId);
  runtime.family->reset();
  logTcMemory("unload_end", runtime.info->path);
}

bool loadTraditionalChineseRuntime(GfxRenderer& renderer, const PackRuntime& target, const bool forceReload = false) {
  if (!isAllowedTraditionalChineseFontId(target.info->fontId)) {
    LOG_ERR("TCFONT", "Refused to load disallowed TC pack %s (fontId=%d). PSRAM-restricted whitelist is {TC_8, TC_10}.",
            target.info->path, target.info->fontId);
    return false;
  }

  Storage.mkdir("/.mofei");
  Storage.mkdir("/.mofei/fonts");

  bool unloadedAny = false;
  for (const auto& runtime : kPackRuntimes) {
    if ((forceReload || runtime.info->fontId != target.info->fontId) && runtime.family->loaded) {
      unloadTraditionalChineseFont(renderer, runtime);
      unloadedAny = true;
    }
  }

  if (target.family->loaded) {
    if (unloadedAny) {
      if (auto* cacheManager = renderer.getFontCacheManager()) {
        cacheManager->clearCache();
      }
    }
    return true;
  }

  target.family->reset();

  const auto* regular =
      StorageFontRegistry::getTraditionalChineseFontFaceById(target.info->fontId, EpdFontFamily::REGULAR);
  const auto* bold = StorageFontRegistry::getTraditionalChineseFontFaceById(target.info->fontId, EpdFontFamily::BOLD);
  const auto* italic =
      StorageFontRegistry::getTraditionalChineseFontFaceById(target.info->fontId, EpdFontFamily::ITALIC);
  const auto* boldItalic =
      StorageFontRegistry::getTraditionalChineseFontFaceById(target.info->fontId, EpdFontFamily::BOLD_ITALIC);

  logTcMemory("load_begin", target.info->path);

  if (regular != nullptr && target.family->regular->load(regular->path)) {
    if (bold != nullptr) target.family->bold->load(bold->path);
    if (italic != nullptr) target.family->italic->load(italic->path);
    if (boldItalic != nullptr) target.family->boldItalic->load(boldItalic->path);

    target.family->family = std::make_unique<EpdFontFamily>(
        &target.family->regular->font(), target.family->bold->loaded() ? &target.family->bold->font() : nullptr,
        target.family->italic->loaded() ? &target.family->italic->font() : nullptr,
        target.family->boldItalic->loaded() ? &target.family->boldItalic->font() : nullptr);
    target.family->loaded = true;
    renderer.insertFont(target.info->fontId, *target.family->family);
    logTcMemory("load_end", target.info->path);
    if (auto* cacheManager = renderer.getFontCacheManager()) {
      cacheManager->clearCache();
    }
    return true;
  }

  renderer.removeFont(target.info->fontId);
  logTcMemory("load_failed", target.info->path);
  LOG_ERR("TCFONT", "Multilingual font family not found or load failed: %s", target.info->path);
  target.family->reset();
  if (auto* cacheManager = renderer.getFontCacheManager()) {
    cacheManager->clearCache();
  }
  return false;
}
}  // namespace

void StorageFontPack::reset() {
  std::vector<uint8_t>().swap(bitmap_);
  std::vector<EpdGlyph>().swap(glyphs_);
  std::vector<EpdUnicodeInterval>().swap(intervals_);
  std::vector<EpdFontGroup>().swap(groups_);
  std::vector<EpdKernClassEntry>().swap(kernLeftClasses_);
  std::vector<EpdKernClassEntry>().swap(kernRightClasses_);
  std::vector<int8_t>().swap(kernMatrix_);
  std::vector<EpdLigaturePair>().swap(ligaturePairs_);
  data_ = {};
  font_.reset();
  loaded_ = false;
}

bool StorageFontPack::load(const char* path) {
  reset();

  FsFile file;
  if (!Storage.openFileForRead("TCFONT", path, file)) {
    LOG_ERR("TCFONT", "Failed to open %s", path);
    return false;
  }

  FontPackHeader header = {};
  if (file.read(&header, sizeof(header)) != static_cast<int>(sizeof(header))) {
    LOG_ERR("TCFONT", "Failed to read header of %s", path);
    return false;
  }

  if (memcmp(header.magic, FONT_MAGIC, sizeof(FONT_MAGIC)) != 0 || header.version != FONT_PACK_VERSION) {
    LOG_ERR("TCFONT", "Invalid font pack header in %s: magic=%c%c%c%c v=%d", path, header.magic[0], header.magic[1],
            header.magic[2], header.magic[3], header.version);
    return false;
  }

  if (header.flags != 0 || std::any_of(std::begin(header.reserved), std::end(header.reserved),
                                       [](const uint8_t value) { return value != 0; })) {
    LOG_DBG("TCFONT", "Ignoring reserved font pack header bits: %s", path);
  }

  LOG_DBG("TCFONT", "Sizes: Header=%u Glyph=%u Int=%u Grp=%u Kern=%u Lig=%u", sizeof(FontPackHeader),
          sizeof(PackedGlyph), sizeof(PackedInterval), sizeof(PackedGroup), sizeof(EpdKernClassEntry),
          sizeof(EpdLigaturePair));

  LOG_DBG("TCFONT", "Allocating bitmap: %u bytes for %s", header.bitmapSize, path);
  bitmap_.resize(header.bitmapSize);

  if (header.bitmapSize > 0) {
    LOG_DBG("TCFONT", "Reading bitmap for %s...", path);
    if (file.read(bitmap_.data(), header.bitmapSize) != static_cast<int>(header.bitmapSize)) {
      LOG_ERR("TCFONT", "Failed to read bitmap data of %s", path);
      return false;
    }
  }

  std::vector<PackedGlyph> rawGlyphs;
  std::vector<PackedInterval> rawIntervals;
  std::vector<PackedGroup> rawGroups;

  LOG_DBG("TCFONT", "Reading arrays for %s...", path);
  if (!readArray(file, rawGlyphs, header.glyphCount)) {
    LOG_ERR("TCFONT", "Failed glyphs");
    return false;
  }
  if (!readArray(file, rawIntervals, header.intervalCount)) {
    LOG_ERR("TCFONT", "Failed intervals");
    return false;
  }
  if (!readArray(file, rawGroups, header.groupCount)) {
    LOG_ERR("TCFONT", "Failed groups");
    return false;
  }
  if (!readArray(file, kernLeftClasses_, header.kernLeftCount)) {
    LOG_ERR("TCFONT", "Failed kernLeft");
    return false;
  }
  if (!readArray(file, kernRightClasses_, header.kernRightCount)) {
    LOG_ERR("TCFONT", "Failed kernRight");
    return false;
  }
  if (!readArray(file, kernMatrix_, header.kernMatrixCount)) {
    LOG_ERR("TCFONT", "Failed kernMatrix");
    return false;
  }
  if (!readArray(file, ligaturePairs_, header.ligatureCount)) {
    LOG_ERR("TCFONT", "Failed ligatures");
    return false;
  }

  LOG_DBG("TCFONT", "Processing arrays for %s...", path);
  glyphs_.resize(rawGlyphs.size());
  for (size_t i = 0; i < rawGlyphs.size(); i++) {
    glyphs_[i] = {rawGlyphs[i].width, rawGlyphs[i].height,     rawGlyphs[i].advanceX,  rawGlyphs[i].left,
                  rawGlyphs[i].top,   rawGlyphs[i].dataLength, rawGlyphs[i].dataOffset};
  }

  intervals_.resize(rawIntervals.size());
  for (size_t i = 0; i < rawIntervals.size(); i++) {
    intervals_[i] = {rawIntervals[i].first, rawIntervals[i].last, rawIntervals[i].offset};
  }

  if (!intervals_.empty()) {
    LOG_DBG("TCFONT", "Interval[0]: %u-%u offset %u", intervals_.front().first, intervals_.front().last,
            intervals_.front().offset);
    LOG_DBG("TCFONT", "Interval[%zu]: %u-%u offset %u", intervals_.size() - 1, intervals_.back().first,
            intervals_.back().last, intervals_.back().offset);
  }

  groups_.resize(rawGroups.size());
  for (size_t i = 0; i < rawGroups.size(); i++) {
    if (rawGroups[i].reserved != 0) {
      LOG_DBG("TCFONT", "Ignoring reserved font pack group bits: %s", path);
    }
    groups_[i] = {rawGroups[i].compressedOffset, rawGroups[i].compressedSize, rawGroups[i].uncompressedSize,
                  rawGroups[i].glyphCount, rawGroups[i].firstGlyphIndex};
  }

  data_.bitmap = bitmap_.data();
  data_.glyph = glyphs_.data();
  data_.intervals = intervals_.data();
  data_.intervalCount = static_cast<uint32_t>(intervals_.size());
  data_.advanceY = static_cast<uint8_t>(header.advanceY);
  data_.ascender = header.ascender;
  data_.descender = header.descender;
  data_.is2Bit = header.is2Bit != 0;
  data_.groups = groups_.empty() ? nullptr : groups_.data();
  data_.groupCount = static_cast<uint16_t>(groups_.size());
  data_.glyphToGroup = nullptr;
  data_.kernLeftClasses = kernLeftClasses_.empty() ? nullptr : kernLeftClasses_.data();
  data_.kernRightClasses = kernRightClasses_.empty() ? nullptr : kernRightClasses_.data();
  data_.kernMatrix = kernMatrix_.empty() ? nullptr : kernMatrix_.data();
  data_.kernLeftEntryCount = static_cast<uint16_t>(kernLeftClasses_.size());
  data_.kernRightEntryCount = static_cast<uint16_t>(kernRightClasses_.size());
  data_.kernLeftClassCount = header.kernLeftClassCount;
  data_.kernRightClassCount = header.kernRightClassCount;
  data_.ligaturePairs = ligaturePairs_.empty() ? nullptr : ligaturePairs_.data();
  data_.ligaturePairCount = static_cast<uint32_t>(ligaturePairs_.size());

  font_ = std::make_unique<EpdFont>(&data_);
  loaded_ = true;
  return true;
}

namespace StorageFontRegistry {

const TraditionalChineseFontPacks& getTraditionalChineseFontPacks() { return kTraditionalChineseFontPacks; }

const TraditionalChineseFontFaces& getTraditionalChineseFontFaces() { return kTraditionalChineseFontFaces; }

const TraditionalChineseFontPackInfo* getTraditionalChineseFontPack(uint8_t fontSize) {
  const auto* runtime = getRuntimeByFontSize(fontSize);
  return runtime == nullptr ? nullptr : runtime->info;
}

const TraditionalChineseFontFaceInfo* getTraditionalChineseFontFace(uint8_t fontSize, EpdFontFamily::Style style) {
  const auto* pack = getTraditionalChineseFontPack(fontSize);
  if (pack == nullptr) {
    return nullptr;
  }
  return getTraditionalChineseFontFaceById(pack->fontId, style);
}

const TraditionalChineseFontPackInfo* getTraditionalChineseFontPackById(int fontId) {
  const auto it = std::find_if(kTraditionalChineseFontPacks.begin(), kTraditionalChineseFontPacks.end(),
                               [fontId](const TraditionalChineseFontPackInfo& pack) { return pack.fontId == fontId; });
  return it == kTraditionalChineseFontPacks.end() ? nullptr : &*it;
}

const TraditionalChineseFontFaceInfo* getTraditionalChineseFontFaceById(int fontId, EpdFontFamily::Style style) {
  const auto* pack = getTraditionalChineseFontPackById(fontId);
  if (pack == nullptr) {
    return nullptr;
  }
  const auto it = std::find_if(kTraditionalChineseFontFaces.begin(), kTraditionalChineseFontFaces.end(),
                               [pack, style](const TraditionalChineseFontFaceInfo& face) {
                                 return face.pointSize == pack->pointSize && face.style == style;
                               });
  return it == kTraditionalChineseFontFaces.end() ? nullptr : &*it;
}

bool loadTraditionalChineseFonts(GfxRenderer& renderer) {
  const auto* runtime = getRuntimeByFontId(kStartupTraditionalChineseFontId);
  return runtime != nullptr && loadTraditionalChineseRuntime(renderer, *runtime, true);
}

bool loadTraditionalChineseFont(GfxRenderer& renderer, uint8_t fontSize) {
  const auto* runtime = getRuntimeByFontSize(fontSize);
  return runtime != nullptr && loadTraditionalChineseFontById(renderer, runtime->info->fontId);
}

bool loadTraditionalChineseFontById(GfxRenderer& renderer, int fontId) {
  const auto* runtime = getLoadableRuntimeByFontId(fontId);
  if (runtime == nullptr) {
    return false;
  }
  if (loadTraditionalChineseRuntime(renderer, *runtime)) {
    return true;
  }
  const int fallbackFontId = runtime->info->fontId == kReaderExtraSmallTraditionalChineseFontId
                                 ? kSmallTraditionalChineseFontId
                                 : kReaderExtraSmallTraditionalChineseFontId;
  const auto* fallback = getRuntimeByFontId(fallbackFontId);
  if (fallback != nullptr) {
    if (Storage.exists(fallback->info->path)) {
      return loadTraditionalChineseRuntime(renderer, *fallback);
    }
    if (runtime->info->fontId != kStartupTraditionalChineseFontId) {
      fallback = getRuntimeByFontId(kStartupTraditionalChineseFontId);
      return fallback != nullptr && loadTraditionalChineseRuntime(renderer, *fallback);
    }
  } else if (runtime->info->fontId != kStartupTraditionalChineseFontId) {
    fallback = getRuntimeByFontId(kStartupTraditionalChineseFontId);
    if (fallback != nullptr) {
      return loadTraditionalChineseRuntime(renderer, *fallback);
    }
  }
  return false;
}

int getCurrentTraditionalChineseFontId() {
  const auto* runtime = getCurrentLoadedRuntime();
  return runtime == nullptr ? 0 : runtime->info->fontId;
}

bool isTraditionalChineseFontLoadSupportedById(int fontId) { return isAllowedTraditionalChineseFontId(fontId); }

bool isTraditionalChineseFontInstalled(uint8_t fontSize) {
  const auto* pack = getTraditionalChineseFontPack(fontSize);
  return pack != nullptr && Storage.exists(pack->path);
}

bool isTraditionalChineseFontFaceInstalled(uint8_t fontSize, EpdFontFamily::Style style) {
  const auto* face = getTraditionalChineseFontFace(fontSize, style);
  return face != nullptr && Storage.exists(face->path);
}

bool isTraditionalChineseFontInstalledById(int fontId) {
  const auto* pack = getTraditionalChineseFontPackById(fontId);
  return pack != nullptr && Storage.exists(pack->path);
}

bool isTraditionalChineseFontFaceInstalledById(int fontId, EpdFontFamily::Style style) {
  const auto* face = getTraditionalChineseFontFaceById(fontId, style);
  return face != nullptr && Storage.exists(face->path);
}

bool isTraditionalChineseFontLoaded(uint8_t fontSize) {
  const auto* runtime = getRuntimeByFontSize(fontSize);
  return runtime != nullptr && runtime->family->loaded;
}

bool isTraditionalChineseFontFaceLoaded(uint8_t fontSize, EpdFontFamily::Style style) {
  const auto* pack = getTraditionalChineseFontPack(fontSize);
  if (pack == nullptr) {
    return false;
  }
  return isTraditionalChineseFontFaceLoadedById(pack->fontId, style);
}

bool isTraditionalChineseFontLoadedById(int fontId) {
  const auto* runtime = getRuntimeByFontId(fontId);
  return runtime != nullptr && runtime->family->loaded;
}

bool isTraditionalChineseFontFaceLoadedById(int fontId, EpdFontFamily::Style style) {
  for (const auto& runtime : kPackRuntimes) {
    if (runtime.info->fontId != fontId) continue;
    switch (style & 0x03) {
      case EpdFontFamily::REGULAR:
        return runtime.family->regular->loaded();
      case EpdFontFamily::BOLD:
        return runtime.family->bold->loaded();
      case EpdFontFamily::ITALIC:
        return runtime.family->italic->loaded();
      case EpdFontFamily::BOLD_ITALIC:
        return runtime.family->boldItalic->loaded();
      default:
        return false;
    }
  }
  return false;
}

size_t countLoadedTraditionalChineseFonts() {
  return std::count_if(kPackRuntimes.begin(), kPackRuntimes.end(),
                       [](const PackRuntime& runtime) { return runtime.family->loaded; });
}

size_t countInstalledTraditionalChineseStyles(uint8_t fontSize) {
  const auto* pack = getTraditionalChineseFontPack(fontSize);
  return pack == nullptr ? 0 : countInstalledTraditionalChineseStylesById(pack->fontId);
}

size_t countLoadedTraditionalChineseStyles(uint8_t fontSize) {
  const auto* pack = getTraditionalChineseFontPack(fontSize);
  return pack == nullptr ? 0 : countLoadedTraditionalChineseStylesById(pack->fontId);
}

size_t countInstalledTraditionalChineseStylesById(int fontId) {
  const auto* pack = getTraditionalChineseFontPackById(fontId);
  if (pack == nullptr) {
    return 0;
  }
  return std::count_if(kTraditionalChineseFontFaces.begin(), kTraditionalChineseFontFaces.end(),
                       [pack](const TraditionalChineseFontFaceInfo& face) {
                         return face.pointSize == pack->pointSize && Storage.exists(face.path);
                       });
}

size_t countLoadedTraditionalChineseStylesById(int fontId) {
  const auto* pack = getTraditionalChineseFontPackById(fontId);
  if (pack == nullptr) {
    return 0;
  }
  return std::count_if(kTraditionalChineseFontFaces.begin(), kTraditionalChineseFontFaces.end(),
                       [fontId, pack](const TraditionalChineseFontFaceInfo& face) {
                         return face.pointSize == pack->pointSize &&
                                isTraditionalChineseFontFaceLoadedById(fontId, face.style);
                       });
}

}  // namespace StorageFontRegistry
