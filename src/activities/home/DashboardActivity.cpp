#include "DashboardActivity.h"

#include <FontCacheManager.h>
#include <HalGPIO.h>
#include <I18n.h>
#include <Logging.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "DashboardShortcutStore.h"
#include "DesktopSummaryStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "components/icons/appgrid64.h"
#include "components/icons/book64.h"
#include "components/icons/bookmark64.h"
#include "components/icons/calendar64.h"
#include "components/icons/folder64.h"
#include "components/icons/hotspot64.h"
#include "components/icons/joystick64.h"
#include "components/icons/library64.h"
#include "components/icons/recent64.h"
#include "components/icons/settings2_64.h"
#include "components/icons/transfer64.h"
#include "components/icons/weather64.h"
#include "components/icons/wifi64.h"
#include "fontIds.h"
#include "util/ButtonNavigator.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kGridGapPx = 0;
constexpr int kCellInnerPadPx = 0;
constexpr int kCellCornerRadiusPx = 0;
constexpr int kDashboardIosGridIconSizePx = 64;
constexpr int kDashboardLabelSpacingPx = 3;
constexpr int kDashboardCellBorderInsetPx = 3;
constexpr int kDashboardCellBottomInsetPx = 5;
constexpr int kDashboardGridSubtitleMaxLines = 2;
constexpr int kDashboardListSwipeStep = 1;
constexpr int kDashboardListTitleReserveRows = 1;

CrossPointSettings::DASHBOARD_LAYOUT dashboardLayoutMode() {
  if (SETTINGS.dashboardLayout >= CrossPointSettings::DASHBOARD_LAYOUT_COUNT) {
    return CrossPointSettings::DASHBOARD_LAYOUT_GRID;
  }
  return static_cast<CrossPointSettings::DASHBOARD_LAYOUT>(SETTINGS.dashboardLayout);
}

bool dashboardUsesListLayout() { return dashboardLayoutMode() == CrossPointSettings::DASHBOARD_LAYOUT_LIST; }

bool dashboardUsesIosGridLayout() { return dashboardLayoutMode() == CrossPointSettings::DASHBOARD_LAYOUT_IOS_GRID; }

#if MOFEI_TOUCH_DEBUG
const char* dashboardLayoutName() {
  switch (dashboardLayoutMode()) {
    case CrossPointSettings::DASHBOARD_LAYOUT_LIST:
      return "list";
    case CrossPointSettings::DASHBOARD_LAYOUT_GRID:
      return "grid";
    case CrossPointSettings::DASHBOARD_LAYOUT_IOS_GRID:
      return "ios-grid";
    case CrossPointSettings::DASHBOARD_LAYOUT_COUNT:
    default:
      return "unknown";
  }
}

void logDashboardTouchHit(const InputTouchEvent& event, const char* target, const int index, const Rect& rect) {
  LOG_INF("TOUCHDBG", "dashboard_hit layout=%s target=%s index=%d raw=(%u,%u) oriented=(%u,%u) rect=(%d,%d,%d,%d)",
          dashboardLayoutName(), target, index, event.sourceX(), event.sourceY(), event.x, event.y, rect.x, rect.y,
          rect.width, rect.height);
}

void logDashboardTouchMiss(const InputTouchEvent& event, const int itemCount) {
  LOG_INF("TOUCHDBG", "dashboard_miss layout=%s raw=(%u,%u) oriented=(%u,%u) items=%d", dashboardLayoutName(),
          event.sourceX(), event.sourceY(), event.x, event.y, itemCount);
}
#endif

std::string formatDashboardValue(const StrId id, const int value) {
  char buffer[64];
  snprintf(buffer, sizeof(buffer), I18N.get(id), value);
  return buffer;
}

std::string formatDashboardValue(const StrId id, const int firstValue, const int secondValue) {
  char buffer[80];
  snprintf(buffer, sizeof(buffer), I18N.get(id), firstValue, secondValue);
  return buffer;
}

