#include "TxtReaderActivity.h"

#include <FontCacheManager.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Serialization.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "MappedInputManager.h"
#include "ReaderUtils.h"
#include "RecentBooksStore.h"
#include "StorageFontRegistry.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "ui_font_manager.h"
#include "util/TouchHitTest.h"

namespace {
constexpr size_t CHUNK_SIZE = 8 * 1024;  // 8KB chunk for reading
// Cache file magic and version
constexpr uint32_t CACHE_MAGIC = 0x54585449;  // "TXTI"
constexpr uint8_t CACHE_VERSION = 3;          // cache 格式或分页边界变化时递增

constexpr size_t cacheHeaderSize() {
  return sizeof(CACHE_MAGIC) + sizeof(CACHE_VERSION) + sizeof(uint32_t) * 2 + sizeof(int32_t) * 4 + sizeof(uint8_t);
}

constexpr const char* kIndexCacheFileName = "/index.bin";
constexpr const char* kIndexCacheTempFileName = "/index.bin.tmp";
constexpr const char* kIndexCacheBackupFileName = "/index.bin.bak";
constexpr unsigned long kIndexYieldIntervalMs = 20;
constexpr size_t kDirectFitCheckMaxBytes = 512;

bool removeIfExists(const std::string& path) { return !Storage.exists(path.c_str()) || Storage.remove(path.c_str()); }

bool promoteIndexCacheFile(const std::string& tempPath, const std::string& finalPath, const std::string& backupPath) {
  const bool hadExistingCache = Storage.exists(finalPath.c_str());
  if (hadExistingCache) {
    if (!removeIfExists(backupPath)) {
      LOG_ERR("TRS", "Failed to clear old page index backup");
      return false;
    }
    if (!Storage.rename(finalPath.c_str(), backupPath.c_str())) {
      LOG_ERR("TRS", "Failed to back up existing page index cache");
      return false;
    }
  }

  if (Storage.rename(tempPath.c_str(), finalPath.c_str())) {
    if (hadExistingCache && !removeIfExists(backupPath)) {
      LOG_ERR("TRS", "Failed to remove page index backup after promote");
    }
    return true;
  }

  LOG_ERR("TRS", "Failed to promote temp page index cache");
  if (hadExistingCache && !Storage.rename(backupPath.c_str(), finalPath.c_str())) {
    LOG_ERR("TRS", "Failed to restore previous page index cache after promote failure");
  }
  return false;
}

void yieldDuringIndexing(unsigned long& lastYieldMs) {
  const unsigned long now = millis();
  if (now - lastYieldMs >= kIndexYieldIntervalMs) {
    yield();
    vTaskDelay(1);
    lastYieldMs = now;
  }
}

void logTxtIndexMemory(const char* event, const size_t offset, const int totalPages, const size_t fileSize) {
  LOG_INF("TXTINDEX_MEM", "%s offset=%zu pages=%d file=%zu heap_free=%u heap_max=%u psram_free=%u psram_max=%u", event,
          offset, totalPages, fileSize, ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getFreePsram(),
          ESP.getMaxAllocPsram());
}

size_t utf8BoundaryStep(const char* text, const size_t length, const size_t pos) {
  if (text == nullptr || pos >= length) {
    return 0;
  }

  const auto lead = static_cast<uint8_t>(text[pos]);
  size_t expected = 1;
  if (lead < 0x80) {
    expected = 1;
  } else if ((lead & 0xE0) == 0xC0) {
    expected = 2;
  } else if ((lead & 0xF0) == 0xE0) {
    expected = 3;
  } else if ((lead & 0xF8) == 0xF0) {
    expected = 4;
  } else {
    return 1;
  }

  if (pos + expected > length) {
    return 1;
  }

  for (size_t i = 1; i < expected; ++i) {
    if ((static_cast<uint8_t>(text[pos + i]) & 0xC0) != 0x80) {
      return 1;
    }
  }
  return expected;
}

size_t completeUtf8PrefixLength(const uint8_t* text, const size_t length) {
  if (text == nullptr || length == 0) {
    return 0;
  }

  size_t pos = 0;
  size_t lastComplete = 0;
  while (pos < length) {
    const uint8_t lead = text[pos];
    size_t expected = 1;
    if (lead < 0x80) {
      expected = 1;
    } else if ((lead & 0xE0) == 0xC0) {
      expected = 2;
    } else if ((lead & 0xF0) == 0xE0) {
      expected = 3;
    } else if ((lead & 0xF8) == 0xF0) {
      expected = 4;
    } else {
      pos++;
      lastComplete = pos;
      continue;
    }

    if (pos + expected > length) {
      break;
    }

    bool valid = true;
    for (size_t i = 1; i < expected; ++i) {
      if ((text[pos + i] & 0xC0) != 0x80) {
        valid = false;
        break;
      }
    }

    pos += valid ? expected : 1;
    lastComplete = pos;
  }
  return lastComplete;
}

void collectUtf8Boundaries(const std::string& line, std::vector<uint16_t>& boundaries, unsigned long& lastYieldMs) {
  boundaries.clear();
  boundaries.reserve(line.size());

  size_t pos = 0;
  while (pos < line.size()) {
    const size_t step = std::max<size_t>(utf8BoundaryStep(line.data(), line.size(), pos), 1);
    pos = std::min(pos + step, line.size());
    boundaries.push_back(static_cast<uint16_t>(pos));
    yieldDuringIndexing(lastYieldMs);
  }
}

size_t firstBoundaryAfter(const std::vector<uint16_t>& boundaries, const size_t bytePos) {
  const auto it = std::upper_bound(boundaries.begin(), boundaries.end(), bytePos);
  return static_cast<size_t>(it - boundaries.begin());
}

bool prefixFitsViewport(const GfxRenderer& renderer, const int fontId, const char* text, const size_t length,
                        const int maxWidth, std::string& scratch) {
  if (text == nullptr || length == 0) {
    return true;
  }
  if (maxWidth <= 0) {
    return false;
  }

  scratch.assign(text, length);
  return renderer.getTextWidth(fontId, scratch.c_str()) <= maxWidth;
}

size_t longestFittingPrefixBytes(const GfxRenderer& renderer, const int fontId, const std::string& line,
                                 const size_t startByte, const std::vector<uint16_t>& boundaries, const int maxWidth,
                                 std::string& scratch, unsigned long& lastYieldMs) {
  size_t low = firstBoundaryAfter(boundaries, startByte);
  size_t high = boundaries.size();
  size_t best = 0;

  while (low < high) {
    const size_t mid = low + (high - low) / 2;
    const size_t prefixBytes = static_cast<size_t>(boundaries[mid]) - startByte;
    if (prefixFitsViewport(renderer, fontId, line.data() + startByte, prefixBytes, maxWidth, scratch)) {
      best = prefixBytes;
      low = mid + 1;
    } else {
      high = mid;
    }
    yieldDuringIndexing(lastYieldMs);
  }

  return best;
}

size_t lastSpaceBreakInPrefix(const std::string& line, const size_t startByte, const size_t prefixBytes) {
  const size_t endByte = std::min(startByte + prefixBytes, line.size());
  for (size_t pos = endByte; pos > startByte + 1; --pos) {
    if (line[pos - 1] == ' ') {
      return pos - 1;
    }
  }
  return std::string::npos;
}

struct WrapBreak {
  size_t visibleBytes;
  size_t consumeBytes;
};

WrapBreak findWrapBreak(const GfxRenderer& renderer, const int fontId, const std::string& line, const size_t startByte,
                        const std::vector<uint16_t>& boundaries, const int maxWidth, std::string& scratch,
                        unsigned long& lastYieldMs) {
  const size_t boundaryIndex = firstBoundaryAfter(boundaries, startByte);
  if (boundaryIndex >= boundaries.size()) {
    return {0, 0};
  }

  size_t fitBytes =
      longestFittingPrefixBytes(renderer, fontId, line, startByte, boundaries, maxWidth, scratch, lastYieldMs);
  if (fitBytes == 0) {
    fitBytes = static_cast<size_t>(boundaries[boundaryIndex]) - startByte;
  }

  const size_t remainingBytes = line.size() - startByte;
  if (fitBytes >= remainingBytes) {
    return {remainingBytes, remainingBytes};
  }

  size_t visibleBytes = fitBytes;
  size_t consumeBytes = fitBytes;
  const size_t spaceBreak = lastSpaceBreakInPrefix(line, startByte, fitBytes);
  if (spaceBreak != std::string::npos) {
    visibleBytes = spaceBreak - startByte;
    consumeBytes = visibleBytes + 1;
  } else if (startByte + consumeBytes < line.size() && line[startByte + consumeBytes] == ' ') {
    consumeBytes++;
  }

  if (visibleBytes == 0) {
    visibleBytes = static_cast<size_t>(boundaries[boundaryIndex]) - startByte;
    consumeBytes = std::max(visibleBytes, consumeBytes);
  }

  return {visibleBytes, std::max<size_t>(consumeBytes, visibleBytes)};
}

int currentReaderFontId(GfxRenderer& renderer) {
  if (SETTINGS.fontFamily == CrossPointSettings::NOTOSANS_TC) {
    const int previousFontId = StorageFontRegistry::getCurrentTraditionalChineseFontId();
    StorageFontRegistry::loadTraditionalChineseFont(renderer, SETTINGS.fontSize);
    if (StorageFontRegistry::getCurrentTraditionalChineseFontId() != previousFontId) {
      updateUiFontMapping();
    }
  }
  return SETTINGS.getReaderFontId();
}
}  // namespace

