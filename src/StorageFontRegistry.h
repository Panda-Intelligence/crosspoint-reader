#pragma once

#include <EpdFont.h>
#include <EpdFontData.h>
#include <EpdFontFamily.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

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
  uint8_t pointSize;
  int fontId;
};

struct TraditionalChineseFontFaceInfo {
  const char* name;
  const char* path;
  CrossPointSettings::FONT_SIZE size;
  uint8_t pointSize;
  EpdFontFamily::Style style;
};

using TraditionalChineseFontPacks = std::array<TraditionalChineseFontPackInfo, 6>;
using TraditionalChineseFontFaces = std::array<TraditionalChineseFontFaceInfo, 24>;

namespace StorageFontRegistry {

const TraditionalChineseFontPacks& getTraditionalChineseFontPacks();
const TraditionalChineseFontFaces& getTraditionalChineseFontFaces();
const TraditionalChineseFontPackInfo* getTraditionalChineseFontPack(uint8_t fontSize);
const TraditionalChineseFontFaceInfo* getTraditionalChineseFontFace(uint8_t fontSize, EpdFontFamily::Style style);
const TraditionalChineseFontPackInfo* getTraditionalChineseFontPackById(int fontId);
const TraditionalChineseFontFaceInfo* getTraditionalChineseFontFaceById(int fontId, EpdFontFamily::Style style);
bool loadTraditionalChineseFonts(GfxRenderer& renderer);
bool loadTraditionalChineseFont(GfxRenderer& renderer, uint8_t fontSize);
bool loadTraditionalChineseFontById(GfxRenderer& renderer, int fontId);
int getCurrentTraditionalChineseFontId();
bool isTraditionalChineseFontLoadSupportedById(int fontId);
bool isTraditionalChineseFontInstalled(uint8_t fontSize);
bool isTraditionalChineseFontFaceInstalled(uint8_t fontSize, EpdFontFamily::Style style);
bool isTraditionalChineseFontInstalledById(int fontId);
bool isTraditionalChineseFontFaceInstalledById(int fontId, EpdFontFamily::Style style);
bool isTraditionalChineseFontLoaded(uint8_t fontSize);
bool isTraditionalChineseFontFaceLoaded(uint8_t fontSize, EpdFontFamily::Style style);
bool isTraditionalChineseFontLoadedById(int fontId);
bool isTraditionalChineseFontFaceLoadedById(int fontId, EpdFontFamily::Style style);
size_t countLoadedTraditionalChineseFonts();
size_t countInstalledTraditionalChineseStyles(uint8_t fontSize);
size_t countLoadedTraditionalChineseStyles(uint8_t fontSize);
size_t countInstalledTraditionalChineseStylesById(int fontId);
size_t countLoadedTraditionalChineseStylesById(int fontId);

}  // namespace StorageFontRegistry
