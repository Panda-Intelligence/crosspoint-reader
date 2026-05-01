#include "RecentBooksActivity.h"

#include <Bitmap.h>
#include <Epub.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Serialization.h>
#include <Txt.h>
#include <Xtc.h>

#include <algorithm>

#include "MappedInputManager.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr unsigned long GO_HOME_MS = 1000;
constexpr int kCoverWidth = 34;
constexpr int kCoverHeight = 52;
constexpr int kCoverRadius = 3;
constexpr int kHomeCoverFallbackHeight = 400;
constexpr int kProgressBarHeight = 5;
constexpr uint32_t TXT_INDEX_CACHE_MAGIC = 0x54585449;  // "TXTI"
constexpr uint8_t TXT_INDEX_CACHE_VERSION = 2;

std::string fileNameForPath(const std::string& path) {
  const size_t slash = path.find_last_of('/');
  if (slash == std::string::npos || slash + 1 >= path.size()) {
    return path;
  }
  return path.substr(slash + 1);
}

uint8_t clampProgressPercent(int percent) {
  if (percent < 0) {
    return 0;
  }
  if (percent > 100) {
    return 100;
  }
  return static_cast<uint8_t>(percent);
}
}  // namespace

void RecentBooksActivity::loadRecentBooks() {
  recentBooks.clear();
  const auto& books = RECENT_BOOKS.getBooks();
  recentBooks.reserve(books.size());

  for (const auto& book : books) {
    // Skip if file no longer exists
    if (!Storage.exists(book.path.c_str())) {
      continue;
    }
    recentBooks.push_back({book, loadProgressForBook(book)});
  }
}

RecentBookProgress RecentBooksActivity::loadProgressForBook(const RecentBook& book) const {
  RecentBookProgress progress;
  const std::string filename = fileNameForPath(book.path);

  if (FsHelpers::hasEpubExtension(filename)) {
    Epub epub(book.path, "/.crosspoint");
    if (!epub.load(false, true)) {
      return progress;
    }

    FsFile f;
    if (!Storage.openFileForRead("RBA", epub.getCachePath() + "/progress.bin", f)) {
      return progress;
    }

    uint8_t data[6] = {};
    const int dataSize = f.read(data, sizeof(data));
    if (dataSize != 4 && dataSize != 6) {
      return progress;
    }

    const int spineIndex = data[0] + (data[1] << 8);
    const uint32_t page = static_cast<uint32_t>(data[2] + (data[3] << 8));
    const uint32_t pageCount = dataSize == 6 ? static_cast<uint32_t>(data[4] + (data[5] << 8)) : 0;
    if (spineIndex < 0 || spineIndex >= epub.getSpineItemsCount()) {
      return progress;
    }

    const float chapterProgress =
        pageCount > 0 ? std::min(1.0f, static_cast<float>(page + 1) / static_cast<float>(pageCount)) : 0.0f;
    progress.percent = clampProgressPercent(static_cast<int>(epub.calculateProgress(spineIndex, chapterProgress) *
                                                             100.0f +
                                                             0.5f));
    progress.currentPage = page + 1;
    progress.totalPages = pageCount;
    progress.available = true;
    return progress;
  }

  if (FsHelpers::hasXtcExtension(filename)) {
    Xtc xtc(book.path, "/.crosspoint");
    if (!xtc.load()) {
      return progress;
    }

    FsFile f;
    if (!Storage.openFileForRead("RBA", xtc.getCachePath() + "/progress.bin", f)) {
      return progress;
    }

    uint8_t data[4] = {};
    if (f.read(data, sizeof(data)) != static_cast<int>(sizeof(data))) {
      return progress;
    }

    uint32_t page = data[0] | (static_cast<uint32_t>(data[1]) << 8) | (static_cast<uint32_t>(data[2]) << 16) |
                    (static_cast<uint32_t>(data[3]) << 24);
    const uint32_t pageCount = xtc.getPageCount();
    if (pageCount == 0) {
      return progress;
    }
    if (page >= pageCount) {
      page = pageCount - 1;
    }

    progress.percent = xtc.calculateProgress(page);
    progress.currentPage = page + 1;
    progress.totalPages = pageCount;
    progress.available = true;
    return progress;
  }

  if (FsHelpers::hasTxtExtension(filename) || FsHelpers::hasMarkdownExtension(filename)) {
    Txt txt(book.path, "/.crosspoint");
    if (!txt.load()) {
      return progress;
    }

    FsFile progressFile;
    if (!Storage.openFileForRead("RBA", txt.getCachePath() + "/progress.bin", progressFile)) {
      return progress;
    }

    uint8_t data[4] = {};
    if (progressFile.read(data, sizeof(data)) != static_cast<int>(sizeof(data))) {
      return progress;
    }
    uint32_t page = data[0] + (data[1] << 8);

    FsFile indexFile;
    if (!Storage.openFileForRead("RBA", txt.getCachePath() + "/index.bin", indexFile)) {
      return progress;
    }

    uint32_t magic = 0;
    uint8_t version = 0;
    uint32_t fileSize = 0;
    int32_t cachedWidth = 0;
    int32_t cachedLines = 0;
    int32_t fontId = 0;
    int32_t margin = 0;
    uint8_t alignment = 0;
    uint32_t pageCount = 0;
    serialization::readPod(indexFile, magic);
    serialization::readPod(indexFile, version);
    serialization::readPod(indexFile, fileSize);
    serialization::readPod(indexFile, cachedWidth);
    serialization::readPod(indexFile, cachedLines);
    serialization::readPod(indexFile, fontId);
    serialization::readPod(indexFile, margin);
    serialization::readPod(indexFile, alignment);
    serialization::readPod(indexFile, pageCount);

    if (magic != TXT_INDEX_CACHE_MAGIC || version != TXT_INDEX_CACHE_VERSION || fileSize != txt.getFileSize() ||
        pageCount == 0) {
      return progress;
    }
    if (page >= pageCount) {
      page = pageCount - 1;
    }

    progress.percent =
        clampProgressPercent(static_cast<int>((static_cast<uint64_t>(page + 1) * 100 + pageCount / 2) / pageCount));
    progress.currentPage = page + 1;
    progress.totalPages = pageCount;
    progress.available = true;
    return progress;
  }

  return progress;
}

