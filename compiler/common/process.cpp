#include "common/process.h"

#include <array>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace bylink {

#ifdef _WIN32

namespace {

std::string quoteArgument(const std::string& arg) {
  if (!arg.empty() && arg.find_first_of(" \t\n\v\"") == std::string::npos) return arg;
  std::string out = "\"";
  size_t backslashes = 0;
  for (char c : arg) {
    if (c == '\\') {
      ++backslashes;
      continue;
    }
    if (c == '"') {
      out.append(backslashes * 2 + 1, '\\');
      out += '"';
    } else {
      out.append(backslashes, '\\');
      out += c;
    }
    backslashes = 0;
  }
  out.append(backslashes * 2, '\\');
  return out + "\"";
}

}

ProcessResult runProcess(const std::vector<std::string>& argv) {
  ProcessResult result;
  if (argv.empty()) return result;
  SECURITY_ATTRIBUTES attributes{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  HANDLE readEnd = nullptr;
  HANDLE writeEnd = nullptr;
  if (!CreatePipe(&readEnd, &writeEnd, &attributes, 0)) return result;
  SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
  std::string command;
  for (size_t i = 0; i < argv.size(); ++i) {
    if (i != 0) command += ' ';
    command += quoteArgument(argv[i]);
  }
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdOutput = writeEnd;
  startup.hStdError = writeEnd;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION info{};
  BOOL created = CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup, &info);
  CloseHandle(writeEnd);
  if (!created) {
    CloseHandle(readEnd);
    return result;
  }
  result.started = true;
  std::array<char, 4096> buffer{};
  DWORD got = 0;
  while (ReadFile(readEnd, buffer.data(), static_cast<DWORD>(buffer.size()), &got, nullptr) && got != 0) result.output.append(buffer.data(), got);
  CloseHandle(readEnd);
  WaitForSingleObject(info.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(info.hProcess, &code);
  result.exitCode = static_cast<int>(code);
  CloseHandle(info.hProcess);
  CloseHandle(info.hThread);
  return result;
}

#else

namespace {

bool setCloseOnExec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  return flags >= 0 && fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == 0;
}

}

ProcessResult runProcess(const std::vector<std::string>& argv) {
  ProcessResult result;
  if (argv.empty()) return result;
  int outPipe[2];
  int errPipe[2];
  if (pipe(outPipe) != 0) return result;
  if (pipe(errPipe) != 0 || !setCloseOnExec(errPipe[1])) {
    close(outPipe[0]);
    close(outPipe[1]);
    return result;
  }
  std::vector<char*> args;
  std::vector<std::string> storage = argv;
  for (std::string& s : storage) args.push_back(s.data());
  args.push_back(nullptr);
  pid_t pid = fork();
  if (pid < 0) {
    for (int fd : {outPipe[0], outPipe[1], errPipe[0], errPipe[1]}) close(fd);
    return result;
  }
  if (pid == 0) {
    close(outPipe[0]);
    close(errPipe[0]);
    dup2(outPipe[1], STDOUT_FILENO);
    dup2(outPipe[1], STDERR_FILENO);
    close(outPipe[1]);
    execvp(args[0], args.data());
    int failure = errno;
    ssize_t ignored = write(errPipe[1], &failure, sizeof(failure));
    (void)ignored;
    _exit(127);
  }
  close(outPipe[1]);
  close(errPipe[1]);
  int execErrno = 0;
  ssize_t got;
  do {
    got = read(errPipe[0], &execErrno, sizeof(execErrno));
  } while (got < 0 && errno == EINTR);
  close(errPipe[0]);
  std::array<char, 4096> buffer{};
  while ((got = read(outPipe[0], buffer.data(), buffer.size())) != 0) {
    if (got < 0) {
      if (errno == EINTR) continue;
      break;
    }
    result.output.append(buffer.data(), static_cast<size_t>(got));
  }
  close(outPipe[0]);
  int status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
  if (execErrno != 0) return result;
  result.started = true;
  result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
  return result;
}

#endif

}
