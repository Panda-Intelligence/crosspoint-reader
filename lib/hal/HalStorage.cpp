#define HAL_STORAGE_IMPL
#include "HalStorage.h"

#include <FS.h>  // need to be included before SdFat.h for compatibility with FS.h's File class
#include <HalGPIO.h>
#include <Logging.h>
#if !MOFEI_DEVICE
#include <SDCardManager.h>
#endif
#include <SD_MMC.h>

#include <cassert>
#include <cstring>

#if !MOFEI_DEVICE
#define SDCard SDCardManager::getInstance()
#endif

HalStorage HalStorage::instance;

namespace {

#if MOFEI_DEVICE
constexpr int MOFEI_SD_PWR = 10;
constexpr int MOFEI_SD_PWR_ENABLE_LEVEL = LOW;
constexpr int MOFEI_SD_D2 = 11;
constexpr int MOFEI_SD_D3 = 14;
constexpr int MOFEI_SD_CMD = 15;
constexpr int MOFEI_SD_CLK = 16;
constexpr int MOFEI_SD_D0 = 17;
constexpr int MOFEI_SD_D1 = 18;
constexpr int MOFEI_SD_CD = 21;
constexpr int MOFEI_SDMMC_FREQ_KHZ = 20000;
constexpr uint8_t MOFEI_SDMMC_MAX_OPEN_FILES = 8;

bool mofeiSdReady = false;

const char* fsModeFromFlags(const oflag_t oflag) {
  if ((oflag & O_APPEND) != 0) {
    return FILE_APPEND;
  }
  if ((oflag & O_TRUNC) != 0 || (oflag & O_CREAT) != 0 || isWriteMode(oflag)) {
    return FILE_WRITE;
  }
  return FILE_READ;
}

std::vector<String> listMofeiFiles(const char* path, const int maxFiles) {
  std::vector<String> ret;
  if (!mofeiSdReady) {
    LOG_ERR("SD", "Mofei SD_MMC not initialized, returning empty list");
    return ret;
  }

  File root = SD_MMC.open(path, FILE_READ);
  if (!root) {
    LOG_ERR("SD", "Failed to open directory: %s", path);
    return ret;
  }
  if (!root.isDirectory()) {
    LOG_ERR("SD", "Path is not a directory: %s", path);
    root.close();
    return ret;
  }

  int count = 0;
  for (File f = root.openNextFile(FILE_READ); f && count < maxFiles; f = root.openNextFile(FILE_READ)) {
    if (!f.isDirectory()) {
      ret.emplace_back(f.name());
      count++;
    }
    f.close();
  }
  root.close();
  return ret;
}

bool removeMofeiDir(const char* path) {
  File dir = SD_MMC.open(path, FILE_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) {
      dir.close();
    }
    return false;
  }

  for (File file = dir.openNextFile(FILE_READ); file; file = dir.openNextFile(FILE_READ)) {
    String filePath = path;
    if (!filePath.endsWith("/")) {
      filePath += "/";
    }
    filePath += file.name();

    const bool isDir = file.isDirectory();
    file.close();

    if (isDir) {
      if (!removeMofeiDir(filePath.c_str())) {
        dir.close();
        return false;
      }
    } else if (!SD_MMC.remove(filePath.c_str())) {
      dir.close();
      return false;
    }
  }

  dir.close();
  return SD_MMC.rmdir(path);
}
#endif

}  // namespace

HalStorage::HalStorage() {
  storageMutex = xSemaphoreCreateMutex();
  assert(storageMutex != nullptr);
}

// begin() and ready() are only called from setup, no need to acquire mutex for them

