#pragma once

#include <string>
#include <vector>

struct ReadLaterItem {
  std::string filename;
  std::string path;
};

class ReadLaterStore {
  static ReadLaterStore instance;
  std::vector<ReadLaterItem> items;

 public:
  static ReadLaterStore& getInstance() { return instance; }

  const std::vector<ReadLaterItem>& getItems() const { return items; }
  void refresh();
  bool ensureSeedFile();
};

#define READ_LATER_STORE ReadLaterStore::getInstance()