void TxtReaderActivity::onEnter() {
  Activity::onEnter();

  if (!txt) {
    return;
  }

  ReaderUtils::applyOrientation(renderer, SETTINGS.orientation);

  txt->setupCacheDir();

  // Save current txt as last opened file and add to recent books
  auto filePath = txt->getPath();
  auto fileName = filePath.substr(filePath.rfind('/') + 1);
  APP_STATE.openEpubPath = filePath;
  APP_STATE.saveToFile();
  RECENT_BOOKS.addBook(filePath, fileName, "", "");

  // Trigger first update
  requestUpdate();
}

void TxtReaderActivity::onExit() {
  Activity::onExit();

  currentPageLines.clear();
  APP_STATE.readerActivityLoadCount = 0;
  APP_STATE.saveToFile();
  txt.reset();
}

void TxtReaderActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    mappedInput.suppressTouchButtonFallback();
    const auto action = TouchHitTest::readerActionForTouch(
        touchEvent, Rect{0, 0, renderer.getScreenWidth(), renderer.getScreenHeight()});

    if (action == TouchHitTest::ReaderAction::PreviousPage) {
      if (currentPage > 0) {
        currentPage--;
        requestUpdate();
      }
      return;
    }

    if (action == TouchHitTest::ReaderAction::NextPage) {
      if (currentPage < totalPages - 1) {
        currentPage++;
        requestUpdate();
      } else {
        onGoHome();
      }
      return;
    }

    return;
  }

  // Long press BACK (1s+) goes to file selection
  if (mappedInput.isPressed(MappedInputManager::Button::Back) && mappedInput.getHeldTime() >= ReaderUtils::GO_HOME_MS) {
    activityManager.goToFileBrowser(txt ? txt->getPath() : "");
    return;
  }

  // Short press BACK goes directly to home
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) &&
      mappedInput.getHeldTime() < ReaderUtils::GO_HOME_MS) {
    onGoHome();
    return;
  }

  auto [prevTriggered, nextTriggered] = ReaderUtils::detectPageTurn(mappedInput);
  if (!prevTriggered && !nextTriggered) {
    return;
  }

  if (prevTriggered && currentPage > 0) {
    currentPage--;
    requestUpdate();
  } else if (nextTriggered) {
    if (currentPage < totalPages - 1) {
      currentPage++;
      requestUpdate();
    } else {
      onGoHome();
    }
  }
}

