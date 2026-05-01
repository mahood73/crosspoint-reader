#include "WriterDraftStore.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstring>

#include "WriterCursor.h"
#include "WriterFile.h"

// Check draft folder exists
bool WriterDraftStore::ensureDraft() {
  if (!Storage.exists(DraftDir)) {
    // Create the folder
    if (!Storage.mkdir(DraftDir)) {
      LOG_ERR("Writer", "Failed to create folder: %s", DraftDir);
      return false;
    }
    LOG_DBG("Writer", "Folder created successfully: %s", DraftDir);
  }

  if (!Storage.exists(DraftPath)) {
    // Create the file
    HalFile file;
    if (!Storage.openFileForWrite("Writer", DraftPath, file)) {
      LOG_ERR("Writer", "Failed to create file: %s", DraftPath);
      return false;
    }
    LOG_DBG("Writer", "Draft file created successfully: %s", DraftPath);
    file.close();
  }

  return true;
}

bool WriterDraftStore::appendToDraft(const std::string& text) {
  if (!ensureDraft()) {
    return false;
  }

  HalFile file;
  if (!WriterFile::openForAppend(DraftPath, file)) {
    LOG_ERR("Writer", "Failed to open draft file for append: %s", DraftPath);
    return false;
  }

  const size_t bytesWritten = file.write(text.data(), text.size());
  file.close();

  if (bytesWritten != text.size()) {
    LOG_ERR("Writer", "Failed to append full text: %s (%zu/%zu bytes)", DraftPath, bytesWritten, text.size());
    return false;
  }

  LOG_DBG("Writer", "Appended to draft file: %s (%zu bytes)", DraftPath, bytesWritten);
  return true;
}

bool WriterDraftStore::readDraft(std::string& out) {
  out.clear();

  if (!ensureDraft()) {
    return false;
  }

  HalFile file;

  if (!Storage.openFileForRead("Writer", DraftPath, file)) {
    LOG_ERR("Writer", "Failed to open draft file: %s", DraftPath);
    return false;
  }

  // For Stage 1 the editor follows the newest text, so large drafts are loaded
  // from the tail instead of building paging or scroll state yet.
  constexpr size_t MaxDraftDisplayBytes = 64 * 1024;
  const size_t fileSize = file.size();
  const size_t startOffset = fileSize > MaxDraftDisplayBytes ? fileSize - MaxDraftDisplayBytes : 0;

  if (startOffset > 0 && !file.seekSet(startOffset)) {
    LOG_ERR("Writer", "Failed to seek draft file: %s (%zu/%zu bytes)", DraftPath, startOffset, fileSize);
    file.close();
    return false;
  }

  if (startOffset > 0) {
    while (file.available()) {
      const int nextByte = file.read();
      if (nextByte < 0) {
        file.close();
        LOG_ERR("Writer", "Failed while aligning draft file: %s", DraftPath);
        return false;
      }

      if (!WriterCursor::isUtf8ContinuationByte(static_cast<unsigned char>(nextByte))) {
        if (!file.seekCur(-1)) {
          file.close();
          LOG_ERR("Writer", "Failed to rewind aligned draft file: %s", DraftPath);
          return false;
        }
        break;
      }
    }
  }

  constexpr size_t bufferSize = 256;
  uint8_t buffer[bufferSize];

  while (file.available()) {
    const int bytesRead = file.read(buffer, bufferSize);
    if (bytesRead <= 0) {
      file.close();
      LOG_ERR("Writer", "Failed while reading draft file: %s", DraftPath);
      return false;
    }

    out.append(reinterpret_cast<const char*>(buffer), static_cast<size_t>(bytesRead));
  }

  file.close();
  LOG_DBG("Writer", "Read draft file: %s (%zu/%zu bytes)", DraftPath, out.size(), fileSize);
  return true;
}

const char* WriterDraftStore::getDraftDisplayName() const {
  const char* slash = std::strrchr(DraftPath, '/');
  return slash == nullptr ? DraftPath : slash + 1;
}
