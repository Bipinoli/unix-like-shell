#pragma once

#include <filesystem>
#include <string>
#include <cstdlib>

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
  
}
