#pragma once

#include "InputTouchEvent.h"
#include "components/themes/BaseTheme.h"

namespace TouchHitTest {

enum class ReaderAction { None, PreviousPage, NextPage, Menu };

inline bool pointInRect(uint16_t x, uint16_t y, const Rect& rect) {
  if (rect.width <= 0 || rect.height <= 0) {
    return false;
  }
  return x >= rect.x && x < rect.x + rect.width && y >= rect.y && y < rect.y + rect.height;
}

inline int listItemAt(const Rect& listRect, int rowHeight, int selectedIndex, int itemCount, uint16_t x, uint16_t y) {
  if (rowHeight <= 0 || itemCount <= 0 || !pointInRect(x, y, listRect)) {
    return -1;
  }

  const int pageItems = listRect.height / rowHeight;
  if (pageItems <= 0) {
    return -1;
  }

  const int clickedRow = (y - listRect.y) / rowHeight;
  if (clickedRow < 0 || clickedRow >= pageItems) {
    return -1;
  }

  int visibleIndex = selectedIndex;
  if (visibleIndex < 0) {
    visibleIndex = 0;
  } else if (visibleIndex >= itemCount) {
    visibleIndex = itemCount - 1;
  }
  const int pageStartIndex = (visibleIndex / pageItems) * pageItems;
  const int clickedIndex = pageStartIndex + clickedRow;
  return clickedIndex < itemCount ? clickedIndex : -1;
}

inline bool isForwardSwipe(const InputTouchEvent& event) {
  return event.type == InputTouchEvent::Type::SwipeUp || event.type == InputTouchEvent::Type::SwipeLeft;
}

inline bool isBackwardSwipe(const InputTouchEvent& event) {
  return event.type == InputTouchEvent::Type::SwipeDown || event.type == InputTouchEvent::Type::SwipeRight;
}

inline ReaderAction readerActionForTouch(const InputTouchEvent& event, const Rect& screenRect) {
  if (isForwardSwipe(event)) {
    return ReaderAction::NextPage;
  }
  if (isBackwardSwipe(event)) {
    return ReaderAction::PreviousPage;
  }
  if (!event.isTap() || !pointInRect(event.x, event.y, screenRect)) {
    return ReaderAction::None;
  }

  const int leftBoundary = screenRect.x + screenRect.width / 3;
  const int rightBoundary = screenRect.x + (screenRect.width * 2) / 3;
  if (event.x < leftBoundary) {
    return ReaderAction::PreviousPage;
  }
  if (event.x >= rightBoundary) {
    return ReaderAction::NextPage;
  }
  return ReaderAction::Menu;
}

}  // namespace TouchHitTest
