#include "StudyCardsTodayActivity.h"

#include <I18n.h>

#include <algorithm>

#include "StudyStateStore.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
// Draw a pill-shaped progress bar
void drawProgressBar(GfxRenderer& renderer, int x, int y, int width, int height, int percent) {
  const int r = height / 2;
  renderer.drawRoundedRect(x, y, width, height, 1, r, true);
  const int fillWidth = (width - 2) * std::clamp(percent, 0, 100) / 100;
  if (fillWidth > 2) {
    renderer.fillRoundedRect(x + 1, y + 1, fillWidth, height - 2, r, Color::Black);
  }
}

// Draw a filled rounded button (primary - inverted)
void drawPrimaryButton(GfxRenderer& renderer, int x, int y, int w, int h, int fontId, const char* label) {
  renderer.fillRoundedRect(x, y, w, h, 10, Color::Black);
  const int tw = renderer.getTextWidth(fontId, label);
  const int th = renderer.getTextHeight(fontId);
  renderer.drawText(fontId, x + (w - tw) / 2, y + (h - th) / 2, label, false);
}

// Draw an outline rounded button (secondary)
void drawSecondaryButton(GfxRenderer& renderer, int x, int y, int w, int h, int fontId, const char* label) {
  renderer.drawRoundedRect(x, y, w, h, 1, 10, true);
  const int tw = renderer.getTextWidth(fontId, label);
  const int th = renderer.getTextHeight(fontId);
  renderer.drawText(fontId, x + (w - tw) / 2, y + (h - th) / 2, label, true);
}
}  // namespace

void StudyCardsTodayActivity::onEnter() {
  Activity::onEnter();
  STUDY_STATE.loadFromFile();
  inSession = false;
  actionIndex = 0;
  requestUpdate();
}

void StudyCardsTodayActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finish();
    return;
  }

  if (!inSession) {
    if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
      inSession = true;
      actionIndex = 0;
      requestUpdate();
    }
    return;
  }

  buttonNavigator.onNextRelease([this] {
    actionIndex = ButtonNavigator::nextIndex(actionIndex, 2);
    requestUpdate();
  });

  buttonNavigator.onPreviousRelease([this] {
    actionIndex = ButtonNavigator::previousIndex(actionIndex, 2);
    requestUpdate();
  });

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    STUDY_STATE.recordReviewResult(actionIndex == 0);
    if (STUDY_STATE.getState().completedToday >= STUDY_STATE.getState().dueToday) {
      inSession = false;
    }
    requestUpdate();
  }
}

void StudyCardsTodayActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const auto& state = STUDY_STATE.getState();

  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, "Study Cards");

  const int pad = metrics.contentSidePadding;
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentBottom = pageHeight - metrics.buttonHintsHeight - 8;
  const int total = state.dueToday > 0 ? static_cast<int>(state.dueToday) : 1;
  const int pct = std::clamp(static_cast<int>(state.completedToday) * 100 / total, 0, 100);
  const int totalReviewed = static_cast<int>(state.correctToday) + static_cast<int>(state.wrongToday);
  const int accuracy = totalReviewed > 0 ? static_cast<int>(state.correctToday) * 100 / totalReviewed : 0;

  if (!inSession) {
    // ── Pre-session layout ──────────────────────────────────────────────
    const int due = std::max(0, static_cast<int>(state.dueToday) - static_cast<int>(state.completedToday));

    // Paper stat card
    const int cardH = 170;
    const int cardY = contentTop + (contentBottom - contentTop - cardH) / 2 - 24;
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);

    // Large due count
    char dueStr[8];
    snprintf(dueStr, sizeof(dueStr), "%d", due);
    renderer.drawCenteredText(UI_12_FONT_ID, cardY + 18, dueStr, true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 68, "cards due today");

    // Thin divider
    renderer.drawLine(pad + 24, cardY + 96, pageWidth - pad - 24, cardY + 96, true);

    // Progress bar
    drawProgressBar(renderer, pad + 20, cardY + 110, pageWidth - (pad + 20) * 2, 10, pct);

    char pctStr[40];
    snprintf(pctStr, sizeof(pctStr), "%u done of %u", state.completedToday, state.dueToday);
    renderer.drawCenteredText(SMALL_FONT_ID, cardY + 132, pctStr);

    // Bottom status row (outside card)
    const int statusY = contentBottom - 16;
    char streakStr[32];
    snprintf(streakStr, sizeof(streakStr), "Streak %u days", state.streakDays);
    renderer.drawText(SMALL_FONT_ID, pad, statusY, streakStr);

    char accStr[24];
    snprintf(accStr, sizeof(accStr), "Accuracy %d%%", accuracy);
    const int accW = renderer.getTextWidth(SMALL_FONT_ID, accStr);
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - accW, statusY, accStr);

  } else {
    // ── In-session layout ───────────────────────────────────────────────
    const int remaining = std::max(0, static_cast<int>(state.dueToday) - static_cast<int>(state.completedToday));

    // Top meta row
    renderer.drawText(SMALL_FONT_ID, pad, contentTop, "Review");
    char remStr[32];
    snprintf(remStr, sizeof(remStr), "%d remaining", remaining);
    const int remW = renderer.getTextWidth(SMALL_FONT_ID, remStr);
    renderer.drawText(SMALL_FONT_ID, pageWidth - pad - remW, contentTop, remStr);

    // Paper card
    const int btnH = 48;
    const int btnGap = 10;
    const int btnAreaH = btnH + btnGap + 8;
    const int cardY = contentTop + 22;
    const int cardH = contentBottom - cardY - btnAreaH - 6;

    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 12, true);
    renderer.drawText(SMALL_FONT_ID, pad + 14, cardY + 14, "Word card");

    // Main card content
    renderer.drawCenteredText(UI_12_FONT_ID, cardY + 50, "Did you recall?", true, EpdFontFamily::BOLD);
    renderer.drawCenteredText(UI_10_FONT_ID, cardY + 94, "Grade your confidence below");

    // Progress bar inside card, near bottom
    const int barY = cardY + cardH - 22;
    drawProgressBar(renderer, pad + 14, barY, pageWidth - (pad + 14) * 2, 8, pct);

    // Action buttons
    const int btnY = contentBottom - btnH;
    const int btnW = (pageWidth - pad * 2 - 12) / 2;
    const int btn2X = pad + btnW + 12;

    if (actionIndex == 1) {
      // "Again" is selected → primary (filled)
      drawPrimaryButton(renderer, pad, btnY, btnW, btnH, UI_10_FONT_ID, "Again");
      drawSecondaryButton(renderer, btn2X, btnY, btnW, btnH, UI_10_FONT_ID, "Know it");
    } else {
      // "Know it" is selected → primary (filled)
      drawSecondaryButton(renderer, pad, btnY, btnW, btnH, UI_10_FONT_ID, "Again");
      drawPrimaryButton(renderer, btn2X, btnY, btnW, btnH, UI_10_FONT_ID, "Know it");
    }
  }

  const auto labels =
      mappedInput.mapLabels(tr(STR_BACK), inSession ? "Grade" : "Start", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
