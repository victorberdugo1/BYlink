#pragma once

#include <string>
#include <vector>

namespace bylink {

struct ProcessResult {
  bool started = false;
  int exitCode = -1;
  std::string output;
};

ProcessResult runProcess(const std::vector<std::string>& argv);

}
