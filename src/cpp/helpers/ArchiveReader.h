#ifndef _ARCHIVEREADER_H
#define _ARCHIVEREADER_H
#include <istream>
#include <sstream>

#include "ArchiveIO.h"
class ArchiveReader : public ArchiveIO {
 private:
  void* internalPointer_ = NULL;
  void* handle_ = NULL;
  void Create() override;

 public:
  explicit ArchiveReader(const std::filesystem::path& archivePath);
  ArchiveReader();

  int32_t Close() override;
  void Delete() override;
  //   void* InternalPointer() const override;
  //   void* handle() const override;

  int Open() override;
  int32_t ExtractFile(const std::filesystem::path& FileToExtractPath);
  int32_t ExtractFile(const std::filesystem::path& FileToExtractPath,
                      const std::filesystem::path& destination);
  int32_t LocateEntry(const std::filesystem::path& fileToExtractPath);
  int32_t OpenEntry(const std::filesystem::path& fileToExtractPath);
  std::istringstream ExtractFileInStringStream(
      const std::filesystem::path& FileToExtractPath);
};
#endif  // _ARCHIVEREADER_H