void TxtReaderActivity::initializeReader() {
  if (initialized) {
    return;
  }

  // Store current settings for cache validation
  cachedFontId = currentReaderFontId(renderer);
  cachedScreenMargin = SETTINGS.screenMargin;
  cachedParagraphAlignment = SETTINGS.paragraphAlignment;

  // Calculate viewport dimensions
  renderer.getOrientedViewableTRBL(&cachedOrientedMarginTop, &cachedOrientedMarginRight, &cachedOrientedMarginBottom,
                                   &cachedOrientedMarginLeft);
  cachedOrientedMarginTop += cachedScreenMargin;
  cachedOrientedMarginLeft += cachedScreenMargin;
  cachedOrientedMarginRight += cachedScreenMargin;
  cachedOrientedMarginBottom +=
      std::max(cachedScreenMargin, static_cast<uint8_t>(UITheme::getInstance().getStatusBarHeight()));

  viewportWidth = renderer.getScreenWidth() - cachedOrientedMarginLeft - cachedOrientedMarginRight;
  const int viewportHeight = renderer.getScreenHeight() - cachedOrientedMarginTop - cachedOrientedMarginBottom;
  const int lineHeight = renderer.getLineHeight(cachedFontId);

  linesPerPage = viewportHeight / lineHeight;
  if (linesPerPage < 1) linesPerPage = 1;

  LOG_DBG("TRS", "Viewport: %dx%d, lines per page: %d", viewportWidth, viewportHeight, linesPerPage);

  // Try to load cached page index first
  if (!loadPageIndexCache()) {
    // Cache not found, build page index
    buildPageIndex();
  }

  // Load saved progress
  loadProgress();

  initialized = true;
}

