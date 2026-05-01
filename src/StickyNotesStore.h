#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct StickyNote {
  uint32_t id = 0;
  std::string text;
  bool done = false;
};

class StickyNotesStore {
 public:
  static constexpr size_t kMaxNotes = 24;
  static constexpr size_t kMaxTextLength = 120;

  static StickyNotesStore& getInstance() { return instance; }

  bool loadFromFile();
  bool saveToFile() const;
  bool addNote(const std::string& text);
  bool updateNote(size_t index, const std::string& text);
  bool toggleDone(size_t index);
  bool deleteNote(size_t index);
  int deleteCompleted();
  void clear();

  const std::vector<StickyNote>& getNotes() const { return notes; }
  size_t getCount() const { return notes.size(); }
  size_t getOpenCount() const;
  size_t getDoneCount() const;

 private:
  static StickyNotesStore instance;

  std::vector<StickyNote> notes;
  uint32_t nextId = 1;

  static std::string normalizeText(const std::string& text);
  uint32_t allocateId();
};

#define STICKY_NOTES StickyNotesStore::getInstance()
