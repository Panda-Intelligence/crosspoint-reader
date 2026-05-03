#include "StickyNotesActivity.h"

#include <I18n.h>

#include <algorithm>

#include "StickyNotesStore.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "fontIds.h"
#include "util/TouchHitTest.h"

int StickyNotesActivity::itemCount() const { return static_cast<int>(STICKY_NOTES.getCount()); }

void StickyNotesActivity::onEnter() {
  Activity::onEnter();
  STICKY_NOTES.loadFromFile();
  selectedIndex = 0;
  showingDetail = false;
  requestUpdate();
}

void StickyNotesActivity::clampSelection() {
  const int count = itemCount();
  if (count <= 0) {
    selectedIndex = 0;
    showingDetail = false;
    return;
  }
  selectedIndex = std::clamp(selectedIndex, 0, count - 1);
}

void StickyNotesActivity::handleKeyboardResult(const ActivityResult& result, const int editIndex) {
  if (!result.isCancelled && std::holds_alternative<KeyboardResult>(result.data)) {
    const auto& keyboard = std::get<KeyboardResult>(result.data);
    if (editIndex >= 0) {
      STICKY_NOTES.updateNote(static_cast<size_t>(editIndex), keyboard.text);
      selectedIndex = editIndex;
    } else {
      STICKY_NOTES.addNote(keyboard.text);
      selectedIndex = 0;
    }
  }
  STICKY_NOTES.loadFromFile();
  clampSelection();
}

void StickyNotesActivity::addNote() {
  auto keyboard = std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_NOTES_NEW_TITLE), "",
                                                          StickyNotesStore::kMaxTextLength);
  startActivityForResult(std::move(keyboard),
                         [this](const ActivityResult& result) { handleKeyboardResult(result, -1); });
}

void StickyNotesActivity::editSelected() {
  const auto& notes = STICKY_NOTES.getNotes();
  if (notes.empty()) {
    return;
  }
  clampSelection();
  const int editIndex = selectedIndex;
  auto keyboard = std::make_unique<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_NOTES_EDIT_TITLE),
                                                          notes[editIndex].text, StickyNotesStore::kMaxTextLength);
  startActivityForResult(std::move(keyboard),
                         [this, editIndex](const ActivityResult& result) { handleKeyboardResult(result, editIndex); });
}

void StickyNotesActivity::toggleSelected() {
  if (itemCount() <= 0) {
    return;
  }
  clampSelection();
  if (STICKY_NOTES.toggleDone(static_cast<size_t>(selectedIndex))) {
    requestUpdate();
  }
}

void StickyNotesActivity::deleteCompleted() {
  if (STICKY_NOTES.deleteCompleted() > 0) {
    clampSelection();
    requestUpdate();
  }
}

void StickyNotesActivity::loop() {
  InputTouchEvent touchEvent;
  if (mappedInput.consumeTouchEvent(&touchEvent, renderer)) {
    const int count = itemCount();
    const bool buttonHintTap = mappedInput.isTouchButtonHintTap(touchEvent);
    if (!buttonHintTap && touchEvent.isTap()) {
      const auto& metrics = UITheme::getInstance().getMetrics();
      const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
      const int contentHeight =
          renderer.getScreenHeight() - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
      const Rect listRect{0, contentTop, renderer.getScreenWidth(), contentHeight};
      const int clickedIndex = TouchHitTest::listItemAt(listRect, metrics.listWithSubtitleRowHeight, selectedIndex,
                                                        count, touchEvent.x, touchEvent.y);
      if (clickedIndex >= 0) {
        mappedInput.suppressTouchButtonFallback();
        selectedIndex = clickedIndex;
        showingDetail = true;
        requestUpdate();
        return;
      }
      if (count == 0) {
        mappedInput.suppressTouchButtonFallback();
        addNote();
        return;
      }
    } else if (!buttonHintTap && count > 0 && TouchHitTest::isForwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, count);
      showingDetail = false;
      requestUpdate();
      return;
    } else if (!buttonHintTap && count > 0 && TouchHitTest::isBackwardSwipe(touchEvent)) {
      mappedInput.suppressTouchButtonFallback();
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, count);
      showingDetail = false;
      requestUpdate();
      return;
    }
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    if (showingDetail) {
      showingDetail = false;
      requestUpdate();
      return;
    }
    finish();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) {
    if (itemCount() > 0) {
      toggleSelected();
    } else {
      addNote();
    }
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Left)) {
    editSelected();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Right)) {
    addNote();
    return;
  }

  if (mappedInput.wasPressed(MappedInputManager::Button::Up)) {
    deleteCompleted();
    return;
  }

  const int count = itemCount();
  if (count > 0 && !showingDetail) {
    buttonNavigator.onNextRelease([this, count] {
      selectedIndex = ButtonNavigator::nextIndex(selectedIndex, count);
      requestUpdate();
    });

    buttonNavigator.onPreviousRelease([this, count] {
      selectedIndex = ButtonNavigator::previousIndex(selectedIndex, count);
      requestUpdate();
    });
  }
}

