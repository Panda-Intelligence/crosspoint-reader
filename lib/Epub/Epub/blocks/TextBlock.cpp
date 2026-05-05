#include "TextBlock.h"

#include <GfxRenderer.h>
#include <Logging.h>
#include <Serialization.h>

void TextBlock::render(const GfxRenderer& renderer, const int fontId, const int x, const int y) const {
  // Validate iterator bounds before rendering
  if (words.size() != wordXpos.size() || words.size() != wordStyles.size() || words.size() != wordContinues.size()) {
    LOG_ERR("TXB", "Render skipped: size mismatch (words=%u, xpos=%u, styles=%u, continues=%u)\n",
            (uint32_t)words.size(), (uint32_t)wordXpos.size(), (uint32_t)wordStyles.size(),
            (uint32_t)wordContinues.size());
    return;
  }

  for (size_t i = 0; i < words.size(); i++) {
    const int wordX = verticalLayout ? x : wordXpos[i] + x;
    const int wordY = verticalLayout ? y + wordXpos[i] : y;
    const EpdFontFamily::Style currentStyle = wordStyles[i];
    if (verticalLayout) {
      renderer.drawTextRotated90CW(fontId, wordX, wordY, words[i].c_str(), true, currentStyle);
    } else {
      renderer.drawText(fontId, wordX, wordY, words[i].c_str(), true, currentStyle);
    }

    if ((currentStyle & EpdFontFamily::UNDERLINE) != 0) {
      const std::string& w = words[i];
      const int fullWordWidth =
          verticalLayout ? renderer.getTextHeight(fontId) : renderer.getTextWidth(fontId, w.c_str(), currentStyle);
      const int underlineY = wordY + renderer.getFontAscenderSize(fontId) + 2;

      int startX = wordX;
      int underlineWidth = fullWordWidth;

      // if word starts with em-space ("\xe2\x80\x83"), account for the additional indent before drawing the line
      if (w.size() >= 3 && static_cast<uint8_t>(w[0]) == 0xE2 && static_cast<uint8_t>(w[1]) == 0x80 &&
          static_cast<uint8_t>(w[2]) == 0x83) {
        const char* visiblePtr = w.c_str() + 3;
        const int prefixWidth = renderer.getTextAdvanceX(fontId, "\xe2\x80\x83", currentStyle);
        const int visibleWidth = renderer.getTextWidth(fontId, visiblePtr, currentStyle);
        startX = wordX + prefixWidth;
        underlineWidth = visibleWidth;
      }

      if (!verticalLayout) {
        renderer.drawLine(startX, underlineY, startX + underlineWidth, underlineY, true);
      }
    }
  }
}

bool TextBlock::serialize(FsFile& file) const {
  if (words.size() != wordXpos.size() || words.size() != wordStyles.size() || words.size() != wordContinues.size()) {
    LOG_ERR("TXB", "Serialization failed: size mismatch (words=%u, xpos=%u, styles=%u, continues=%u)\n", words.size(),
            wordXpos.size(), wordStyles.size(), wordContinues.size());
    return false;
  }

  // Word data
  serialization::writePod(file, static_cast<uint16_t>(words.size()));
  for (const auto& w : words) serialization::writeString(file, w);
  for (auto x : wordXpos) serialization::writePod(file, x);
  for (auto s : wordStyles) serialization::writePod(file, s);
  for (const bool c : wordContinues) serialization::writePod(file, c);
  serialization::writePod(file, verticalLayout);
  serialization::writePod(file, blockIndex);
  serialization::writePod(file, tokenStartIndex);

  // Style (alignment + margins/padding/indent)
  serialization::writePod(file, blockStyle.alignment);
  serialization::writePod(file, blockStyle.textAlignDefined);
  serialization::writePod(file, blockStyle.marginTop);
  serialization::writePod(file, blockStyle.marginBottom);
  serialization::writePod(file, blockStyle.marginLeft);
  serialization::writePod(file, blockStyle.marginRight);
  serialization::writePod(file, blockStyle.paddingTop);
  serialization::writePod(file, blockStyle.paddingBottom);
  serialization::writePod(file, blockStyle.paddingLeft);
  serialization::writePod(file, blockStyle.paddingRight);
  serialization::writePod(file, blockStyle.textIndent);
  serialization::writePod(file, blockStyle.textIndentDefined);

  return true;
}

std::unique_ptr<TextBlock> TextBlock::deserialize(FsFile& file) {
  uint16_t wc;
  std::vector<std::string> words;
  std::vector<int16_t> wordXpos;
  std::vector<EpdFontFamily::Style> wordStyles;
  std::vector<bool> wordContinues;
  BlockStyle blockStyle;
  bool verticalLayout = false;
  uint16_t blockIndex = 0;
  uint16_t tokenStartIndex = 0;

  // Word count
  serialization::readPod(file, wc);

  // Sanity check: prevent allocation of unreasonably large vectors (max 10000 words per block)
  if (wc > 10000) {
    LOG_ERR("TXB", "Deserialization failed: word count %u exceeds maximum", wc);
    return nullptr;
  }

  // Word data
  words.resize(wc);
  wordXpos.resize(wc);
  wordStyles.resize(wc);
  wordContinues.resize(wc);
  for (auto& w : words) serialization::readString(file, w);
  for (auto& x : wordXpos) serialization::readPod(file, x);
  for (auto& s : wordStyles) serialization::readPod(file, s);
  for (size_t i = 0; i < wordContinues.size(); ++i) {
    bool continued = false;
    serialization::readPod(file, continued);
    wordContinues[i] = continued;
  }
  serialization::readPod(file, verticalLayout);
  serialization::readPod(file, blockIndex);
  serialization::readPod(file, tokenStartIndex);

  // Style (alignment + margins/padding/indent)
  serialization::readPod(file, blockStyle.alignment);
  serialization::readPod(file, blockStyle.textAlignDefined);
  serialization::readPod(file, blockStyle.marginTop);
  serialization::readPod(file, blockStyle.marginBottom);
  serialization::readPod(file, blockStyle.marginLeft);
  serialization::readPod(file, blockStyle.marginRight);
  serialization::readPod(file, blockStyle.paddingTop);
  serialization::readPod(file, blockStyle.paddingBottom);
  serialization::readPod(file, blockStyle.paddingLeft);
  serialization::readPod(file, blockStyle.paddingRight);
  serialization::readPod(file, blockStyle.textIndent);
  serialization::readPod(file, blockStyle.textIndentDefined);

  return std::unique_ptr<TextBlock>(new TextBlock(std::move(words), std::move(wordXpos), std::move(wordStyles),
                                                  std::move(wordContinues), blockStyle, verticalLayout, blockIndex,
                                                  tokenStartIndex));
}