const uint8_t* iconBitmap64ForName(const UIIcon icon) {
  switch (icon) {
    case UIIcon::Folder:
      return Folder64Icon;
    case UIIcon::Book:
      return Book64Icon;
    case UIIcon::Recent:
      return Recent64Icon;
    case UIIcon::Settings:
      return Settings2_64Icon;
    case UIIcon::Transfer:
      return Transfer64Icon;
    case UIIcon::Library:
      return Library64Icon;
    case UIIcon::Wifi:
      return Wifi64Icon;
    case UIIcon::Hotspot:
      return Hotspot64Icon;
    case UIIcon::Weather:
      return Weather64Icon;
    case UIIcon::Calendar:
      return Calendar64Icon;
    case UIIcon::Bookmark:
      return Bookmark64Icon;
    case UIIcon::Joystick:
      return Joystick64Icon;
    case UIIcon::AppGrid:
      return AppGrid64Icon;
    case UIIcon::Text:
    case UIIcon::Image:
    case UIIcon::File:
      return nullptr;
  }

  return nullptr;
}

std::vector<std::string> wrapSubtitle(const GfxRenderer& renderer, const std::string& text, int maxWidth,
                                      int maxLines) {
  if (text.empty() || maxWidth <= 0 || maxLines <= 0) return {};

  auto lines = renderer.wrappedText(SMALL_FONT_ID, text.c_str(), maxWidth, maxLines);
  if (lines.size() > 1 || renderer.getTextWidth(SMALL_FONT_ID, text.c_str()) <= maxWidth) {
    return lines;
  }

  lines.clear();

  const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text.c_str());
  std::string line;
  while (*cursor != '\0') {
    const unsigned char* charStart = cursor;
    const uint32_t cp = utf8NextCodepoint(&cursor);
    if (cp == 0 || cursor <= charStart) break;
    const std::string nextToken(reinterpret_cast<const char*>(charStart), static_cast<size_t>(cursor - charStart));
    const std::string candidate = line + nextToken;

    if (!line.empty() && renderer.getTextWidth(SMALL_FONT_ID, candidate.c_str()) > maxWidth) {
      lines.push_back(renderer.truncatedText(SMALL_FONT_ID, line.c_str(), maxWidth));
      line = nextToken;
      if (static_cast<int>(lines.size()) == maxLines - 1) {
        std::string remainder = line + reinterpret_cast<const char*>(cursor);
        lines.push_back(renderer.truncatedText(SMALL_FONT_ID, remainder.c_str(), maxWidth));
        return lines;
      }
    } else {
      line = candidate;
    }
  }

  if (!line.empty() && static_cast<int>(lines.size()) < maxLines) {
    lines.push_back(renderer.truncatedText(SMALL_FONT_ID, line.c_str(), maxWidth));
  }

  return lines;
}

int listRowHeightForDashboard(const ThemeMetrics& metrics) { return std::max(metrics.listWithSubtitleRowHeight, 1); }

Rect dashboardListRect(const GfxRenderer& renderer, const ThemeMetrics& metrics) {
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing;
  return Rect{0, contentTop, pageWidth, std::max(contentBottom - contentTop, 0)};
}

int rowItemCount(const int row, const int itemCount, const int columnCount) {
  if (row < 0 || itemCount <= 0 || columnCount <= 0) {
    return 0;
  }

  const int rowStart = row * columnCount;
  if (rowStart >= itemCount) {
    return 0;
  }
  return std::min(columnCount, itemCount - rowStart);
}

int clampedIndexForRowAndColumn(const int row, const int column, const int itemCount, const int columnCount) {
  const int itemsInRow = rowItemCount(row, itemCount, columnCount);
  if (itemsInRow <= 0) {
    return -1;
  }

  const int clampedColumn = std::clamp(column, 0, itemsInRow - 1);
  return row * columnCount + clampedColumn;
}

#if MOFEI_DEVICE
bool mapButtonHintTap(const InputTouchEvent& touchEvent, uint8_t* outButtonIndex) {
  if (outButtonIndex == nullptr) {
    return false;
  }

  uint8_t buttonIndex = 0;
  if (!gpio.mapMofeiButtonHintTapToButton(touchEvent.sourceX(), touchEvent.sourceY(), &buttonIndex)) {
    return false;
  }
  *outButtonIndex = buttonIndex;
  return true;
}
#endif
}  // namespace

void DashboardActivity::onEnter() {
  Activity::onEnter();
  DASHBOARD_SHORTCUTS.loadFromFile();
  selectedIndex = 0;
  STUDY_STATE.loadFromFile();
  DESKTOP_SUMMARY.refresh();
  requestUpdate();
}

