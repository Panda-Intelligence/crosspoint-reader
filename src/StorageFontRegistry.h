#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <EpdFont.h>
#include <EpdFontData.h>
#include <EpdFontFamily.h>

#include "CrossPointSettings.h"

class GfxRenderer;

class StorageFontPack {
  std::vector<uint8_t> bitmap_;
  std::vector<EpdGlyph> glyphs_;
  std::vector<EpdUnicodeInterval> intervals_;
  std::vector<EpdFontGroup> groups_;
  std::vector<EpdKernClassEntry> kernLeftClasses_;
  std::vector<EpdKernClassEntry> kernRightClasses_;
  std::vector<int8_t> kernMatrix_;
  std::vector<EpdLigaturePair> ligaturePairs_;
  EpdFontData data_ = {};
  std::unique_ptr<EpdFont> font_;
  bool loaded_ = false;

 public:
  void reset();
  bool load(const char* path);
  bool loaded() const { return loaded_; }
  const EpdFont& font() const { return *font_; }
};

struct TraditionalChineseFontPackInfo {
  const char* name;
  const char* path;
  CrossPointSettings::FONT_SIZE size;
  int fontId;
};

struct TraditionalChineseFontFaceInfo {
  const char* name;
  const char* path;
  CrossPointSettings::FONT_SIZE size;
  EpdFontFamily::Style style;
};

namespace StorageFontRegistry {

const std::array<TraditionalChineseFontPackInfo, 4>& getTraditionalChineseFontPacks();
const std::array<TraditionalChineseFontFaceInfo, 16>& getTraditionalChineseFontFaces();
const TraditionalChineseFontPackInfo* getTraditionalChineseFontPack(uint8_t fontSize);
const TraditionalChineseFontFaceInfo* getTraditionalChineseFontFace(uint8_t fontSize, EpdFontFamily::Style style);
bool loadTraditionalChineseFonts(GfxRenderer& renderer);
bool isTraditionalChineseFontInstalled(uint8_t fontSize);
bool isTraditionalChineseFontFaceInstalled(uint8_t fontSize, EpdFontFamily::Style style);
bool isTraditionalChineseFontLoaded(uint8_t fontSize);
bool isTraditionalChineseFontFaceLoaded(uint8_t fontSize, EpdFontFamily::Style style);
size_t countLoadedTraditionalChineseFonts();
size_t countInstalledTraditionalChineseStyles(uint8_t fontSize);
size_t countLoadedTraditionalChineseStyles(uint8_t fontSize);

}  // namespace StorageFontRegistry
