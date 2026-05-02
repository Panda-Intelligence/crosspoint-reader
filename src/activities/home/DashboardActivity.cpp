#include "DashboardActivity.h"

#include <FontCacheManager.h>
#include <HalGPIO.h>
#include <I18n.h>
#include <Utf8.h>

#include <algorithm>
#include <cstdio>
#include <vector>

#include "DashboardShortcutStore.h"
#include "DesktopSummaryStore.h"
#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

namespace {
constexpr int kGridGapPx = 0;
constexpr int kCellInnerPadPx = 0;
constexpr int kCellCornerRadiusPx = 0;
constexpr int kCustomizeCornerRadiusPx = 4;

bool isGridIndex(int index) { return index >= 0 && index < DashboardActivity::kGridCellCount; }

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

#if MOFEI_DEVICE
bool dispatchButtonHintTap(const MappedInputManager& mappedInput, const InputTouchEvent& touchEvent, int& selectedIndex,
                           int itemCount, bool* shouldOpenSelection) {
  uint8_t buttonIndex = 0;
  if (!gpio.mapMofeiButtonHintTapToButton(touchEvent.sourceX(), touchEvent.sourceY(), &buttonIndex)) {
    return false;
  }
  mappedInput.suppressTouchButtonFallback();
  if (buttonIndex == HalGPIO::BTN_CONFIRM) {
    if (shouldOpenSelection != nullptr) {
      *shouldOpenSelection = true;
    }
  } else if (buttonIndex == HalGPIO::BTN_LEFT || buttonIndex == HalGPIO::BTN_UP) {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount);
  } else if (buttonIndex == HalGPIO::BTN_RIGHT || buttonIndex == HalGPIO::BTN_DOWN) {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount);
  }
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

std::string DashboardActivity::subtitleForShortcut(const DashboardShortcutId id) const {
  const auto& summary = DESKTOP_SUMMARY.getState();
  switch (id) {
    case DashboardShortcutId::WeatherClock:
      return summary.weatherLine;
    case DashboardShortcutId::Today:
      return summary.todaySecondary;
    case DashboardShortcutId::StudyToday:
      if (summary.loadedCards <= 0) {
        return tr(STR_DASHBOARD_STUDY_IMPORT_HINT);
      }
      if (summary.againCards > 0) {
        return formatDashboardValue(StrId::STR_DASHBOARD_STUDY_AGAIN_FORMAT, summary.againCards);
      }
      return summary.dueCards > 0 ? formatDashboardValue(StrId::STR_DASHBOARD_STUDY_DUE_FORMAT, summary.dueCards)
                                  : std::string(tr(STR_DASHBOARD_STUDY_CAUGHT_UP));
    case DashboardShortcutId::StudyHub:
      return formatDashboardValue(StrId::STR_DASHBOARD_STUDY_SUMMARY_FORMAT, summary.loadedCards, summary.laterCards);
    default: {
      const auto* definition = DashboardShortcutStore::definitionFor(id);
      return definition != nullptr ? std::string(I18N.get(definition->subtitleId)) : std::string();
    }
  }
}

void DashboardActivity::openCurrentSelection() {
  if (selectedIndex == kCustomizeIndex) {
    activityManager.goToDashboardCustomize();
    return;
  }
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
    case DashboardShortcutId::DesktopHub:
      activityManager.goToDesktopHub();
      break;
    case DashboardShortcutId::StudyHub:
    case DashboardShortcutId::StudyToday:
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
      requestUpdate();
      return;
  }
}

void DashboardActivity::layoutCells() {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int sidePad = std::max(metrics.contentSidePadding, 0);

  // Vertical band reserved for the grid + customize footer row.
  const int gridTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int customizeRowHeight = std::max(metrics.listRowHeight, 28);
  const int bandBottom =
      pageHeight - metrics.buttonHintsHeight - metrics.verticalSpacing - customizeRowHeight - metrics.verticalSpacing;
  const int bandHeight = std::max(bandBottom - gridTop, 0);

  const int usableWidth = std::max(pageWidth - 2 * sidePad - (kGridCols - 1) * kGridGapPx, 0);
  const int cellWidth = usableWidth / kGridCols;
  const int usableHeight = std::max(bandHeight - (kGridRows - 1) * kGridGapPx, 0);
  const int cellHeight = usableHeight / kGridRows;

  for (int row = 0; row < kGridRows; ++row) {
    for (int col = 0; col < kGridCols; ++col) {
      const int idx = row * kGridCols + col;
      const int x = sidePad + col * (cellWidth + kGridGapPx);
      const int y = gridTop + row * (cellHeight + kGridGapPx);
      cellRects[idx] = Rect{x, y, cellWidth, cellHeight};
    }
  }

  // Customize footer row spans full content width below the grid.
  const int customizeY = gridTop + kGridRows * cellHeight + (kGridRows - 1) * kGridGapPx + metrics.verticalSpacing;
  const int customizeWidth = std::max(pageWidth - 2 * sidePad, 0);
  cellRects[kCustomizeIndex] = Rect{sidePad, customizeY, customizeWidth, customizeRowHeight};
}