bool HalStorage::begin() {
#if MOFEI_DEVICE
  pinMode(MOFEI_SD_PWR, OUTPUT);
  digitalWrite(MOFEI_SD_PWR, MOFEI_SD_PWR_ENABLE_LEVEL);
  LOG_INF("SD", "Mofei SD power GPIO%d=%d", MOFEI_SD_PWR, MOFEI_SD_PWR_ENABLE_LEVEL);
  delay(200);

  pinMode(MOFEI_SD_CD, INPUT_PULLUP);
  const bool cardDetectLow = digitalRead(MOFEI_SD_CD) == LOW;
  LOG_INF("SD", "Mofei SD card detect GPIO%d=%d", MOFEI_SD_CD, cardDetectLow ? 0 : 1);

#ifndef MOFEI_SD_1BIT
#define MOFEI_SD_1BIT 0
#endif

  if (MOFEI_SD_1BIT) {
    if (!SD_MMC.setPins(MOFEI_SD_CLK, MOFEI_SD_CMD, MOFEI_SD_D0)) {
      LOG_ERR("SD", "Mofei SD_MMC (1-bit) setPins failed");
      mofeiSdReady = false;
      return false;
    }
  } else {
    if (!SD_MMC.setPins(MOFEI_SD_CLK, MOFEI_SD_CMD, MOFEI_SD_D0, MOFEI_SD_D1, MOFEI_SD_D2, MOFEI_SD_D3)) {
      LOG_ERR("SD", "Mofei SD_MMC (4-bit) setPins failed");
      mofeiSdReady = false;
      return false;
    }
  }

  mofeiSdReady = SD_MMC.begin("/sdcard", MOFEI_SD_1BIT != 0, false, MOFEI_SDMMC_FREQ_KHZ, MOFEI_SDMMC_MAX_OPEN_FILES);
  if (!mofeiSdReady) {
    LOG_ERR("SD", "Mofei SD_MMC mount failed");
    return false;
  }

  LOG_INF("SD", "Mofei SD_MMC mounted: type=%d size=%lluMB", static_cast<int>(SD_MMC.cardType()),
          SD_MMC.cardSize() / (1024ULL * 1024ULL));
  return true;
#else
  return SDCard.begin();
#endif
}

bool HalStorage::ready() const {
#if MOFEI_DEVICE
  return mofeiSdReady;
#else
  return SDCard.ready();
#endif
}

// For the rest of the methods, we acquire the mutex to ensure thread safety

class HalStorage::StorageLock {
 public:
  StorageLock() { xSemaphoreTake(HalStorage::getInstance().storageMutex, portMAX_DELAY); }
  ~StorageLock() { xSemaphoreGive(HalStorage::getInstance().storageMutex); }
};

std::vector<String> HalStorage::listFiles(const char* path, int maxFiles) {
  StorageLock lock;
#if MOFEI_DEVICE
  return listMofeiFiles(path, maxFiles);
#else
  return SDCard.listFiles(path, maxFiles);
#endif
}

String HalStorage::readFile(const char* path) {
  StorageLock lock;
#if MOFEI_DEVICE
  if (!mofeiSdReady) {
    LOG_ERR("SD", "Mofei SD_MMC not initialized; cannot read file");
    return "";
  }

  File f = SD_MMC.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) {
      f.close();
    }
    return "";
  }

  String content = "";
  constexpr size_t maxSize = 50000;
  size_t readSize = 0;
  while (f.available() && readSize < maxSize) {
    content += static_cast<char>(f.read());
    readSize++;
  }
  f.close();
  return content;
#else
  return SDCard.readFile(path);
#endif
}

bool HalStorage::readFileToStream(const char* path, Print& out, size_t chunkSize) {
  StorageLock lock;
#if MOFEI_DEVICE
  if (!mofeiSdReady) {
    LOG_ERR("SD", "Mofei SD_MMC not initialized; cannot stream file");
    return false;
  }

  File f = SD_MMC.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) {
      f.close();
    }
    return false;
  }

  constexpr size_t localBufSize = 256;
  uint8_t buf[localBufSize];
  const size_t toRead = (chunkSize == 0) ? localBufSize : (chunkSize < localBufSize ? chunkSize : localBufSize);
  while (f.available()) {
    const size_t r = f.read(buf, toRead);
    if (r > 0) {
      out.write(buf, r);
    } else {
      break;
    }
  }
  f.close();
  return true;
#else
  return SDCard.readFileToStream(path, out, chunkSize);
#endif
}