void TxtReaderActivity::buildPageIndex() {
  totalPages = 0;

  const std::string cachePath = txt->getCachePath() + kIndexCacheFileName;
  const std::string tempCachePath = txt->getCachePath() + kIndexCacheTempFileName;
  const std::string backupCachePath = txt->getCachePath() + kIndexCacheBackupFileName;
  removeIfExists(tempCachePath);

  FsFile f;
  if (!Storage.openFileForWrite("TRS", tempCachePath, f)) {
    LOG_ERR("TRS", "Failed to open page index cache for writing");
    return;
  }

  // 先写入 header，占位页数，分页结束后再回写 numPages。
  serialization::writePod(f, CACHE_MAGIC);
  serialization::writePod(f, CACHE_VERSION);
  serialization::writePod(f, static_cast<uint32_t>(txt->getFileSize()));
  serialization::writePod(f, static_cast<int32_t>(viewportWidth));
  serialization::writePod(f, static_cast<int32_t>(linesPerPage));
  serialization::writePod(f, static_cast<int32_t>(cachedFontId));
  serialization::writePod(f, static_cast<int32_t>(cachedScreenMargin));
  serialization::writePod(f, cachedParagraphAlignment);
  serialization::writePod(f, static_cast<uint32_t>(0));

  size_t offset = 0;
  const size_t fileSize = txt->getFileSize();
  bool buildOk = true;
  unsigned long lastYieldMs = millis();

  LOG_DBG("TRS", "Building page index for %zu bytes...", fileSize);
  logTxtIndexMemory("build_begin", offset, totalPages, fileSize);

  GUI.drawPopup(renderer, tr(STR_INDEXING));

  while (offset < fileSize) {
    std::vector<std::string> tempLines;
    size_t nextOffset = offset;

    yieldDuringIndexing(lastYieldMs);

    if (!loadPageAtOffset(offset, tempLines, nextOffset)) {
      buildOk = false;
      break;
    }

    const uint32_t pageOffsetValue = static_cast<uint32_t>(offset);
    if (f.write(reinterpret_cast<const uint8_t*>(&pageOffsetValue), sizeof(pageOffsetValue)) !=
        sizeof(pageOffsetValue)) {
      LOG_ERR("TRS", "Failed to write page index entry %d", totalPages);
      buildOk = false;
      break;
    }
    totalPages++;

    if (nextOffset <= offset) {
      // 分页未推进时放弃本轮 cache，避免保存截断或重复 offset。
      buildOk = false;
      LOG_ERR("TRS", "Page index made no progress at offset %zu", offset);
      break;
    }

    offset = nextOffset;

    // Yield to other tasks periodically
    if (totalPages % 20 == 0) {
      logTxtIndexMemory("build_progress", offset, totalPages, fileSize);
      vTaskDelay(1);
      lastYieldMs = millis();
    }
  }

  if (!buildOk || totalPages <= 0) {
    logTxtIndexMemory("build_failed", offset, totalPages, fileSize);
    totalPages = 0;
    f.close();
    removeIfExists(tempCachePath);
    return;
  }

  if (!f.seek(cacheHeaderSize() - sizeof(uint32_t))) {
    LOG_ERR("TRS", "Failed to seek page count placeholder for cache patch");
    totalPages = 0;
    f.close();
    removeIfExists(tempCachePath);
    return;
  }

  const uint32_t totalPagesValue = static_cast<uint32_t>(totalPages);
  if (f.write(reinterpret_cast<const uint8_t*>(&totalPagesValue), sizeof(totalPagesValue)) != sizeof(totalPagesValue)) {
    LOG_ERR("TRS", "Failed to patch page count in cache header");
    totalPages = 0;
    f.close();
    removeIfExists(tempCachePath);
    return;
  }

  f.flush();
  f.close();

  // 通过 backup 回滚避免“旧 cache 已删但新 cache promote 失败”的窗口。
  if (!promoteIndexCacheFile(tempCachePath, cachePath, backupCachePath)) {
    logTxtIndexMemory("build_promote_failed", offset, totalPages, fileSize);
    totalPages = 0;
    removeIfExists(tempCachePath);
    return;
  }

  logTxtIndexMemory("build_end", offset, totalPages, fileSize);
  LOG_DBG("TRS", "Built page index: %d pages", totalPages);
}