void DashboardActivity::loop() {
  layoutCells();

  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    if (touchEvent.isTap()) {
      // Grid hit-test first.
      int row = -1;
      int col = -1;
      const Rect& topLeft = cellRects[0];
      const int gridWidth = kGridCols * topLeft.width + (kGridCols - 1) * kGridGapPx;
      const int gridHeight = kGridRows * topLeft.height + (kGridRows - 1) * kGridGapPx;
      const Rect gridRect{topLeft.x, topLeft.y, gridWidth, gridHeight};
      bool handled = false;
      if (TouchHitTest::gridCellAt(gridRect, kGridRows, kGridCols, touchEvent.x, touchEvent.y, &row, &col)) {
        const int idx = row * kGridCols + col;
        if (idx >= 0 && idx < kGridCellCount && TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, cellRects[idx])) {
          mappedInput.suppressTouchButtonFallback();
          selectedIndex = idx;
          openCurrentSelection();
          return;
        }
      }
      if (!handled && TouchHitTest::pointInRect(touchEvent.x, touchEvent.y, cellRects[kCustomizeIndex])) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = kCustomizeIndex;
        openCurrentSelection();
        return;
      }

#if MOFEI_DEVICE
      bool shouldOpenSelection = false;
      if (dispatchButtonHintTap(mappedInput, touchEvent, selectedIndex, itemCount(), &shouldOpenSelection)) {
        if (shouldOpenSelection) {
          openCurrentSelection();
          return;
        }
        requestUpdate();
        return;
      }
#endif

      if (!mappedInput.isTouchButtonHintTap(touchEvent)) {
        mappedInput.suppressTouchButtonFallback();
        return;
      }
    } else {
      const int total = itemCount();
      if (touchEvent.type == InputTouchEvent::Type::SwipeLeft) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = (selectedIndex + 1) % total;
        requestUpdate();
        return;
      }
      if (touchEvent.type == InputTouchEvent::Type::SwipeRight) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = (selectedIndex - 1 + total) % total;
        requestUpdate();
        return;
      }
      if (touchEvent.type == InputTouchEvent::Type::SwipeUp) {
        mappedInput.suppressTouchButtonFallback();
        if (selectedIndex == kCustomizeIndex) {
          // Wrap to the top row.
          selectedIndex = 0;
        } else {
          const int next = selectedIndex + kGridCols;
          selectedIndex = next < kGridCellCount ? next : kCustomizeIndex;
        }
        requestUpdate();
        return;
      }
      if (touchEvent.type == InputTouchEvent::Type::SwipeDown) {
        mappedInput.suppressTouchButtonFallback();
        if (selectedIndex == kCustomizeIndex) {
          selectedIndex = kGridCellCount - 1;
        } else {
          const int prev = selectedIndex - kGridCols;
          selectedIndex = prev >= 0 ? prev : kCustomizeIndex;
        }
        requestUpdate();
        return;
      }
      if (!mappedInput.isTouchButtonHintTap(touchEvent)) {
        mappedInput.suppressTouchButtonFallback();
        return;
      }
    }
  }

  buttonNavigator.onNextRelease([this] {
    selectedIndex = ButtonNavigator::nextIndex(selectedIndex, itemCount());
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    selectedIndex = ButtonNavigator::previousIndex(selectedIndex, itemCount());
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    openCurrentSelection();
  }
}