int DashboardActivity::gridRowCount() const { return (itemCount() + kGridCols - 1) / kGridCols; }

bool DashboardActivity::isGridIndex(const int index) const { return index >= 0 && index < itemCount(); }

std::string DashboardActivity::subtitleForShortcut(const DashboardShortcutId id) const {
  const auto& summary = DESKTOP_SUMMARY.getState();
  switch (id) {
    case DashboardShortcutId::WeatherClock:
      return summary.weatherLine;
    case DashboardShortcutId::Today:
      return summary.todaySecondary;
    case DashboardShortcutId::StudyHub:
      if (summary.againCards > 0) {
        return formatDashboardValue(StrId::STR_DASHBOARD_STUDY_AGAIN_FORMAT, summary.againCards);
      }
      if (summary.dueCards > 0) {
        return formatDashboardValue(StrId::STR_DASHBOARD_STUDY_DUE_FORMAT, summary.dueCards);
      }
      return summary.loadedCards <= 0 ? std::string(tr(STR_DASHBOARD_STUDY_IMPORT_HINT))
                                      : std::string(tr(STR_DASHBOARD_STUDY_CAUGHT_UP));
    default: {
      const auto* definition = DashboardShortcutStore::definitionFor(id);
      return definition != nullptr ? std::string(I18N.get(definition->subtitleId)) : std::string();
    }
  }
}

void DashboardActivity::openCurrentSelection() {
  if (!isGridIndex(selectedIndex)) {
    requestUpdate();
    return;
  }

  switch (DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(selectedIndex))) {
    case DashboardShortcutId::WeatherClock:
      activityManager.goToWeatherClock();
      break;
    case DashboardShortcutId::Today:
      activityManager.goToCalendar();
      break;
    case DashboardShortcutId::StudyHub:
      activityManager.goToStudyHub();
      break;
    case DashboardShortcutId::ReadingHub:
      activityManager.goToReadingHub();
      break;
    case DashboardShortcutId::ArcadeHub:
      activityManager.goToArcadeHub();
      break;
    case DashboardShortcutId::ImportSync:
      activityManager.goToFileTransfer();
      break;
    case DashboardShortcutId::FileBrowser:
      activityManager.goToFileBrowser();
      break;
    case DashboardShortcutId::Settings:
      activityManager.goToSettings();
      break;
    case DashboardShortcutId::RecentReading:
      activityManager.goToRecentBooks();
      break;
    case DashboardShortcutId::Count:
    case DashboardShortcutId::DesktopHub:
    case DashboardShortcutId::StudyToday:
      requestUpdate();
      return;
  }
}

void DashboardActivity::layoutCells() {
  if (dashboardUsesListLayout()) {
    layoutListCells();
  } else {
    layoutGridCells();
  }
}

void DashboardActivity::layoutGridCells() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int sidePad = std::max(metrics.contentSidePadding, 0);
  const int count = itemCount();
  const int rows = std::max(gridRowCount(), 1);
  selectedIndex = std::clamp(selectedIndex, 0, std::max(count - 1, 0));

  const int gridTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int buttonHintsTop = pageHeight - metrics.buttonHintsHeight;
  const int contentBottom = std::max(buttonHintsTop, gridTop);
  const int gridHeight = std::max(contentBottom - gridTop, 0);

  const int usableWidth = std::max(pageWidth - 2 * sidePad - (kGridCols - 1) * kGridGapPx, 0);
  const int cellWidth = usableWidth / kGridCols;
  const int usableHeight = std::max(gridHeight - (rows - 1) * kGridGapPx, 0);
  const int cellHeight = rows > 0 ? usableHeight / rows : 0;

  cellRects.fill(Rect{});

  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < kGridCols; ++col) {
      const int idx = row * kGridCols + col;
      if (idx >= count) {
        continue;
      }
      const int x = sidePad + col * (cellWidth + kGridGapPx);
      const int y = gridTop + row * (cellHeight + kGridGapPx);
      cellRects[idx] = Rect{x, y, cellWidth, cellHeight};
    }
  }
}

