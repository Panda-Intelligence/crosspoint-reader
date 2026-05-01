#pragma once

#include "../Activity.h"
#include "DashboardShortcutStore.h"
#include "util/ButtonNavigator.h"

class DashboardActivity final : public Activity {
  ButtonNavigator buttonNavigator;
  int selectedIndex = 0;

  int itemCount() const;
  std::string subtitleForShortcut(DashboardShortcutId id) const;
  void openCurrentSelection();

 public:
  explicit DashboardActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Dashboard", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
};
