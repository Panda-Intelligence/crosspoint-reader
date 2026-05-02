#include "Activity.h"

#include <I18n.h>

#include "ActivityManager.h"

void Activity::onEnter() {
  LOG_INF("ACT", "Entering activity: %s (lang=%d)", name.c_str(), static_cast<int>(I18N.getLanguage()));
}

void Activity::onExit() {
  LOG_INF("ACT", "Exiting activity: %s (lang=%d)", name.c_str(), static_cast<int>(I18N.getLanguage()));
}

void Activity::requestUpdate(bool immediate) { activityManager.requestUpdate(immediate); }

void Activity::requestUpdateAndWait() { activityManager.requestUpdateAndWait(); }

void Activity::onGoHome() { activityManager.goHome(); }

void Activity::onSelectBook(const std::string& path) { activityManager.goToReader(path); }

void Activity::startActivityForResult(std::unique_ptr<Activity>&& activity, ActivityResultHandler resultHandler) {
  this->resultHandler = std::move(resultHandler);
  activityManager.pushActivity(std::move(activity));
}

void Activity::setResult(ActivityResult&& result) { this->result = std::move(result); }

void Activity::finish() { activityManager.popActivity(); }