void DashboardActivity::layoutListCells() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  selectedIndex = std::clamp(selectedIndex, 0, std::max(selectionCount() - 1, 0));
  cellRects.fill(Rect{});

  const Rect listRect = dashboardListRect(renderer, metrics);
  const int rowHeight = listRowHeightForDashboard(metrics);
  const int pageItems = std::max(listRect.height / rowHeight, 1);
  const int pageStartIndex = (selectedIndex / pageItems) * pageItems;

  for (int idx = pageStartIndex; idx < selectionCount() && idx < pageStartIndex + pageItems; ++idx) {
    const int row = idx - pageStartIndex;
    cellRects[idx] = Rect{listRect.x, listRect.y + row * rowHeight, listRect.width, rowHeight};
  }
}

void DashboardActivity::moveSelectionHorizontally(const int delta) {
  if (delta == 0) {
    return;
  }

  if (dashboardUsesListLayout()) {
    return;
  }

  const int count = itemCount();
  if (count <= 0) {
    selectedIndex = 0;
    return;
  }

  const int row = selectedIndex / kGridCols;
  const int itemsInRow = rowItemCount(row, count, kGridCols);
  if (itemsInRow <= 1) {
    return;
  }

  int column = selectedIndex % kGridCols;
  column += delta > 0 ? 1 : -1;
  if (column < 0) {
    column = itemsInRow - 1;
  } else if (column >= itemsInRow) {
    column = 0;
  }

  selectedIndex = row * kGridCols + column;
}

void DashboardActivity::moveSelectionVertically(const int delta) {
  if (delta == 0) {
    return;
  }

  if (dashboardUsesListLayout()) {
    const int total = selectionCount();
    if (delta > 0) {
      selectedIndex = (selectedIndex + kDashboardListSwipeStep) % total;
    } else {
      selectedIndex = (selectedIndex + total - kDashboardListSwipeStep) % total;
    }
    return;
  }

  const int count = itemCount();
  if (count <= 0) {
    selectedIndex = 0;
    return;
  }

  const int lastRow = std::max(gridRowCount() - 1, 0);
  const int row = selectedIndex / kGridCols;
  const int column = selectedIndex % kGridCols;
  if (delta > 0) {
    if (row >= lastRow) {
      selectedIndex = 0;
      return;
    }
    selectedIndex = clampedIndexForRowAndColumn(row + 1, column, count, kGridCols);
    return;
  }

  if (row == 0) {
    selectedIndex = clampedIndexForRowAndColumn(lastRow, column, count, kGridCols);
    return;
  }

  selectedIndex = clampedIndexForRowAndColumn(row - 1, column, count, kGridCols);
}

void DashboardActivity::loop() {
  layoutCells();

  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap()) {
      for (int idx = 0; idx < itemCount(); ++idx) {
        if (TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, cellRects[idx])) {
          mappedInput.suppressTouchButtonFallback();
          selectedIndex = idx;
#if MOFEI_TOUCH_DEBUG
          logDashboardTouchHit(touchEvent, "shortcut", idx, cellRects[idx]);
#endif
          openCurrentSelection();
          return;
        }
      }
#if MOFEI_DEVICE
      uint8_t buttonIndex = 0;
      if (mapButtonHintTap(touchEvent, &buttonIndex)) {
        mappedInput.suppressTouchButtonFallback();
        if (buttonIndex == HalGPIO::BTN_CONFIRM) {
          openCurrentSelection();
          return;
        }
        if (buttonIndex == HalGPIO::BTN_LEFT) {
          if (dashboardUsesListLayout()) {
            moveSelectionVertically(-1);
          } else {
            moveSelectionHorizontally(-1);
          }
        } else if (buttonIndex == HalGPIO::BTN_RIGHT) {
          if (dashboardUsesListLayout()) {
            moveSelectionVertically(1);
          } else {
            moveSelectionHorizontally(1);
          }
        } else if (buttonIndex == HalGPIO::BTN_UP) {
          moveSelectionVertically(-1);
        } else if (buttonIndex == HalGPIO::BTN_DOWN) {
          moveSelectionVertically(1);
        }
        requestUpdate();
        return;
      }
