#include <iostream>
#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>

#include "myfilesystem.hpp"
#include "process.hpp"
#include "parser.hpp"

using namespace std;

class JobManager {
  public:
    string cwd;

    JobManager() {
      myfilesystem::cd_to_home();
      cwd = myfilesystem::get_cwd();
      init_native_cmds();
    }

    void run(Job& job) {
      if (is_native_job(job)) {
        run_native_cmd(job.cmds.front());
        return;
      }
      auto [is_valid_job, error_msg] = verify(job);
      if (!is_valid_job) {
        cout << error_msg << endl;
        return;
      }
      if (job.cmds.size() == 1) {
        run_normal_cmd(job.cmds.front(), job.in_bg);
        return;
      }
      if (!job.in_bg) {
        run_piped_cmds(job.cmds);
      }
    }


    void display(Job& job) {
      if (job.in_bg) {
        cout << "To be run in the background" << endl;
      }
      for (auto cmd: job.cmds) {
        cout << "______________________________________" << endl;
        for (auto c: cmd.cmd) {
          cout << c << " ";
        }
        cout << endl;
        cout << "Redirect output to file: " << cmd.out_redirect_to_file << endl;
        cout << "Redirect input from process: " << cmd.in_redirect_from_process << endl;
        cout << "Redirect output to process: " << cmd.out_redirect_to_process << endl;
      }
      cout << "______________________________________" << endl;
    }

  private:
    ProcessManager process_mgnr;
    unordered_map<string, function<void (const Command&)>> native_cmd_registry;

    void init_native_cmds() {
      native_cmd_registry["exit"] = [&](const Command&) {
        exit(0);
      };
      native_cmd_registry["pwd"] = [&](const Command&) {
        cout << myfilesystem::get_cwd() << endl;
      };
      native_cmd_registry["cd"] = [&](const Command& command) {
        vector<string> cmd = command.cmd;
        if (cmd.size() != 2) {
          cerr << "Command error: Missing path to change directory" << endl;
          return;
        }
        string path = cmd[1];
        myfilesystem::cd(path);
        cwd = myfilesystem::get_cwd();
      };
      native_cmd_registry["test"] = [&](const Command&) {
        cout << "launching sleep" << endl;
        vector<string> command { "sleep", "30"};
        process_mgnr.spawn("/bin/sleep", command);
      };
      native_cmd_registry["testbg"] = [&](const Command&) {
        cout << "launching sleep in bg. Enter fg to bring to foreground." << endl;
        vector<string> command { "sleep", "20"};
        process_mgnr.spawn_in_bg("/bin/sleep", command);
      };
      native_cmd_registry["fg"] = [&](const Command&) {
        process_mgnr.bring2fg();
      };
    }

    void create_pipe(int fds[2]) {
      if (pipe(fds) == -1) {
        cerr << "CRASH! pipe() failed" << endl;
        exit(1);
      }
    }

    void close_file(int fd) {
      if (close(fd) == -1) {
        cerr << "CRASH! close() failed" << endl;
        exit(1);
      }
    }

    bool is_native_job(Job& job) {
      return job.cmds.size() == 1 
        && job.cmds.front().cmd.size() == 1 
        && native_cmd_registry.find(job.cmds.front().cmd.front()) != native_cmd_registry.end();
    }

    void run_native_cmd(const Command& command) {
      native_cmd_registry[command.cmd.front()](command);
    }

    void run_normal_cmd(const Command& command, bool in_bg) {
      vector<string> cmd = command.cmd;
      optional<string> exec_path = myfilesystem::locate_executable_file_in_path(cmd.front());
      if (in_bg) {
        process_mgnr.spawn_in_bg(exec_path.value(), cmd);
        return;
      }
      process_mgnr.spawn(exec_path.value(), cmd);
      return;
    }

    void run_piped_cmds(const vector<Command>& cmds) {
      int read_pipe[2];
      int write_pipe[2];
      create_pipe(read_pipe);
      for (auto command: cmds) {
        create_pipe(write_pipe);
        optional<string> exec_path = myfilesystem::locate_executable_file_in_path(command.cmd.front());
        process_mgnr.spawn_with_pipe(
            exec_path.value(), command.cmd, 
            command.in_redirect_from_process,
            command.out_redirect_to_process,
            read_pipe,
            write_pipe
        );
        // read_pipe is completely closed at this point
        // current write_pipe will be read_pipe next
        read_pipe[0] = write_pipe[0];
        read_pipe[1] = write_pipe[1];
      }
      close_file(read_pipe[0]);
      close_file(read_pipe[1]);
    }

    pair<bool, string> verify(Job& job) {
      for (auto command: job.cmds) {
        if (command.out_redirect_to_file) {
          break;
        }
        if (!myfilesystem::locate_executable_file_in_path(command.cmd.front())) {
          return {false, "unknown command: " + command.cmd.front()};
        }
      }
      return {true, ""};
    }

};


