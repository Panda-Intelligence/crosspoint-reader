#pragma once

#include <string>
#include <vector>

#include "StudyDeckStore.h"

struct StudyQueuedCard {
  std::string front;
  std::string back;
  std::string deckName;
  uint16_t count = 0;
};

enum class StudyQueueKind : uint8_t { Again, Later, Saved };

class StudyReviewQueueStore {
  static StudyReviewQueueStore instance;
  std::vector<StudyQueuedCard> againCards;
  std::vector<StudyQueuedCard> laterCards;
  std::vector<StudyQueuedCard> savedCards;

  static constexpr int kMaxItemsPerQueue = 40;

  static StudyQueuedCard toQueuedCard(const StudyCard& card);
  static void upsert(std::vector<StudyQueuedCard>& queue, const StudyCard& card);
  std::vector<StudyQueuedCard>& mutableQueue(StudyQueueKind kind);
  const std::vector<StudyQueuedCard>& queue(StudyQueueKind kind) const;

 public:
  static StudyReviewQueueStore& getInstance() { return instance; }

  bool loadFromFile();
  bool saveToFile() const;
  void recordAgain(const StudyCard& card);
  void recordLater(const StudyCard& card);
  void recordSaved(const StudyCard& card);
  bool removeAt(StudyQueueKind kind, int index);
  void clearQueue(StudyQueueKind kind);

  const std::vector<StudyQueuedCard>& getCards(StudyQueueKind kind) const { return queue(kind); }
  int getAgainCount() const { return static_cast<int>(againCards.size()); }
  int getLaterCount() const { return static_cast<int>(laterCards.size()); }
  int getSavedCount() const { return static_cast<int>(savedCards.size()); }
};

#define STUDY_REVIEW_QUEUE StudyReviewQueueStore::getInstance()