#endif

      if (!mappedInput.isTouchButtonHintTap(touchEvent)) {
        mappedInput.suppressTouchButtonFallback();
#if MOFEI_TOUCH_DEBUG
        logDashboardTouchMiss(touchEvent, itemCount());
#endif
        return;
      }
    } else {
      if (touchEvent.type == InputTouchEvent::Type::SwipeLeft) {
        mappedInput.suppressTouchButtonFallback();
        if (dashboardUsesListLayout()) {
          moveSelectionVertically(1);
        } else {
          moveSelectionHorizontally(1);
        }
        requestUpdate();
        return;
      }
      if (touchEvent.type == InputTouchEvent::Type::SwipeRight) {
        mappedInput.suppressTouchButtonFallback();
        if (dashboardUsesListLayout()) {
          moveSelectionVertically(-1);
        } else {
          moveSelectionHorizontally(-1);
        }
        requestUpdate();
        return;
      }
      if (touchEvent.type == InputTouchEvent::Type::SwipeUp) {
        mappedInput.suppressTouchButtonFallback();
        moveSelectionVertically(1);
        requestUpdate();
        return;
      }
      if (touchEvent.type == InputTouchEvent::Type::SwipeDown) {
        mappedInput.suppressTouchButtonFallback();
        moveSelectionVertically(-1);
        requestUpdate();
        return;
      }
      if (!mappedInput.isTouchButtonHintTap(touchEvent)) {
        mappedInput.suppressTouchButtonFallback();
        return;
      }
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Right)) {
    if (dashboardUsesListLayout()) {
      moveSelectionVertically(1);
    } else {
      moveSelectionHorizontally(1);
    }
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left)) {
    if (dashboardUsesListLayout()) {
      moveSelectionVertically(-1);
    } else {
      moveSelectionHorizontally(-1);
    }
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    moveSelectionVertically(1);
    requestUpdate();
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    moveSelectionVertically(-1);
    requestUpdate();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openCurrentSelection();
  }
}