bool TxtReaderActivity::loadPageAtOffset(size_t offset, std::vector<std::string>& outLines, size_t& nextOffset) {
  outLines.clear();
  const size_t fileSize = txt->getFileSize();

  if (offset >= fileSize) {
    return false;
  }

  // Read a chunk from file
  size_t chunkSize = std::min(CHUNK_SIZE, fileSize - offset);
  auto* buffer = static_cast<uint8_t*>(malloc(chunkSize + 1));
  if (!buffer) {
    LOG_ERR("TRS", "Failed to allocate %zu bytes", chunkSize);
    return false;
  }

  if (!txt->readContent(buffer, offset, chunkSize)) {
    free(buffer);
    return false;
  }
  buffer[chunkSize] = '\0';

  // Parse lines from buffer
  size_t pos = 0;
  unsigned long lastYieldMs = millis();
  std::vector<uint16_t> lineBoundaries;
  std::string widthScratch;

  while (pos < chunkSize && static_cast<int>(outLines.size()) < linesPerPage) {
    // Find end of line
    size_t lineEnd = pos;
    while (lineEnd < chunkSize && buffer[lineEnd] != '\n') {
      yieldDuringIndexing(lastYieldMs);
      lineEnd++;
    }

    // Check if we have a complete line
    bool lineComplete = (lineEnd < chunkSize) || (offset + lineEnd >= fileSize);

    if (!lineComplete && static_cast<int>(outLines.size()) > 0) {
      // Incomplete line and we already have some lines, stop here
      break;
    }

    // Calculate the actual length of line content in the buffer (excluding newline)
    size_t lineContentLen = lineEnd - pos;

    // Check for carriage return
    bool hasCR = (lineContentLen > 0 && buffer[pos + lineContentLen - 1] == '\r');
    size_t displayLen = hasCR ? lineContentLen - 1 : lineContentLen;
    if (!lineComplete) {
      displayLen = completeUtf8PrefixLength(buffer + pos, displayLen);
    }

    // Extract line content for display (without CR/LF)
    std::string line(reinterpret_cast<char*>(buffer + pos), displayLen);

    // Track position within this source line (in bytes from pos)
    size_t lineBytePos = 0;
    widthScratch.reserve(line.size());
    collectUtf8Boundaries(line, lineBoundaries, lastYieldMs);

    // Word wrap if needed
    while (lineBytePos < displayLen && static_cast<int>(outLines.size()) < linesPerPage) {
      yieldDuringIndexing(lastYieldMs);
      const char* remainingLine = line.data() + lineBytePos;
      const size_t remainingBytes = displayLen - lineBytePos;

      if (remainingBytes <= kDirectFitCheckMaxBytes &&
          prefixFitsViewport(renderer, cachedFontId, remainingLine, remainingBytes, viewportWidth, widthScratch)) {
        outLines.emplace_back(remainingLine, remainingBytes);
        lineBytePos = displayLen;  // Consumed entire display content
        break;
      }

      const WrapBreak wrapBreak = findWrapBreak(renderer, cachedFontId, line, lineBytePos, lineBoundaries,
                                                viewportWidth, widthScratch, lastYieldMs);
      const size_t visibleBytes = std::min(wrapBreak.visibleBytes, remainingBytes);
      const size_t consumeBytes = std::min(std::max(wrapBreak.consumeBytes, visibleBytes), remainingBytes);

      if (visibleBytes == 0 || consumeBytes == 0) {
        break;
      }

      outLines.emplace_back(remainingLine, visibleBytes);
      lineBytePos += consumeBytes;
    }

    // Determine how much of the source buffer we consumed
    if (lineBytePos >= displayLen) {
      // 已消耗完整源行；未完整读入的长行只推进到安全 UTF-8 边界。
      pos = lineComplete ? lineEnd + 1 : pos + lineBytePos;
    } else {
      // Partially consumed - page is full mid-line
      // Move pos to where we stopped in the line (NOT past the line)
      pos = pos + lineBytePos;
      break;
    }
  }

  // Ensure we make progress even if calculations go wrong
  if (pos == 0 && !outLines.empty()) {
    // Fallback: at minimum, consume something to avoid infinite loop
    pos = 1;
  }

  nextOffset = offset + pos;

  // Make sure we don't go past the file
  if (nextOffset > fileSize) {
    nextOffset = fileSize;
  }

  free(buffer);

  return !outLines.empty();
}

