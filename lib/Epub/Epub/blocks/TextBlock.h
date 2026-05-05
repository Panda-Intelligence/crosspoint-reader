#pragma once
#include <EpdFontFamily.h>
#include <HalStorage.h>

#include <memory>
#include <string>
#include <vector>

#include "Block.h"
#include "BlockStyle.h"

// Represents a line of text on a page
class TextBlock final : public Block {
 private:
  std::vector<std::string> words;
  std::vector<int16_t> wordXpos;
  std::vector<EpdFontFamily::Style> wordStyles;
  std::vector<bool> wordContinues;
  BlockStyle blockStyle;
  bool verticalLayout = false;
  uint16_t blockIndex = 0;
  uint16_t tokenStartIndex = 0;

 public:
  explicit TextBlock(std::vector<std::string> words, std::vector<int16_t> word_xpos,
                     std::vector<EpdFontFamily::Style> word_styles, std::vector<bool> word_continues,
                     const BlockStyle& blockStyle = BlockStyle(), const bool verticalLayout = false,
                     const uint16_t blockIndex = 0, const uint16_t tokenStartIndex = 0)
      : words(std::move(words)),
        wordXpos(std::move(word_xpos)),
        wordStyles(std::move(word_styles)),
        wordContinues(std::move(word_continues)),
        blockStyle(blockStyle),
        verticalLayout(verticalLayout),
        blockIndex(blockIndex),
        tokenStartIndex(tokenStartIndex) {}
  ~TextBlock() override = default;
  void setBlockStyle(const BlockStyle& blockStyle) { this->blockStyle = blockStyle; }
  const BlockStyle& getBlockStyle() const { return blockStyle; }
  const std::vector<std::string>& getWords() const { return words; }
  const std::vector<int16_t>& getWordXPositions() const { return wordXpos; }
  const std::vector<EpdFontFamily::Style>& getWordStyles() const { return wordStyles; }
  const std::vector<bool>& getWordContinues() const { return wordContinues; }
  bool isVerticalLayout() const { return verticalLayout; }
  uint16_t getBlockIndex() const { return blockIndex; }
  uint16_t getTokenStartIndex() const { return tokenStartIndex; }
  bool isEmpty() override { return words.empty(); }
  size_t wordCount() const { return words.size(); }
  // given a renderer works out where to break the words into lines
  void render(const GfxRenderer& renderer, int fontId, int x, int y) const;
  BlockType getType() override { return TEXT_BLOCK; }
  bool serialize(FsFile& file) const;
  static std::unique_ptr<TextBlock> deserialize(FsFile& file);
};
