#include "StorageFontRegistry.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

#include "CrossPointSettings.h"
#include "ExternalFontIds.h"
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

struct PackMeta {
  CrossPointSettings::FONT_SIZE size;
  const char* path;
  int fontId;
  StorageFontPack* pack;
};

PackMeta packMeta[] = {
    {CrossPointSettings::SMALL, "/.mofei/fonts/notosans_tc_12.epf", NOTOSANS_TC_12_FONT_ID, &tc12Pack},
    {CrossPointSettings::MEDIUM, "/.mofei/fonts/notosans_tc_14.epf", NOTOSANS_TC_14_FONT_ID, &tc14Pack},
    {CrossPointSettings::LARGE, "/.mofei/fonts/notosans_tc_16.epf", NOTOSANS_TC_16_FONT_ID, &tc16Pack},
    {CrossPointSettings::EXTRA_LARGE, "/.mofei/fonts/notosans_tc_18.epf", NOTOSANS_TC_18_FONT_ID, &tc18Pack},
};

template <typename T>
bool readArray(FsFile& file, std::vector<T>& out, size_t count) {
  out.resize(count);
  if (count == 0) return true;
  const size_t bytes = sizeof(T) * count;
  return file.read(out.data(), bytes) == static_cast<int>(bytes);
}
}  // namespace

bool StorageFontPack::load(const char* path) {
  loaded_ = false;

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
  family_ = std::make_unique<EpdFontFamily>(font_.get());
  loaded_ = true;
  return true;
}

namespace StorageFontRegistry {

bool loadTraditionalChineseFonts(GfxRenderer& renderer) {
  Storage.mkdir("/.mofei");
  Storage.mkdir("/.mofei/fonts");

  bool anyLoaded = false;
  for (auto& meta : packMeta) {
    if (meta.pack->load(meta.path)) {
      renderer.insertFont(meta.fontId, meta.pack->family());
      LOG_INF("TCFONT", "Loaded Traditional Chinese font pack: %s", meta.path);
      anyLoaded = true;
    } else {
      LOG_DBG("TCFONT", "Traditional Chinese font pack not found: %s", meta.path);
    }
  }
  return anyLoaded;
}

bool isTraditionalChineseFontLoaded(uint8_t fontSize) {
  for (const auto& meta : packMeta) {
    if (meta.size == fontSize) return meta.pack->loaded();
  }
  return false;
}

}  // namespace StorageFontRegistry
