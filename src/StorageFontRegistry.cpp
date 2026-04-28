#include "StorageFontRegistry.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "ExternalFontIds.h"
#include "FontCacheManager.h"
#include "GfxRenderer.h"

namespace {
constexpr char FONT_MAGIC[] = {'E', 'P', 'F', '2'};
constexpr uint16_t FONT_PACK_VERSION = 2;

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

StorageFontPack tc12Pack;
StorageFontPack tc14Pack;
StorageFontPack tc16Pack;
StorageFontPack tc18Pack;
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

StorageFontFamilySlot tc12Family{&tc12Pack, &tc12BoldPack, &tc12ItalicPack, &tc12BoldItalicPack};
StorageFontFamilySlot tc14Family{&tc14Pack, &tc14BoldPack, &tc14ItalicPack, &tc14BoldItalicPack};
StorageFontFamilySlot tc16Family{&tc16Pack, &tc16BoldPack, &tc16ItalicPack, &tc16BoldItalicPack};
StorageFontFamilySlot tc18Family{&tc18Pack, &tc18BoldPack, &tc18ItalicPack, &tc18BoldItalicPack};

constexpr std::array<TraditionalChineseFontPackInfo, 4> kTraditionalChineseFontPacks = {{
    {"Noto Sans CJK 12", "/.mofei/fonts/notosans_tc_12.epf", CrossPointSettings::SMALL, NOTOSANS_TC_12_FONT_ID},
    {"Noto Sans CJK 14", "/.mofei/fonts/notosans_tc_14.epf", CrossPointSettings::MEDIUM, NOTOSANS_TC_14_FONT_ID},
    {"Noto Sans CJK 16", "/.mofei/fonts/notosans_tc_16.epf", CrossPointSettings::LARGE, NOTOSANS_TC_16_FONT_ID},
    {"Noto Sans CJK 18", "/.mofei/fonts/notosans_tc_18.epf", CrossPointSettings::EXTRA_LARGE, NOTOSANS_TC_18_FONT_ID},
}};

constexpr std::array<TraditionalChineseFontFaceInfo, 16> kTraditionalChineseFontFaces = {{
    {"Noto Sans CJK 12", "/.mofei/fonts/notosans_tc_12.epf", CrossPointSettings::SMALL, EpdFontFamily::REGULAR},
    {"Noto Sans CJK 12 Bold", "/.mofei/fonts/notosans_tc_12_bold.epf", CrossPointSettings::SMALL, EpdFontFamily::BOLD},
    {"Noto Sans CJK 12 Italic", "/.mofei/fonts/notosans_tc_12_italic.epf", CrossPointSettings::SMALL,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 12 Bold Italic", "/.mofei/fonts/notosans_tc_12_bolditalic.epf", CrossPointSettings::SMALL,
     EpdFontFamily::BOLD_ITALIC},
    {"Noto Sans CJK 14", "/.mofei/fonts/notosans_tc_14.epf", CrossPointSettings::MEDIUM, EpdFontFamily::REGULAR},
    {"Noto Sans CJK 14 Bold", "/.mofei/fonts/notosans_tc_14_bold.epf", CrossPointSettings::MEDIUM, EpdFontFamily::BOLD},
    {"Noto Sans CJK 14 Italic", "/.mofei/fonts/notosans_tc_14_italic.epf", CrossPointSettings::MEDIUM,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 14 Bold Italic", "/.mofei/fonts/notosans_tc_14_bolditalic.epf", CrossPointSettings::MEDIUM,
     EpdFontFamily::BOLD_ITALIC},
    {"Noto Sans CJK 16", "/.mofei/fonts/notosans_tc_16.epf", CrossPointSettings::LARGE, EpdFontFamily::REGULAR},
    {"Noto Sans CJK 16 Bold", "/.mofei/fonts/notosans_tc_16_bold.epf", CrossPointSettings::LARGE, EpdFontFamily::BOLD},
    {"Noto Sans CJK 16 Italic", "/.mofei/fonts/notosans_tc_16_italic.epf", CrossPointSettings::LARGE,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 16 Bold Italic", "/.mofei/fonts/notosans_tc_16_bolditalic.epf", CrossPointSettings::LARGE,
     EpdFontFamily::BOLD_ITALIC},
    {"Noto Sans CJK 18", "/.mofei/fonts/notosans_tc_18.epf", CrossPointSettings::EXTRA_LARGE, EpdFontFamily::REGULAR},
    {"Noto Sans CJK 18 Bold", "/.mofei/fonts/notosans_tc_18_bold.epf", CrossPointSettings::EXTRA_LARGE,
     EpdFontFamily::BOLD},
    {"Noto Sans CJK 18 Italic", "/.mofei/fonts/notosans_tc_18_italic.epf", CrossPointSettings::EXTRA_LARGE,
     EpdFontFamily::ITALIC},
    {"Noto Sans CJK 18 Bold Italic", "/.mofei/fonts/notosans_tc_18_bolditalic.epf", CrossPointSettings::EXTRA_LARGE,
     EpdFontFamily::BOLD_ITALIC},
}};

struct PackRuntime {
  const TraditionalChineseFontPackInfo* info;
  StorageFontFamilySlot* family;
};

const std::array<PackRuntime, 4> kPackRuntimes = {{
    {&kTraditionalChineseFontPacks[0], &tc12Family},
    {&kTraditionalChineseFontPacks[1], &tc14Family},
    {&kTraditionalChineseFontPacks[2], &tc16Family},
    {&kTraditionalChineseFontPacks[3], &tc18Family},
}};

template <typename T>
bool readArray(FsFile& file, std::vector<T>& out, size_t count) {
  out.resize(count);
  if (count == 0) return true;
  const size_t bytes = sizeof(T) * count;
  return file.read(out.data(), bytes) == static_cast<int>(bytes);
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
    return false;
  }

