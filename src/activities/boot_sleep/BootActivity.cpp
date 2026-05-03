#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include "fontIds.h"
#include "images/Logo120.h"

void BootActivity::onEnter() {
  Activity::onEnter();

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();

  renderer.clearScreen();
  renderer.drawImage(Logo120, (pageWidth - 120) / 2, (pageHeight - 120) / 2, 120, 120);
  const int centerY = pageHeight / 2;
  renderer.drawCenteredText(UI_10_FONT_ID, renderer.getTextYForCentering(centerY + 70, 0, UI_10_FONT_ID), tr(STR_CROSSPOINT), true, EpdFontFamily::BOLD);
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getTextYForCentering(centerY + 95, 0, SMALL_FONT_ID), tr(STR_BOOTING));
  renderer.drawCenteredText(SMALL_FONT_ID, renderer.getTextYForCentering(pageHeight - 30, 0, SMALL_FONT_ID), CROSSPOINT_VERSION);
  renderer.displayBuffer();
}
