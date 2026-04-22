#include "ReadLaterStore.h"

#include <HalStorage.h>

#include <algorithm>

namespace {
constexpr char READ_LATER_DIR[] = "/.mofei/reading/read_later";
}  // namespace

ReadLaterStore ReadLaterStore::instance;

bool ReadLaterStore::ensureReadLaterDirectory() {
  Storage.mkdir("/.mofei");
  Storage.mkdir("/.mofei/reading");
  Storage.mkdir(READ_LATER_DIR);
  return true;
}

void ReadLaterStore::refresh() {
  ensureReadLaterDirectory();
  items.clear();

  const auto files = Storage.listFiles(READ_LATER_DIR);
  for (const auto& file : files) {
    std::string name = file.c_str();
    if (name.empty()) continue;
    if (!name.empty() && name.back() == '/') continue;

    std::string path = std::string(READ_LATER_DIR) + "/" + name;
    size_t bytes = 0;
    FsFile input;
    if (Storage.openFileForRead("RDL", path.c_str(), input)) {
      bytes = input.size();
      input.close();
    }
    items.push_back({name, path, bytes});
  }

  std::sort(items.begin(), items.end(),
            [](const ReadLaterItem& a, const ReadLaterItem& b) { return a.filename < b.filename; });
}