void StickyNotesActivity::render(RenderLock&&) {
  renderer.clearScreen();

  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageWidth = renderer.getScreenWidth();
  const int pageHeight = renderer.getScreenHeight();
  const int contentTop = metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int contentHeight = pageHeight - contentTop - metrics.buttonHintsHeight - metrics.verticalSpacing * 2;
  const int count = itemCount();

  char subtitle[48];
  snprintf(subtitle, sizeof(subtitle), "%u %s  %u %s", static_cast<unsigned>(STICKY_NOTES.getOpenCount()),
           tr(STR_NOTES_OPEN), static_cast<unsigned>(STICKY_NOTES.getDoneCount()), tr(STR_NOTES_DONE));
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, tr(STR_NOTES_TITLE), subtitle);

  if (count == 0) {
    const int cardH = 132;
    const int cardY = contentTop + (contentHeight - cardH) / 2;
    const int pad = metrics.contentSidePadding;
    const int h = renderer.getLineHeight(UI_10_FONT_ID);
    const int titleY = renderer.getTextYForCentering(cardY, cardH / 2, UI_10_FONT_ID);
    renderer.drawRoundedRect(pad, cardY, pageWidth - pad * 2, cardH, 1, 8, true);
    renderer.drawCenteredText(UI_10_FONT_ID, titleY, tr(STR_NOTES_EMPTY_TITLE), true, EpdFontFamily::BOLD);
    renderer.drawLine(pad + 18, cardY + cardH / 2, pageWidth - pad - 18, cardY + cardH / 2, true);
    renderer.drawCenteredText(SMALL_FONT_ID, renderer.getTextYForCentering(cardY + cardH / 2, cardH / 2, SMALL_FONT_ID),
                              tr(STR_NOTES_EMPTY_HINT));
  } else {
    const auto& notes = STICKY_NOTES.getNotes();
    GUI.drawList(
        renderer, Rect{0, contentTop, pageWidth, contentHeight}, count, selectedIndex,
        [&notes](int index) {
          const auto& note = notes[static_cast<size_t>(index)];
          std::string title = note.done ? "[x] " : "[ ] ";
          title += note.text;
          return title;
        },
        [&notes](int index) {
          const auto& note = notes[static_cast<size_t>(index)];
          return std::string(note.done ? tr(STR_NOTES_DONE_SUBTITLE) : tr(STR_NOTES_OPEN_SUBTITLE));
        });

    if (showingDetail) {
      const int pad = metrics.contentSidePadding;
      const int detailTop = contentTop + 22;
      const int detailH = std::min(176, contentHeight - 18);
      renderer.fillRoundedRect(pad, detailTop, pageWidth - pad * 2, detailH, 8, Color::White);
      renderer.drawRoundedRect(pad, detailTop, pageWidth - pad * 2, detailH, 1, 8, true);
      const auto& note = notes[static_cast<size_t>(std::clamp(selectedIndex, 0, count - 1))];
      renderer.drawText(SMALL_FONT_ID, pad + 14, detailTop + 12,
                        note.done ? tr(STR_NOTES_DONE_SUBTITLE) : tr(STR_NOTES_OPEN_SUBTITLE));
      const auto lines = renderer.wrappedText(UI_10_FONT_ID, note.text.c_str(), pageWidth - pad * 2 - 28, 4);
      for (size_t i = 0; i < lines.size(); i++) {
        renderer.drawText(UI_10_FONT_ID, pad + 14, detailTop + 42 + static_cast<int>(i) * 24, lines[i].c_str(), true,
                          EpdFontFamily::BOLD);
      }
      renderer.drawText(SMALL_FONT_ID, pad + 14, detailTop + detailH - 28, tr(STR_NOTES_DETAIL_HINT));
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), count > 0 ? tr(STR_NOTES_TOGGLE) : tr(STR_NOTES_ADD),
                                            tr(STR_NOTES_EDIT), tr(STR_NOTES_ADD));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