size_t HalStorage::readFileToBuffer(const char* path, char* buffer, size_t bufferSize, size_t maxBytes) {
  StorageLock lock;
#if MOFEI_DEVICE
  if (!buffer || bufferSize == 0) {
    return 0;
  }
  if (!mofeiSdReady) {
    buffer[0] = '\0';
    LOG_ERR("SD", "Mofei SD_MMC not initialized; cannot read file");
    return 0;
  }

  File f = SD_MMC.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    buffer[0] = '\0';
    if (f) {
      f.close();
    }
    return 0;
  }

  const size_t maxToRead = (maxBytes == 0) ? (bufferSize - 1) : min(maxBytes, bufferSize - 1);
  size_t total = 0;
  while (f.available() && total < maxToRead) {
    constexpr size_t chunk = 64;
    const size_t want = maxToRead - total;
    const size_t readLen = (want < chunk) ? want : chunk;
    const size_t r = f.read(reinterpret_cast<uint8_t*>(buffer + total), readLen);
    if (r > 0) {
      total += r;
    } else {
      break;
    }
  }

  buffer[total] = '\0';
  f.close();
  return total;
#else
  return SDCard.readFileToBuffer(path, buffer, bufferSize, maxBytes);
#endif
}

bool HalStorage::writeFile(const char* path, const String& content) {
  StorageLock lock;
#if MOFEI_DEVICE
  if (!mofeiSdReady) {
    LOG_ERR("SD", "Mofei SD_MMC not initialized; cannot write file");
    return false;
  }

  if (SD_MMC.exists(path)) {
    SD_MMC.remove(path);
  }
  File f = SD_MMC.open(path, FILE_WRITE, true);
  if (!f) {
    LOG_ERR("SD", "Failed to open file for write: %s", path);
    return false;
  }
  const size_t written = f.print(content);
  f.close();
  return written == content.length();
#else
  return SDCard.writeFile(path, content);
#endif
}

bool HalStorage::ensureDirectoryExists(const char* path) {
  StorageLock lock;
#if MOFEI_DEVICE
  if (!mofeiSdReady) {
    LOG_ERR("SD", "Mofei SD_MMC not initialized; cannot create directory");
    return false;
  }
  if (SD_MMC.exists(path)) {
    File dir = SD_MMC.open(path, FILE_READ);
    const bool ok = dir && dir.isDirectory();
    if (dir) {
      dir.close();
    }
    return ok;
  }
  return SD_MMC.mkdir(path);
#else
  return SDCard.ensureDirectoryExists(path);
#endif
}

class HalFile::Impl {
 public:
#if MOFEI_DEVICE
  explicit Impl(File&& arduinoFile) : arduinoFile(std::move(arduinoFile)) {}
#else
  explicit Impl(FsFile&& fsFile) : fsFile(std::move(fsFile)), backend(Backend::SdFat) {}
  explicit Impl(File&& arduinoFile) : arduinoFile(std::move(arduinoFile)), backend(Backend::ArduinoFs) {}

  enum class Backend { SdFat, ArduinoFs };

  FsFile fsFile;
#endif
  File arduinoFile;
#if !MOFEI_DEVICE
  Backend backend;

  bool isArduinoFs() const { return backend == Backend::ArduinoFs; }
#endif
};

HalFile::HalFile() = default;

HalFile::HalFile(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}

HalFile::~HalFile() = default;

HalFile::HalFile(HalFile&&) = default;

HalFile& HalFile::operator=(HalFile&&) = default;

HalFile HalStorage::open(const char* path, const oflag_t oflag) {
  StorageLock lock;  // ensure thread safety for the duration of this function
#if MOFEI_DEVICE
  return HalFile(std::make_unique<HalFile::Impl>(SD_MMC.open(path, fsModeFromFlags(oflag), (oflag & O_CREAT) != 0)));
#else
  return HalFile(std::make_unique<HalFile::Impl>(SDCard.open(path, oflag)));
#endif
}

bool HalStorage::mkdir(const char* path, const bool pFlag) {
  StorageLock lock;
#if MOFEI_DEVICE
  (void)pFlag;
  return mofeiSdReady && SD_MMC.mkdir(path);
#else
  return SDCard.mkdir(path, pFlag);
#endif
}

bool HalStorage::exists(const char* path) {
  StorageLock lock;
#if MOFEI_DEVICE
  return mofeiSdReady && SD_MMC.exists(path);
#else
  return SDCard.exists(path);
#endif
}

bool HalStorage::remove(const char* path) {
  StorageLock lock;
#if MOFEI_DEVICE
  return mofeiSdReady && SD_MMC.remove(path);
#else
  return SDCard.remove(path);
#endif
}
bool HalStorage::rename(const char* oldPath, const char* newPath) {
  StorageLock lock;
#if MOFEI_DEVICE
  return mofeiSdReady && SD_MMC.rename(oldPath, newPath);
#else
  return SDCard.rename(oldPath, newPath);
#endif
}

