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
  std::unique_ptr<EpdFontFamily> family_;
  bool loaded_ = false;
  void reset();

 public:
  bool load(const char* path);
  bool loaded() const { return loaded_; }
  const EpdFontFamily& family() const { return *family_; }
};

struct TraditionalChineseFontPackInfo {
  const char* name;
  const char* path;
  CrossPointSettings::FONT_SIZE size;
  int fontId;
};

namespace StorageFontRegistry {

const std::array<TraditionalChineseFontPackInfo, 4>& getTraditionalChineseFontPacks();
const TraditionalChineseFontPackInfo* getTraditionalChineseFontPack(uint8_t fontSize);
bool loadTraditionalChineseFonts(GfxRenderer& renderer);
bool isTraditionalChineseFontInstalled(uint8_t fontSize);
bool isTraditionalChineseFontLoaded(uint8_t fontSize);
size_t countLoadedTraditionalChineseFonts();

}  // namespace StorageFontRegistry