void TxtReaderActivity::render(RenderLock&&) {
  if (!txt) {
    return;
  }

  // Initialize reader if not done
  if (!initialized) {
    initializeReader();
  }

  if (totalPages <= 0) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID,
                              renderer.getTextYForCentering(0, renderer.getScreenHeight(), UI_12_FONT_ID),
                              tr(STR_EMPTY_FILE), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  // Bounds check
  if (currentPage < 0) currentPage = 0;
  if (currentPage >= totalPages) currentPage = totalPages - 1;

  // Load current page content
  size_t offset = 0;
  if (!loadPageOffset(currentPage, offset)) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID,
                              renderer.getTextYForCentering(0, renderer.getScreenHeight(), UI_12_FONT_ID),
                              tr(STR_EMPTY_FILE), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }
  size_t nextOffset;
  currentPageLines.clear();
  if (!loadPageAtOffset(offset, currentPageLines, nextOffset)) {
    renderer.clearScreen();
    renderer.drawCenteredText(UI_12_FONT_ID,
                              renderer.getTextYForCentering(0, renderer.getScreenHeight(), UI_12_FONT_ID),
                              tr(STR_EMPTY_FILE), true, EpdFontFamily::BOLD);
    renderer.displayBuffer();
    return;
  }

  renderer.clearScreen();
  renderPage();

  // Save progress
  saveProgress();
}

