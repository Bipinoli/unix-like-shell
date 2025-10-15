#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

#include "parser.hpp"
#include "myfilesystem.hpp"
#include "process.hpp"

using namespace std;

namespace job {
  void display(parser::Job& job) {
    if (job.in_bg) {
      cout << "To be run in the background" << endl;
    }
    // const parser::Command* cur = &job.cmd;
  }
}


class Shell {
public:
  Shell() {
    init();
  }

  void run() {
    parser::Parser prsr;
    string prompt; 
    while (1) {
      cout << "[" << cwd << "]$ ";
      getline(cin, prompt);
      auto parse_result = prsr.parse(prompt);
      if (parse_result.has_error) {
        cout << parse_result.error_msg << endl;
        continue;
      }
      if (parse_result.job.has_value()) {
        job::display(parse_result.job.value());
      }
      // if (registry.find(command.front()) != registry.end()) {
      //   registry[command.front()]();
      //   continue;
      // }
      // auto exec_file = myfilesystem::locate_executable_file_in_path(command.front());
      // if (exec_file.has_value()) {
      //   if (command.back() == "&") {
      //     command.pop_back();
      //     process::spawn_in_bg(exec_file.value(), command);
      //   } else {
      //     process::spawn(exec_file.value(), command);
      //   }
      // } else {
      //   cout << "unknown command: " << command.front() << endl;
      // }
    }
  }

private:
  unordered_map<string, function<void ()>> registry;
  string cwd;
  vector<string> command;

  void init() {
    process::init();
    myfilesystem::cd_to_home();
    cwd = myfilesystem::get_cwd();
    init_handlers();  
  }

  void init_handlers() {
    registry["exit"] = [&]() {
      exit(0);
    };
    registry["pwd"] = [&]() {
      cout << myfilesystem::get_cwd() << endl;
    };
    registry["cd"] = [&]() {
      auto a = command;
      if (command.size() != 2) {
        cerr << "Command error: Missing path to change directory" << endl;
        return;
      }
      string path = command[1];
      myfilesystem::cd(path);
      cwd = myfilesystem::get_cwd();
    };
    registry["test"] = [&]() {
      cout << "launching sleep" << endl;
      vector<string> command { "sleep", "30"};
      process::spawn("/bin/sleep", command);
    };
    registry["test2"] = [&]() {
      cout << "launching sleep in bg" << endl;
      vector<string> command { "sleep", "10"};
      process::spawn_in_bg("/bin/sleep", command);
    };
    registry["fg"] = [&]() {
      process::bring2fg();
    };
  }
  
};


int main() {
  Shell shell;
  shell.run();
  return 0;
}
