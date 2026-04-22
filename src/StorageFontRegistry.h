#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <EpdFont.h>
#include <EpdFontData.h>
#include <EpdFontFamily.h>

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

 public:
  bool load(const char* path);
  bool loaded() const { return loaded_; }
  const EpdFontFamily& family() const { return *family_; }
};

namespace StorageFontRegistry {

bool loadTraditionalChineseFonts(GfxRenderer& renderer);
bool isTraditionalChineseFontLoaded(uint8_t fontSize);

}  // namespace StorageFontRegistry