void TxtReaderActivity::renderPage() {
  const int lineHeight = renderer.getLineHeight(cachedFontId);
  const int contentWidth = viewportWidth;

  // Render text lines with alignment
  auto renderLines = [&]() {
    int y = cachedOrientedMarginTop;
    for (const auto& line : currentPageLines) {
      if (!line.empty()) {
        int x = cachedOrientedMarginLeft;

        // Apply text alignment
        switch (cachedParagraphAlignment) {
          case CrossPointSettings::LEFT_ALIGN:
          default:
            // x already set to left margin
            break;
          case CrossPointSettings::CENTER_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = cachedOrientedMarginLeft + (contentWidth - textWidth) / 2;
            break;
          }
          case CrossPointSettings::RIGHT_ALIGN: {
            int textWidth = renderer.getTextWidth(cachedFontId, line.c_str());
            x = cachedOrientedMarginLeft + contentWidth - textWidth;
            break;
          }
          case CrossPointSettings::JUSTIFIED:
            // For plain text, justified is treated as left-aligned
            // (true justification would require word spacing adjustments)
            break;
        }

        renderer.drawText(cachedFontId, x, y, line.c_str());
      }
      y += lineHeight;
    }
  };

  // Font prewarm: scan pass accumulates text, then prewarm, then real render
  auto* fcm = renderer.getFontCacheManager();
  auto scope = fcm->createPrewarmScope();
  renderLines();  // scan pass — text accumulated, no drawing
  scope.endScanAndPrewarm();

  // BW rendering
  renderLines();
  renderStatusBar();

  ReaderUtils::displayWithRefreshCycle(renderer, pagesUntilFullRefresh);
  // TXT uses one stable BW frame; grayscale two-pass rendering can make plain text fade on e-ink.
  // scope destructor clears font cache via FontCacheManager
}

void TxtReaderActivity::renderStatusBar() const {
  const float progress = totalPages > 0 ? (currentPage + 1) * 100.0f / totalPages : 0;
  std::string title;
  if (SETTINGS.statusBarTitle != CrossPointSettings::STATUS_BAR_TITLE::HIDE_TITLE) {
    title = txt->getTitle();
  }
  GUI.drawStatusBar(renderer, progress, currentPage + 1, totalPages, title);
}