void DashboardActivity::render(RenderLock&& lock) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  layoutCells();
  const int count = itemCount();

  // Prewarm font cache to prevent extreme cache thrashing with CJK characters
  if (auto* fcm = renderer.getFontCacheManager()) {
    auto scope = fcm->createPrewarmScope();
    std::string allText = tr(STR_MOFEI_DASHBOARD_TITLE);
    for (int idx = 0; idx < count; ++idx) {
      const auto id = DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(idx));
      const auto* definition = DashboardShortcutStore::definitionFor(id);
      if (definition) allText += I18N.get(definition->labelId);
      if (!dashboardUsesIosGridLayout()) {
        allText += subtitleForShortcut(id);
      }
    }
    const auto labels = dashboardUsesListLayout()
                            ? mappedInput.mapLabels("", tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN))
                            : mappedInput.mapLabels("", tr(STR_OPEN), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
    allText += std::string(labels.btn1) + labels.btn2 + labels.btn3 + labels.btn4;

    fcm->prewarmCache(UI_10_FONT_ID, allText.c_str(), (1 << EpdFontFamily::REGULAR) | (1 << EpdFontFamily::BOLD));
    fcm->prewarmCache(SMALL_FONT_ID, allText.c_str(), (1 << EpdFontFamily::REGULAR));
  }

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MOFEI_DASHBOARD_TITLE));

  if (dashboardUsesListLayout()) {
    GUI.drawList(
        renderer, dashboardListRect(renderer, metrics), selectionCount(), selectedIndex,
        [this](int index) {
          const auto id = DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(index));
          const auto* definition = DashboardShortcutStore::definitionFor(id);
          return definition != nullptr ? std::string(I18N.get(definition->labelId)) : std::string();
        },
        [this](int index) { return subtitleForShortcut(DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(index))); },
        nullptr, [this](int index) { return std::to_string(index + kDashboardListTitleReserveRows); }, true);

    const auto labels = mappedInput.mapLabels("", tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer();
    return;
  }

  // === Grid cells (3 columns, dynamic row count) ===
  const bool iosGrid = dashboardUsesIosGridLayout();
  for (int idx = 0; idx < count; ++idx) {
    const Rect& cell = cellRects[idx];
    if (cell.width <= 0 || cell.height <= 0) continue;

    const bool selected = (idx == selectedIndex);
    const int borderWidth = selected ? 2 : 1;
    renderer.drawRoundedRect(cell.x, cell.y, cell.width, cell.height, borderWidth, kCellCornerRadiusPx, true);
    if (selected) {
      // Inner ring to reinforce focus on a 1-bit display.
      renderer.drawRoundedRect(cell.x + 3, cell.y + 3, cell.width - 6, cell.height - 6, 1, kCellCornerRadiusPx, true);
    }

    const auto id = DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(idx));
    const auto* definition = DashboardShortcutStore::definitionFor(id);
    const char* label = definition != nullptr ? I18N.get(definition->labelId) : "";

    if (iosGrid) {
      const int contentX = cell.x + kDashboardCellBorderInsetPx;
      const int contentY = cell.y + kDashboardCellBorderInsetPx;
      const int contentWidth = std::max(cell.width - 2 * kDashboardCellBorderInsetPx, 0);
      const int contentHeight = std::max(cell.height - 2 * kDashboardCellBorderInsetPx, 0);
      const int labelLineH = renderer.getLineHeight(UI_10_FONT_ID);
      const int labelBlockHeight = labelLineH + kDashboardCellBottomInsetPx;
      const int labelTextW = std::max(contentWidth - 2 * kCellInnerPadPx, 0);
      const int labelBlockTop = contentY + std::max(contentHeight - labelBlockHeight, 0);
      std::string truncLabel = renderer.truncatedText(UI_10_FONT_ID, label, labelTextW, EpdFontFamily::BOLD);
      const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, truncLabel.c_str(), EpdFontFamily::BOLD);
      const int labelDrawX = contentX + std::max((labelTextW - labelWidth) / 2, 0);
      renderer.drawText(UI_10_FONT_ID, labelDrawX, labelBlockTop, truncLabel.c_str(), true, EpdFontFamily::BOLD);

      const int iconRegionTop = contentY;
      const int iconRegionBottom = labelBlockTop - kDashboardLabelSpacingPx;
      const int iconRegionHeight = std::max(iconRegionBottom - iconRegionTop, kDashboardIosGridIconSizePx);
      const int iconX = contentX + std::max((contentWidth - kDashboardIosGridIconSizePx) / 2, 0);
      const int iconY = iconRegionTop + std::max((iconRegionHeight - kDashboardIosGridIconSizePx) / 2, 0);
      UIIcon iconId = definition != nullptr ? definition->icon : UIIcon::Settings;
      if (const uint8_t* iconBitmap = iconBitmap64ForName(iconId); iconBitmap != nullptr) {
        renderer.drawIcon(iconBitmap, iconX, iconY, kDashboardIosGridIconSizePx, kDashboardIosGridIconSizePx);
      }
    } else {
      const std::string subtitle = subtitleForShortcut(id);
      const int labelTextW = std::max(cell.width - 2 * kCellInnerPadPx, 0);
      const int labelLineH = renderer.getLineHeight(UI_10_FONT_ID);
      const int gap = subtitle.empty() ? 0 : kDashboardLabelSpacingPx;
      const auto subtitleLines = wrapSubtitle(renderer, subtitle, labelTextW, kDashboardGridSubtitleMaxLines);
      const int subLineH = subtitleLines.empty() ? 0 : renderer.getLineHeight(SMALL_FONT_ID);
      const int totalTextH = labelLineH + gap + subLineH * static_cast<int>(subtitleLines.size());
      const int startY = cell.y + std::max((cell.height - totalTextH) / 2, 0);

      const int labelTextX = cell.x + kCellInnerPadPx;
      std::string truncLabel = renderer.truncatedText(UI_10_FONT_ID, label, labelTextW, EpdFontFamily::BOLD);
      const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, truncLabel.c_str(), EpdFontFamily::BOLD);
      const int labelDrawX = labelWidth < labelTextW ? labelTextX + (labelTextW - labelWidth) / 2 : labelTextX;
      renderer.drawText(UI_10_FONT_ID, labelDrawX, startY, truncLabel.c_str(), true, EpdFontFamily::BOLD);

      int subY = startY + labelLineH + gap;
      for (const auto& line : subtitleLines) {
        const int subWidth = renderer.getTextWidth(SMALL_FONT_ID, line.c_str());
        const int subDrawX = subWidth < labelTextW ? labelTextX + (labelTextW - subWidth) / 2 : labelTextX;
        renderer.drawText(SMALL_FONT_ID, subDrawX, subY, line.c_str(), true);
        subY += subLineH;
      }
    }
  }

  const auto labels = mappedInput.mapLabels("", tr(STR_OPEN), tr(STR_DIR_LEFT), tr(STR_DIR_RIGHT));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
