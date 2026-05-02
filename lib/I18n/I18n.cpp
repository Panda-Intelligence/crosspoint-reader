#include "I18n.h"

#include <HalStorage.h>
#include <HardwareSerial.h>
#include <Serialization.h>

#include "I18nStrings.h"

using namespace i18n_strings;

// Settings file path
static constexpr const char* SETTINGS_FILE = "/.crosspoint/language.bin";
static constexpr uint8_t SETTINGS_VERSION = 1;

I18n& I18n::getInstance() {
  static I18n instance;
  return instance;
}

const char* I18n::get(StrId id) const {
  const auto index = static_cast<size_t>(id);
  if (index >= static_cast<size_t>(StrId::_COUNT)) {
    return "???";
  }

  // Use generated helper function - no hardcoded switch needed!
  const char* const* strings = getStringArray(_language);
  return strings[index];
}

void I18n::setLanguage(Language lang) {
  if (lang >= Language::_COUNT) {
    return;
  }
  _language = lang;
  saveSettings();
}

const char* I18n::getLanguageName(Language lang) const {
  const auto index = static_cast<size_t>(lang);
  if (index >= static_cast<size_t>(Language::_COUNT)) {
    return "???";
  }
  return LANGUAGE_NAMES[index];
}

void I18n::saveSettings() {
  Storage.mkdir("/.crosspoint");

  if (Storage.exists(SETTINGS_FILE) && !Storage.remove(SETTINGS_FILE)) {
    Serial.printf("[I18N] Failed to replace settings file\n");
    return;
  }

  FsFile file;
  if (!Storage.openFileForWrite("I18N", SETTINGS_FILE, file)) {
    Serial.printf("[I18N] Failed to save settings\n");
    return;
  }

  const uint8_t lang = static_cast<uint8_t>(_language);
  const uint8_t payload[] = {SETTINGS_VERSION, lang};
  const size_t written = file.write(payload, sizeof(payload));
  file.flush();
  file.close();

  if (written != sizeof(payload)) {
    Serial.printf("[I18N] Settings write incomplete: wrote=%u expected=%u language=%d\n",
                  static_cast<unsigned>(written), static_cast<unsigned>(sizeof(payload)), static_cast<int>(_language));
    return;
  }

  Serial.printf("[I18N] Settings saved: language=%d\n", static_cast<int>(_language));
}

void I18n::loadSettings() {
  FsFile file;
  if (!Storage.openFileForRead("I18N", SETTINGS_FILE, file)) {
    Serial.printf("[I18N] No settings file, using default (English)\n");
    return;
  }

  constexpr size_t expectedSize = 2;
  const size_t actualSize = file.size();
  if (actualSize < expectedSize) {
    Serial.printf("[I18N] Settings file too small: size=%u expected=%u; keeping language=%d\n",
                  static_cast<unsigned>(actualSize), static_cast<unsigned>(expectedSize), static_cast<int>(_language));
    file.close();
    return;
  }

  uint8_t payload[expectedSize] = {};
  const int bytesRead = file.read(payload, sizeof(payload));
  file.close();
  if (bytesRead != static_cast<int>(sizeof(payload))) {
    Serial.printf("[I18N] Settings read incomplete: read=%d expected=%u; keeping language=%d\n", bytesRead,
                  static_cast<unsigned>(sizeof(payload)), static_cast<int>(_language));
    return;
  }

  const uint8_t version = payload[0];
  if (version != SETTINGS_VERSION) {
    Serial.printf("[I18N] Settings version mismatch: version=%u expected=%u; keeping language=%d\n",
                  static_cast<unsigned>(version), static_cast<unsigned>(SETTINGS_VERSION), static_cast<int>(_language));
    return;
  }

  const uint8_t lang = payload[1];
  if (lang < static_cast<uint8_t>(Language::_COUNT)) {
    _language = static_cast<Language>(lang);
    Serial.printf("[I18N] Loaded language: %d\n", static_cast<int>(_language));
  } else {
    Serial.printf("[I18N] Invalid language in settings: %u; keeping language=%d\n", static_cast<unsigned>(lang),
                  static_cast<int>(_language));
  }
}

// Generate character set for a specific language
const char* I18n::getCharacterSet(Language lang) {
  const auto langIndex = static_cast<size_t>(lang);
  if (langIndex >= static_cast<size_t>(Language::_COUNT)) {
    lang = Language::EN;  // Fallback to first language
  }

  return CHARACTER_SETS[static_cast<size_t>(lang)];
}