void TxtReaderActivity::saveProgress() const {
  FsFile f;
  if (Storage.openFileForWrite("TRS", txt->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    data[0] = currentPage & 0xFF;
    data[1] = (currentPage >> 8) & 0xFF;
    data[2] = 0;
    data[3] = 0;
    f.write(data, 4);
  }
}

void TxtReaderActivity::loadProgress() {
  if (totalPages <= 0) {
    currentPage = 0;
    return;
  }

  FsFile f;
  if (Storage.openFileForRead("TRS", txt->getCachePath() + "/progress.bin", f)) {
    uint8_t data[4];
    if (f.read(data, 4) == 4) {
      currentPage = data[0] + (data[1] << 8);
      if (currentPage >= totalPages) {
        currentPage = totalPages - 1;
      }
      if (currentPage < 0) {
        currentPage = 0;
      }
      LOG_DBG("TRS", "Loaded progress: page %d/%d", currentPage, totalPages);
    }
  }
}

bool TxtReaderActivity::loadPageIndexCache() {
  // Cache file format (using serialization module):
  // - uint32_t: magic "TXTI"
  // - uint8_t: cache version
  // - uint32_t: file size (to validate cache)
  // - int32_t: viewport width
  // - int32_t: lines per page
  // - int32_t: font ID (to invalidate cache on font change)
  // - int32_t: screen margin (to invalidate cache on margin change)
  // - uint8_t: paragraph alignment (to invalidate cache on alignment change)
  // - uint32_t: total pages count
  // - N * uint32_t: page offsets

  std::string cachePath = txt->getCachePath() + kIndexCacheFileName;
  FsFile f;
  if (!Storage.openFileForRead("TRS", cachePath, f)) {
    LOG_DBG("TRS", "No page index cache found");
    return false;
  }

  const size_t cacheSize = static_cast<size_t>(f.size());
  if (cacheSize < cacheHeaderSize()) {
    LOG_DBG("TRS", "Page index cache is too small (%zu < %zu), rebuilding", cacheSize, cacheHeaderSize());
    return false;
  }

  // Read and validate header using serialization module
  uint32_t magic;
  serialization::readPod(f, magic);
  if (magic != CACHE_MAGIC) {
    LOG_DBG("TRS", "Cache magic mismatch, rebuilding");
    return false;
  }

  uint8_t version;
  serialization::readPod(f, version);
  if (version != CACHE_VERSION) {
    LOG_DBG("TRS", "Cache version mismatch (%d != %d), rebuilding", version, CACHE_VERSION);
    return false;
  }

  uint32_t fileSize;
  serialization::readPod(f, fileSize);
  if (fileSize != txt->getFileSize()) {
    LOG_DBG("TRS", "Cache file size mismatch, rebuilding");
    return false;
  }

  int32_t cachedWidth;
  serialization::readPod(f, cachedWidth);
  if (cachedWidth != viewportWidth) {
    LOG_DBG("TRS", "Cache viewport width mismatch, rebuilding");
    return false;
  }

  int32_t cachedLines;
  serialization::readPod(f, cachedLines);
  if (cachedLines != linesPerPage) {
    LOG_DBG("TRS", "Cache lines per page mismatch, rebuilding");
    return false;
  }

  int32_t fontId;
  serialization::readPod(f, fontId);
  if (fontId != cachedFontId) {
    LOG_DBG("TRS", "Cache font ID mismatch (%d != %d), rebuilding", fontId, cachedFontId);
    return false;
  }

  int32_t margin;
  serialization::readPod(f, margin);
  if (margin != cachedScreenMargin) {
    LOG_DBG("TRS", "Cache screen margin mismatch, rebuilding");
    return false;
  }

  uint8_t alignment;
  serialization::readPod(f, alignment);
  if (alignment != cachedParagraphAlignment) {
    LOG_DBG("TRS", "Cache paragraph alignment mismatch, rebuilding");
    return false;
  }

  uint32_t numPages;
  serialization::readPod(f, numPages);

  const size_t expectedSize = cacheHeaderSize() + static_cast<size_t>(numPages) * sizeof(uint32_t);
  if (numPages == 0 || cacheSize != expectedSize) {
    LOG_DBG("TRS", "Cache size mismatch for %u pages (%zu != %zu), rebuilding", numPages, cacheSize, expectedSize);
    return false;
  }

  totalPages = static_cast<int>(numPages);
  LOG_DBG("TRS", "Loaded page index cache: %d pages", totalPages);
  return true;
}

bool TxtReaderActivity::loadPageOffset(uint32_t pageIndex, size_t& outOffset) const {
  if (!txt || pageIndex >= static_cast<uint32_t>(totalPages)) {
    return false;
  }

  std::string cachePath = txt->getCachePath() + kIndexCacheFileName;
  FsFile f;
  if (!Storage.openFileForRead("TRS", cachePath, f)) {
    LOG_ERR("TRS", "Failed to open page index cache for page %u", pageIndex);
    return false;
  }

  const size_t entryPos = cacheHeaderSize() + sizeof(uint32_t) * pageIndex;
  if (entryPos + sizeof(uint32_t) > static_cast<size_t>(f.size())) {
    LOG_ERR("TRS", "Page index entry %u is out of range", pageIndex);
    return false;
  }
  if (!f.seek(entryPos)) {
    LOG_ERR("TRS", "Failed to seek page index entry %u", pageIndex);
    return false;
  }

  uint32_t offset = 0;
  if (f.read(reinterpret_cast<uint8_t*>(&offset), sizeof(offset)) != sizeof(offset)) {
    LOG_ERR("TRS", "Failed to read page index entry %u", pageIndex);
    return false;
  }
  outOffset = offset;
  return true;
}