void RecentBooksActivity::onEnter() {
  Activity::onEnter();

  // Load data
  loadRecentBooks();

  selectorIndex = 0;
  requestUpdate();
}

void RecentBooksActivity::onExit() {
  Activity::onExit();
  recentBooks.clear();
}

void RecentBooksActivity::loop() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageItems = std::max(
      1, (renderer.getScreenHeight() -
          (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)) /
             metrics.listWithSubtitleRowHeight);

  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap()) {
      const Rect listRect{
          0, metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing, renderer.getScreenWidth(),
          renderer.getScreenHeight() -
              (metrics.topPadding + metrics.headerHeight + metrics.buttonHintsHeight + metrics.verticalSpacing * 2)};
      const int clickedIndex =
          TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, static_cast<int>(selectorIndex),
                                   static_cast<int>(recentBooks.size()), touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectorIndex = clickedIndex;
        LOG_DBG("RBA", "Selected recent book by touch: %s", recentBooks[selectorIndex].book.path.c_str());
        onSelectBook(recentBooks[selectorIndex].book.path);
        return;
      }
    } else {
      const auto gestureAction = TouchHitTest::listGestureActionForTouch(touchEvent);
      if (gestureAction == TouchHitTest::ListGestureAction::NextItem) {
        mappedInput.suppressTouchButtonFallback();
        selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex),
                                                       static_cast<int>(recentBooks.size()), pageItems);
        requestUpdate();
        return;
      }
      if (gestureAction == TouchHitTest::ListGestureAction::PreviousItem) {
        mappedInput.suppressTouchButtonFallback();
        selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex),
                                                           static_cast<int>(recentBooks.size()), pageItems);
        requestUpdate();
        return;
      }
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (!recentBooks.empty() && selectorIndex < static_cast<int>(recentBooks.size())) {
      LOG_DBG("RBA", "Selected recent book: %s", recentBooks[selectorIndex].book.path.c_str());
      onSelectBook(recentBooks[selectorIndex].book.path);
      return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    onGoHome();
  }

  int listSize = static_cast<int>(recentBooks.size());

  buttonNavigator.onNextRelease([this, listSize] {
    selectorIndex = ButtonNavigator::nextIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this, listSize] {
    selectorIndex = ButtonNavigator::previousIndex(static_cast<int>(selectorIndex), listSize);
    requestUpdate();
  });

  buttonNavigator.onNextContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::nextPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });

  buttonNavigator.onPreviousContinuous([this, listSize, pageItems] {
    selectorIndex = ButtonNavigator::previousPageIndex(static_cast<int>(selectorIndex), listSize, pageItems);
    requestUpdate();
  });
}

std::string RecentBooksActivity::subtitleForBook(const RecentBook& book) const {
  if (!book.author.empty()) {
    return book.author;
  }
  return fileNameForPath(book.path);
}

bool RecentBooksActivity::drawBookCover(const RecentBook& book, const int x, const int y, const int width,
                                        const int height) {
  bool renderedCover = false;
  if (!book.coverBmpPath.empty()) {
    const std::string coverPaths[] = {UITheme::getCoverThumbPath(book.coverBmpPath, height),
                                      UITheme::getCoverThumbPath(book.coverBmpPath, kHomeCoverFallbackHeight),
                                      book.coverBmpPath};
    for (const auto& coverPath : coverPaths) {
      FsFile file;
      if (Storage.openFileForRead("RBA", coverPath, file)) {
        Bitmap bitmap(file);
        if (bitmap.parseHeaders() == BmpReaderError::Ok) {
          renderer.drawBitmap(bitmap, x, y, width, height);
          renderedCover = true;
        }
        file.close();
      }
      if (renderedCover) {
        break;
      }
    }
  }

  renderer.drawRoundedRect(x, y, width, height, 1, kCoverRadius, true);
  if (!renderedCover) {
    renderer.fillRect(x + 1, y + height / 3, width - 2, height / 3, true);
    renderer.drawText(SMALL_FONT_ID, x + 8, y + height / 2 - 5, "B", false, EpdFontFamily::BOLD);
  }
  return renderedCover;
}