  FontPackHeader header = {};
  if (file.read(&header, sizeof(header)) != static_cast<int>(sizeof(header))) {
    return false;
  }

  if (memcmp(header.magic, FONT_MAGIC, sizeof(FONT_MAGIC)) != 0 || header.version != FONT_PACK_VERSION) {
    LOG_ERR("TCFONT", "Invalid font pack header: %s", path);
    return false;
  }

  bitmap_.resize(header.bitmapSize);
  if (header.bitmapSize > 0 &&
      file.read(bitmap_.data(), header.bitmapSize) != static_cast<int>(header.bitmapSize)) {
    return false;
  }

  std::vector<PackedGlyph> rawGlyphs;
  std::vector<PackedInterval> rawIntervals;
  std::vector<PackedGroup> rawGroups;

  if (!readArray(file, rawGlyphs, header.glyphCount)) return false;
  if (!readArray(file, rawIntervals, header.intervalCount)) return false;
  if (!readArray(file, rawGroups, header.groupCount)) return false;
  if (!readArray(file, kernLeftClasses_, header.kernLeftCount)) return false;
  if (!readArray(file, kernRightClasses_, header.kernRightCount)) return false;
  if (!readArray(file, kernMatrix_, header.kernMatrixCount)) return false;
  if (!readArray(file, ligaturePairs_, header.ligatureCount)) return false;

  glyphs_.resize(rawGlyphs.size());
  for (size_t i = 0; i < rawGlyphs.size(); i++) {
    glyphs_[i] = {rawGlyphs[i].width,  rawGlyphs[i].height, rawGlyphs[i].advanceX, rawGlyphs[i].left,
                  rawGlyphs[i].top,    rawGlyphs[i].dataLength, rawGlyphs[i].dataOffset};
  }

  intervals_.resize(rawIntervals.size());
  for (size_t i = 0; i < rawIntervals.size(); i++) {
    intervals_[i] = {rawIntervals[i].first, rawIntervals[i].last, rawIntervals[i].offset};
  }

