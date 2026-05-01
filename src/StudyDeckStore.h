#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct StudyCard {
  std::string front;
  std::string back;
  std::string example;
  std::string deckName;
};

struct StudyDeckSummary {
  std::string filename;
  std::string title;
  size_t bytes = 0;
  int cardCount = 0;
  bool valid = false;
};

enum class StudyUserWordSaveResult : uint8_t { Added, AlreadyExists, Failed };

class StudyDeckStore {
  static StudyDeckStore instance;
  std::vector<StudyCard> cards;
  std::vector<StudyDeckSummary> deckSummaries;
  int deckCount = 0;
  int errorCount = 0;

  StudyDeckSummary loadDeckFile(const std::string& filename);

 public:
  static constexpr int kMaxCards = 64;

  static StudyDeckStore& getInstance() { return instance; }

  const std::vector<StudyCard>& getCards() const { return cards; }
  const std::vector<StudyDeckSummary>& getDeckSummaries() const { return deckSummaries; }
  int getDeckCount() const { return deckCount; }
  int getErrorCount() const { return errorCount; }
  bool hasCards() const { return !cards.empty(); }
  void refresh();
  StudyUserWordSaveResult addUserWord(const std::string& front, const std::string& back,
                                      const std::string& example = "");
};

#define STUDY_DECKS StudyDeckStore::getInstance()
