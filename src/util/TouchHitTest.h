#pragma once

#include <GfxRenderer.h>

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

inline bool gridCellAt(const Rect& gridRect, int rows, int cols, uint16_t x, uint16_t y, int* outRow, int* outCol) {
  if (rows <= 0 || cols <= 0 || outRow == nullptr || outCol == nullptr || !pointInRect(x, y, gridRect)) {
    return false;
  }

  const int cellW = gridRect.width / cols;
  const int cellH = gridRect.height / rows;
  if (cellW <= 0 || cellH <= 0) {
    return false;
  }

  const int col = (x - gridRect.x) / cellW;
  const int row = (y - gridRect.y) / cellH;
  if (row < 0 || row >= rows || col < 0 || col >= cols) {
    return false;
  }

  *outRow = row;
  *outCol = col;
  return true;
}

inline bool isForwardSwipe(const InputTouchEvent& event) {
  return event.type == InputTouchEvent::Type::SwipeUp || event.type == InputTouchEvent::Type::SwipeLeft;
}

inline bool isBackwardSwipe(const InputTouchEvent& event) {
  return event.type == InputTouchEvent::Type::SwipeDown || event.type == InputTouchEvent::Type::SwipeRight;
}

inline InputTouchEvent::Type transformSwipeForOrientation(InputTouchEvent::Type type,
                                                          GfxRenderer::Orientation orientation) {
  if (type == InputTouchEvent::Type::Tap || type == InputTouchEvent::Type::None) {
    return type;
  }

  switch (orientation) {
    case GfxRenderer::Orientation::Portrait:
      return type;
    case GfxRenderer::Orientation::PortraitInverted:
      if (type == InputTouchEvent::Type::SwipeLeft) return InputTouchEvent::Type::SwipeRight;
      if (type == InputTouchEvent::Type::SwipeRight) return InputTouchEvent::Type::SwipeLeft;
      if (type == InputTouchEvent::Type::SwipeUp) return InputTouchEvent::Type::SwipeDown;
      return InputTouchEvent::Type::SwipeUp;
    case GfxRenderer::Orientation::LandscapeClockwise:
      if (type == InputTouchEvent::Type::SwipeLeft) return InputTouchEvent::Type::SwipeUp;
      if (type == InputTouchEvent::Type::SwipeRight) return InputTouchEvent::Type::SwipeDown;
      if (type == InputTouchEvent::Type::SwipeUp) return InputTouchEvent::Type::SwipeRight;
      return InputTouchEvent::Type::SwipeLeft;
    case GfxRenderer::Orientation::LandscapeCounterClockwise:
      if (type == InputTouchEvent::Type::SwipeLeft) return InputTouchEvent::Type::SwipeDown;
      if (type == InputTouchEvent::Type::SwipeRight) return InputTouchEvent::Type::SwipeUp;
      if (type == InputTouchEvent::Type::SwipeUp) return InputTouchEvent::Type::SwipeLeft;
      return InputTouchEvent::Type::SwipeRight;
  }

  return type;
}

inline uint16_t clampTouchCoord(int value, int maxExclusive) {
  if (value < 0 || maxExclusive <= 0) {
    return 0;
  }
  if (value >= maxExclusive) {
    return static_cast<uint16_t>(maxExclusive - 1);
  }
  return static_cast<uint16_t>(value);
}

inline InputTouchEvent eventForRendererOrientation(const InputTouchEvent& event, const GfxRenderer& renderer) {
  InputTouchEvent oriented = event;
  const auto orientation = renderer.getOrientation();
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();

  switch (orientation) {
    case GfxRenderer::Orientation::Portrait:
      break;
    case GfxRenderer::Orientation::PortraitInverted:
      oriented.x = clampTouchCoord(width - 1 - static_cast<int>(event.x), width);
      oriented.y = clampTouchCoord(height - 1 - static_cast<int>(event.y), height);
      break;
    case GfxRenderer::Orientation::LandscapeClockwise:
      oriented.x = clampTouchCoord(width - 1 - static_cast<int>(event.y), width);
      oriented.y = clampTouchCoord(event.x, height);
      break;
    case GfxRenderer::Orientation::LandscapeCounterClockwise:
      oriented.x = clampTouchCoord(event.y, width);
      oriented.y = clampTouchCoord(height - 1 - static_cast<int>(event.x), height);
      break;
  }

  oriented.type = transformSwipeForOrientation(event.type, orientation);
  return oriented;
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