  groups_.resize(rawGroups.size());
  for (size_t i = 0; i < rawGroups.size(); i++) {
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

const std::array<TraditionalChineseFontPackInfo, 4>& getTraditionalChineseFontPacks() {
  return kTraditionalChineseFontPacks;
}

const std::array<TraditionalChineseFontFaceInfo, 16>& getTraditionalChineseFontFaces() {
  return kTraditionalChineseFontFaces;
}

const TraditionalChineseFontPackInfo* getTraditionalChineseFontPack(uint8_t fontSize) {
  for (const auto& pack : kTraditionalChineseFontPacks) {
    if (pack.size == fontSize) {
      return &pack;
    }
  }
  return nullptr;
}

const TraditionalChineseFontFaceInfo* getTraditionalChineseFontFace(uint8_t fontSize, EpdFontFamily::Style style) {
  for (const auto& face : kTraditionalChineseFontFaces) {
    if (face.size == fontSize && face.style == style) {
      return &face;
    }
  }
  return nullptr;
}

bool loadTraditionalChineseFonts(GfxRenderer& renderer) {
  Storage.mkdir("/.mofei");
  Storage.mkdir("/.mofei/fonts");

  bool anyLoaded = false;
  for (const auto& runtime : kPackRuntimes) {
    runtime.family->reset();

    const auto* regular = getTraditionalChineseFontFace(runtime.info->size, EpdFontFamily::REGULAR);
    const auto* bold = getTraditionalChineseFontFace(runtime.info->size, EpdFontFamily::BOLD);
    const auto* italic = getTraditionalChineseFontFace(runtime.info->size, EpdFontFamily::ITALIC);
    const auto* boldItalic = getTraditionalChineseFontFace(runtime.info->size, EpdFontFamily::BOLD_ITALIC);

    if (regular != nullptr && runtime.family->regular->load(regular->path)) {
      if (bold != nullptr) runtime.family->bold->load(bold->path);
      if (italic != nullptr) runtime.family->italic->load(italic->path);
      if (boldItalic != nullptr) runtime.family->boldItalic->load(boldItalic->path);

      runtime.family->family = std::make_unique<EpdFontFamily>(
          &runtime.family->regular->font(),
          runtime.family->bold->loaded() ? &runtime.family->bold->font() : nullptr,
          runtime.family->italic->loaded() ? &runtime.family->italic->font() : nullptr,
          runtime.family->boldItalic->loaded() ? &runtime.family->boldItalic->font() : nullptr);
      runtime.family->loaded = true;
      renderer.insertFont(runtime.info->fontId, *runtime.family->family);
      LOG_INF("TCFONT", "Loaded multilingual font family: %s", runtime.info->path);
      anyLoaded = true;
    } else {
      renderer.removeFont(runtime.info->fontId);
      LOG_DBG("TCFONT", "Multilingual font family not found: %s", runtime.info->path);
    }
  }
  if (auto* cacheManager = renderer.getFontCacheManager()) {
    cacheManager->clearCache();
  }
  return anyLoaded;
}

bool isTraditionalChineseFontInstalled(uint8_t fontSize) {
  const auto* pack = getTraditionalChineseFontPack(fontSize);
  return pack != nullptr && Storage.exists(pack->path);
}

bool isTraditionalChineseFontFaceInstalled(uint8_t fontSize, EpdFontFamily::Style style) {
  const auto* face = getTraditionalChineseFontFace(fontSize, style);
  return face != nullptr && Storage.exists(face->path);
}

bool isTraditionalChineseFontLoaded(uint8_t fontSize) {
  for (const auto& runtime : kPackRuntimes) {
    if (runtime.info->size == fontSize) return runtime.family->loaded;
  }
  return false;
}

bool isTraditionalChineseFontFaceLoaded(uint8_t fontSize, EpdFontFamily::Style style) {
  for (const auto& runtime : kPackRuntimes) {
    if (runtime.info->size != fontSize) continue;
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
  size_t count = 0;
  for (const auto& runtime : kPackRuntimes) {
    if (runtime.family->loaded) {
      count++;
    }
  }
  return count;
}

size_t countInstalledTraditionalChineseStyles(uint8_t fontSize) {
  size_t count = 0;
  for (const auto& face : kTraditionalChineseFontFaces) {
    if (face.size == fontSize && Storage.exists(face.path)) {
      count++;
    }
  }
  return count;
}

size_t countLoadedTraditionalChineseStyles(uint8_t fontSize) {
  size_t count = 0;
  for (const auto& face : kTraditionalChineseFontFaces) {
    if (face.size == fontSize && isTraditionalChineseFontFaceLoaded(fontSize, face.style)) {
      count++;
    }
  }
  return count;
}

}  // namespace StorageFontRegistry
