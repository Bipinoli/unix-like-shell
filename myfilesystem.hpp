#pragma once

#include <sys/stat.h>
#include <string>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <filesystem>
#include <optional>

namespace myfilesystem {
  using namespace std;

  string get_home() {
    const char *home = getenv("HOME");
    if (!home) {
      cerr << "CRASH! couldn't determine HOME directory" << endl;
      exit(1);
    }
    return home;
  }

  void cd_to_home() {
    filesystem::current_path(get_home()); 
  }

  string get_cwd() {
    return filesystem::current_path();
  }

  void cd(const string& path) {
    filesystem::current_path(path);
  }

  vector<string> get_path_dirs() {
    const char* path_env = getenv("PATH");
    vector<string> paths {"."};
    string path(path_env);
    stringstream ss(path);
    string buffer;
    while (getline(ss, buffer, ':')) {
      if (!buffer.empty()) {
        paths.push_back(buffer);
      }
    }
    return paths;
  }

  optional<string> locate_executable_file_in_path(const string& name) {
    vector<string> paths = get_path_dirs();
    for (string path: paths) {
      struct stat buffer;
      const string file_path = path + "/" + name;
      if (stat(file_path.c_str(), &buffer) == 0) {
        if (buffer.st_mode & S_IXUSR) { // man 2 stat
          // executable file
          return file_path;
        }
      }
    }
    return nullopt;
  }
  
}