void DashboardActivity::render(RenderLock&& lock) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  layoutCells();

  // Prewarm font cache to prevent extreme cache thrashing with CJK characters
  if (auto* fcm = renderer.getFontCacheManager()) {
    auto scope = fcm->createPrewarmScope();
    std::string allText = tr(STR_MOFEI_DASHBOARD_TITLE);
    for (int idx = 0; idx < kGridCellCount; ++idx) {
      const auto id = DASHBOARD_SHORTCUTS.getShortcut(static_cast<size_t>(idx));
      const auto* definition = DashboardShortcutStore::definitionFor(id);
      if (definition) allText += I18N.get(definition->labelId);
      allText += subtitleForShortcut(id);
    }
    allText += tr(STR_DASHBOARD_CUSTOMIZE);
    const auto labels = mappedInput.mapLabels("", tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    allText += std::string(labels.btn1) + labels.btn2 + labels.btn3 + labels.btn4;

    fcm->prewarmCache(UI_10_FONT_ID, allText.c_str(), (1 << EpdFontFamily::REGULAR) | (1 << EpdFontFamily::BOLD));
    fcm->prewarmCache(SMALL_FONT_ID, allText.c_str(), (1 << EpdFontFamily::REGULAR));
  }

  renderer.clearScreen();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_MOFEI_DASHBOARD_TITLE));

  // === Grid cells (3×3) ===
  for (int idx = 0; idx < kGridCellCount; ++idx) {
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
    const std::string subtitle = subtitleForShortcut(id);

    // Use dynamic metrics for perfect centering
    const int labelTextW = std::max(cell.width - 2 * kCellInnerPadPx, 0);
    const int labelLineH = renderer.getLineHeight(UI_10_FONT_ID);
    const int subLineH = subtitle.empty() ? 0 : renderer.getLineHeight(SMALL_FONT_ID);
    const int gap = (subtitle.empty() || labelLineH == 0) ? 0 : 4;
    const auto subtitleLines = wrapSubtitle(renderer, subtitle, labelTextW, 2);
    const int totalTextH = labelLineH + gap + subLineH * static_cast<int>(subtitleLines.size());
    const int startY = cell.y + (cell.height - totalTextH) / 2;

    // Label centered horizontally and positioned in the vertical block.
    const int labelTextX = cell.x + kCellInnerPadPx;
    std::string truncLabel = renderer.truncatedText(UI_10_FONT_ID, label, labelTextW, EpdFontFamily::BOLD);
    const int labelWidth = renderer.getTextWidth(UI_10_FONT_ID, truncLabel.c_str(), EpdFontFamily::BOLD);
    const int labelDrawX = labelWidth < labelTextW ? labelTextX + (labelTextW - labelWidth) / 2 : labelTextX;
    renderer.drawText(UI_10_FONT_ID, labelDrawX, startY, truncLabel.c_str(), true, EpdFontFamily::BOLD);

    if (!subtitleLines.empty()) {
      int subY = startY + labelLineH + gap;
      for (const auto& line : subtitleLines) {
        const int subWidth = renderer.getTextWidth(SMALL_FONT_ID, line.c_str());
        const int subDrawX = subWidth < labelTextW ? labelTextX + (labelTextW - subWidth) / 2 : labelTextX;
        renderer.drawText(SMALL_FONT_ID, subDrawX, subY, line.c_str(), true);
        subY += subLineH;
      }
    }
  }

  // === Customize footer row ===
  const Rect& cust = cellRects[kCustomizeIndex];
  if (cust.width > 0 && cust.height > 0) {
    const bool selected = (selectedIndex == kCustomizeIndex);
    if (selected) {
      renderer.fillRoundedRect(cust.x, cust.y, cust.width, cust.height, kCustomizeCornerRadiusPx, Color::LightGray);
    }
    renderer.drawRoundedRect(cust.x, cust.y, cust.width, cust.height, selected ? 2 : 1, kCustomizeCornerRadiusPx, true);

    const char* customizeLabel = tr(STR_DASHBOARD_CUSTOMIZE);
    const int custTextW = std::max(cust.width - 2 * kCellInnerPadPx, 0);
    std::string truncCust = renderer.truncatedText(UI_10_FONT_ID, customizeLabel, custTextW, EpdFontFamily::BOLD);
    const int textY = cust.y + (cust.height - 14) / 2;
    const int textWidth = renderer.getTextWidth(UI_10_FONT_ID, truncCust.c_str(), EpdFontFamily::BOLD);
    const int textX = cust.x + std::max((cust.width - textWidth) / 2, kCellInnerPadPx);
    renderer.drawText(UI_10_FONT_ID, textX, textY, truncCust.c_str(), true, EpdFontFamily::BOLD);
  }

  const auto labels = mappedInput.mapLabels("", tr(STR_OPEN), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
