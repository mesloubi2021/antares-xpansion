//

#ifndef ANTARESXPANSION_CANDIDATESINIREADER_H
#define ANTARESXPANSION_CANDIDATESINIREADER_H

#include <filesystem>
#include <string>
#include <vector>

#include "Candidate.h"
#include "INIReader.h"

struct IntercoFileData {
  int index_interco;
  int index_pays_origine;
  int index_pays_extremite;
};

class CandidatesINIReader {
 public:
  CandidatesINIReader(const std::filesystem::path& antaresIntercoFile,
                      const std::filesystem::path& areaFile);

  static std::vector<IntercoFileData> ReadAntaresIntercoFile(
      const std::filesystem::path& antaresIntercoFile);
  static std::vector<IntercoFileData> ReadAntaresIntercoFile(
      std::istringstream& antaresIntercoFileInStringStream);
  static std::vector<std::string> ReadAreaFile(
      const std::filesystem::path& areaFile);
  static std::vector<std::string> ReadAreaFile(
      std::istringstream& areaFileInStringStream);
  static std::vector<IntercoFileData> ReadLineByLineInterco(
      std::istream& stream);
  static std::vector<std::string> ReadLineByLineArea(std::istream& stream);
  std::vector<CandidateData> readCandidateData(
      const std::filesystem::path& candidateFile);

 private:
  bool checkArea(std::string const& areaName_p) const;
  CandidateData readCandidateSection(const std::filesystem::path& candidateFile,
                                     const INIReader& reader,
                                     const std::string& sectionName);

  std::map<std::string, int> _intercoIndexMap;
  std::vector<IntercoFileData> _intercoFileData;
  std::vector<std::string> _areaNames;
};

#endif  // ANTARESXPANSION_CANDIDATESINIREADER_H
