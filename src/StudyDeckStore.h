#pragma once

#include <cstddef>
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
};

#define STUDY_DECKS StudyDeckStore::getInstance()