bool HalStorage::rmdir(const char* path) {
  StorageLock lock;
#if MOFEI_DEVICE
  return mofeiSdReady && SD_MMC.rmdir(path);
#else
  return SDCard.rmdir(path);
#endif
}

bool HalStorage::openFileForRead(const char* moduleName, const char* path, HalFile& file) {
  StorageLock lock;  // ensure thread safety for the duration of this function
#if MOFEI_DEVICE
  if (!mofeiSdReady || !SD_MMC.exists(path)) {
    LOG_ERR(moduleName, "File does not exist: %s", path);
    return false;
  }
  File arduinoFile = SD_MMC.open(path, FILE_READ);
  if (!arduinoFile || arduinoFile.isDirectory()) {
    if (arduinoFile) {
      arduinoFile.close();
    }
    LOG_ERR(moduleName, "Failed to open file for reading: %s", path);
    return false;
  }
  file = HalFile(std::make_unique<HalFile::Impl>(std::move(arduinoFile)));
  return true;
#else
  FsFile fsFile;
  bool ok = SDCard.openFileForRead(moduleName, path, fsFile);
  file = HalFile(std::make_unique<HalFile::Impl>(std::move(fsFile)));
  return ok;
#endif
}

bool HalStorage::openFileForRead(const char* moduleName, const std::string& path, HalFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool HalStorage::openFileForRead(const char* moduleName, const String& path, HalFile& file) {
  return openFileForRead(moduleName, path.c_str(), file);
}

bool HalStorage::openFileForWrite(const char* moduleName, const char* path, HalFile& file) {
  StorageLock lock;  // ensure thread safety for the duration of this function
#if MOFEI_DEVICE
  File arduinoFile = SD_MMC.open(path, FILE_WRITE, true);
  if (!arduinoFile) {
    LOG_ERR(moduleName, "Failed to open file for writing: %s", path);
    return false;
  }
  file = HalFile(std::make_unique<HalFile::Impl>(std::move(arduinoFile)));
  return true;
#else
  FsFile fsFile;
  bool ok = SDCard.openFileForWrite(moduleName, path, fsFile);
  file = HalFile(std::make_unique<HalFile::Impl>(std::move(fsFile)));
  return ok;
#endif
}

bool HalStorage::openFileForWrite(const char* moduleName, const std::string& path, HalFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool HalStorage::openFileForWrite(const char* moduleName, const String& path, HalFile& file) {
  return openFileForWrite(moduleName, path.c_str(), file);
}

bool HalStorage::removeDir(const char* path) {
  StorageLock lock;
#if MOFEI_DEVICE
  return mofeiSdReady && removeMofeiDir(path);
#else
  return SDCard.removeDir(path);
#endif
}

// HalFile implementation
// Allow doing file operations while ensuring thread safety via HalStorage's mutex.
// Please keep the list below in sync with the HalFile.h header

void HalFile::flush() {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  impl->arduinoFile.flush();
#else
  if (impl->isArduinoFs()) {
    impl->arduinoFile.flush();
  } else {
    impl->fsFile.flush();
  }
#endif
}

size_t HalFile::getName(char* name, size_t len) {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  const char* fileName = impl->arduinoFile.name();
  if (!fileName || len == 0) {
    return 0;
  }
  std::strncpy(name, fileName, len - 1);
  name[len - 1] = '\0';
  return std::strlen(name);
#else
  if (impl->isArduinoFs()) {
    const char* fileName = impl->arduinoFile.name();
    if (!fileName || len == 0) {
      return 0;
    }
    std::strncpy(name, fileName, len - 1);
    name[len - 1] = '\0';
    return std::strlen(name);
  }
  return impl->fsFile.getName(name, len);
#endif
}

size_t HalFile::size() {
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.size();
#else
  return impl->isArduinoFs() ? impl->arduinoFile.size() : impl->fsFile.size();
#endif
}

size_t HalFile::fileSize() {
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.size();
#else
  return impl->isArduinoFs() ? impl->arduinoFile.size() : impl->fsFile.fileSize();
#endif
}

bool HalFile::seek(size_t pos) {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.seek(pos);
#else
  return impl->isArduinoFs() ? impl->arduinoFile.seek(pos) : impl->fsFile.seekSet(pos);
#endif
}

bool HalFile::seekCur(int64_t offset) {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.seek(static_cast<uint32_t>(offset), SeekCur);
#else
  if (impl->isArduinoFs()) {
    return impl->arduinoFile.seek(static_cast<uint32_t>(offset), SeekCur);
  }
  return impl->fsFile.seekCur(offset);
#endif
}

bool HalFile::seekSet(size_t offset) {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.seek(offset);
#else
  return impl->isArduinoFs() ? impl->arduinoFile.seek(offset) : impl->fsFile.seekSet(offset);
#endif
}

int HalFile::available() const {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.available();
#else
  return impl->isArduinoFs() ? impl->arduinoFile.available() : impl->fsFile.available();
#endif
}

size_t HalFile::position() const {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.position();
#else
  return impl->isArduinoFs() ? impl->arduinoFile.position() : impl->fsFile.position();
#endif
}

int HalFile::read(void* buf, size_t count) {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return static_cast<int>(impl->arduinoFile.read(static_cast<uint8_t*>(buf), count));
#else
  if (impl->isArduinoFs()) {
    return static_cast<int>(impl->arduinoFile.read(static_cast<uint8_t*>(buf), count));
  }
  return impl->fsFile.read(buf, count);
#endif
}

int HalFile::read() {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.read();
#else
  return impl->isArduinoFs() ? impl->arduinoFile.read() : impl->fsFile.read();
#endif
}

size_t HalFile::write(const void* buf, size_t count) {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.write(static_cast<const uint8_t*>(buf), count);
#else
  return impl->isArduinoFs() ? impl->arduinoFile.write(static_cast<const uint8_t*>(buf), count)
                             : impl->fsFile.write(buf, count);
#endif
}

size_t HalFile::write(uint8_t b) {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.write(b);
#else
  return impl->isArduinoFs() ? impl->arduinoFile.write(b) : impl->fsFile.write(b);
#endif
}

bool HalFile::rename(const char* newPath) {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  const char* oldPath = impl->arduinoFile.path();
  if (!oldPath) {
    return false;
  }
  impl->arduinoFile.close();
  return SD_MMC.rename(oldPath, newPath);
#else
  if (impl->isArduinoFs()) {
    const char* oldPath = impl->arduinoFile.path();
    if (!oldPath) {
      return false;
    }
    impl->arduinoFile.close();
#ifdef SOC_SDMMC_HOST_SUPPORTED
    return SD_MMC.rename(oldPath, newPath);
#else
    return false;
#endif
  }
  return impl->fsFile.rename(newPath);
#endif
}

bool HalFile::isDirectory() const {
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return impl->arduinoFile.isDirectory();
#else
  return impl->isArduinoFs() ? impl->arduinoFile.isDirectory() : impl->fsFile.isDirectory();
#endif
}

void HalFile::rewindDirectory() {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  impl->arduinoFile.rewindDirectory();
#else
  if (impl->isArduinoFs()) {
    impl->arduinoFile.rewindDirectory();
  } else {
    impl->fsFile.rewindDirectory();
  }
#endif
}

bool HalFile::close() {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  impl->arduinoFile.close();
  return true;
#else
  if (impl->isArduinoFs()) {
    impl->arduinoFile.close();
    return true;
  }
  return impl->fsFile.close();
#endif
}

HalFile HalFile::openNextFile() {
  HalStorage::StorageLock lock;
  assert(impl != nullptr);
#if MOFEI_DEVICE
  return HalFile(std::make_unique<Impl>(impl->arduinoFile.openNextFile(FILE_READ)));
#else
  if (impl->isArduinoFs()) {
    return HalFile(std::make_unique<Impl>(impl->arduinoFile.openNextFile(FILE_READ)));
  }
  return HalFile(std::make_unique<Impl>(impl->fsFile.openNextFile()));
#endif
}
bool HalFile::isOpen() const {
  if (impl == nullptr) {
    return false;
  }
#if MOFEI_DEVICE
  return static_cast<bool>(impl->arduinoFile);
#else
  return impl->isArduinoFs() ? static_cast<bool>(impl->arduinoFile) : impl->fsFile.isOpen();
#endif
}
HalFile::operator bool() const { return isOpen(); }