void RecentBooksActivity::drawRecentBookRow(const RecentBookListItem& item, const int index, const Rect rowRect,
                                            const bool selected) {
  const RecentBook& book = item.book;
  if (selected) {
    renderer.fillRect(rowRect.x, rowRect.y - 2, rowRect.width, rowRect.height, true);
  }

  const int coverX = rowRect.x + UITheme::getInstance().getMetrics().contentSidePadding;
  const int coverY = rowRect.y + (rowRect.height - kCoverHeight) / 2 - 1;
  renderer.fillRoundedRect(coverX - 1, coverY - 1, kCoverWidth + 2, kCoverHeight + 2, kCoverRadius, Color::White);
  drawBookCover(book, coverX, coverY, kCoverWidth, kCoverHeight);

  const int textX = coverX + kCoverWidth + 12;
  const int textWidth = rowRect.width - textX - UITheme::getInstance().getMetrics().contentSidePadding;
  const std::string title = book.title.empty() ? fileNameForPath(book.path) : book.title;
  const std::string truncatedTitle = renderer.truncatedText(UI_12_FONT_ID, title.c_str(), textWidth);
  renderer.drawText(UI_12_FONT_ID, textX, rowRect.y + 5, truncatedTitle.c_str(), !selected);

  std::string subtitle = subtitleForBook(book);
  if (item.progress.available) {
    subtitle += "  ";
    subtitle += std::to_string(item.progress.percent);
    subtitle += "%";
  }
  const std::string subtitleText = renderer.truncatedText(UI_10_FONT_ID, subtitle.c_str(), textWidth);
  renderer.drawText(UI_10_FONT_ID, textX, rowRect.y + 32, subtitleText.c_str(), !selected);

  if (item.progress.available) {
    const int barY = rowRect.y + rowRect.height - kProgressBarHeight - 6;
    const int barWidth = std::max(20, textWidth);
    renderer.drawRect(textX, barY, barWidth, kProgressBarHeight, !selected);
    const int fillWidth = std::max(1, (barWidth - 2) * item.progress.percent / 100);
    renderer.fillRect(textX + 1, barY + 1, fillWidth, kProgressBarHeight - 2, !selected);
  }

  if (selected) {
    char slot[8];
    snprintf(slot, sizeof(slot), "%d", index + 1);
    renderer.fillRoundedRect(rowRect.width - 32, rowRect.y + 18, 20, 20, 3, Color::White);
    renderer.drawText(SMALL_FONT_ID, rowRect.width - 26, rowRect.y + 20, slot, true, EpdFontFamily::BOLD);
  }
}

void RecentBooksActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  const auto& metrics = UITheme::getInstance().getMetrics();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MENU_RECENT_BOOKS));

  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing;

  // Recent tab
  if (recentBooks.empty()) {
    renderer.drawText(UI_10_FONT_ID, metrics.contentSidePadding, contentTop + 20, tr(STR_NO_RECENT_BOOKS));
  } else {
    const int rowHeight = metrics.listWithSubtitleRowHeight;
    const int pageItems = std::max(1, contentHeight / rowHeight);
    const int totalPages = (static_cast<int>(recentBooks.size()) + pageItems - 1) / pageItems;
    if (totalPages > 1) {
      constexpr int indicatorWidth = 20;
      constexpr int arrowSize = 6;
      constexpr int margin = 15;
      const int centerX = pageWidth - indicatorWidth / 2 - margin;
      const int indicatorBottom = contentTop + contentHeight - arrowSize;
      for (int i = 0; i < arrowSize; ++i) {
        const int lineWidth = 1 + i * 2;
        renderer.drawLine(centerX - i, contentTop + i, centerX - i + lineWidth - 1, contentTop + i);
      }
      for (int i = 0; i < arrowSize; ++i) {
        const int lineWidth = 1 + (arrowSize - 1 - i) * 2;
        const int startX = centerX - (arrowSize - 1 - i);
        renderer.drawLine(startX, indicatorBottom - arrowSize + 1 + i, startX + lineWidth - 1,
                          indicatorBottom - arrowSize + 1 + i);
      }
    }

    const int pageStartIndex = static_cast<int>(selectorIndex) / pageItems * pageItems;
    const int pageEndIndex = std::min(static_cast<int>(recentBooks.size()), pageStartIndex + pageItems);
    for (int i = pageStartIndex; i < pageEndIndex; i++) {
      const int rowY = contentTop + (i - pageStartIndex) * rowHeight;
      drawRecentBookRow(recentBooks[i], i, Rect{0, rowY, pageWidth, rowHeight}, i == static_cast<int>(selectorIndex));
    }
  }

  // Help text
  const auto labels = mappedInput.mapLabels(tr(STR_HOME), tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);

  renderer.displayBuffer();
}
