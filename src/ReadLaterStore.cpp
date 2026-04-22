#include "ReadLaterStore.h"

#include <HalStorage.h>

#include <algorithm>

namespace {
constexpr char READ_LATER_DIR[] = "/.mofei/reading/read_later";
constexpr char READ_LATER_SEED_FILE[] = "/.mofei/reading/read_later/welcome.txt";
constexpr char READ_LATER_SEED_CONTENT[] =
    "Welcome to Read Later\n\n"
    "This is the first local Read Later article on the mofei branch.\n\n"
    "The next step is to extend the device-side import flow so web uploads and companion pushes\n"
    "can save clean article text into this folder.\n";
}  // namespace

ReadLaterStore ReadLaterStore::instance;

bool ReadLaterStore::ensureSeedFile() {
  Storage.mkdir("/.mofei");
  Storage.mkdir("/.mofei/reading");
  Storage.mkdir(READ_LATER_DIR);
  if (!Storage.exists(READ_LATER_SEED_FILE)) {
    return Storage.writeFile(READ_LATER_SEED_FILE, READ_LATER_SEED_CONTENT);
  }
  return true;
}

void ReadLaterStore::refresh() {
  ensureSeedFile();
  items.clear();

  const auto files = Storage.listFiles(READ_LATER_DIR);
  for (const auto& file : files) {
    std::string name = file.c_str();
    if (name.empty()) continue;
    if (!name.empty() && name.back() == '/') continue;
    std::string path = std::string(READ_LATER_DIR) + "/" + name;
    items.push_back({name, path});
  }

  std::sort(items.begin(), items.end(), [](const ReadLaterItem& a, const ReadLaterItem& b) {
    return a.filename < b.filename;
  });
}
