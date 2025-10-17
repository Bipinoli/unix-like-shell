#include <iostream>
#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>
#include <fstream>

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
      run_piped_cmds(job.cmds);
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
        cout << "Input redirect: " << (cmd.in_redirect ? "true" : "false") << endl;
        cout << "Output redirect: " << (cmd.out_redirect ? "true" : "false") << endl;
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
      return job.cmds.size() == 1 && native_cmd_registry.find(job.cmds.front().cmd.front()) != native_cmd_registry.end();
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
      // The pipe buffer in kernel memory is of fixed size.
      // If the data is bigger than the buffer
      // then the data will be written in the buffer in multiple steps.
      // The writing process will wait until the pipe buffer is read by reading process.
      // Therefore it is important that the reading and writing process are running in parallel.
      // Otherwise, the writer might deadlock waiting for the buffer to be read.
      //
      // Also what happens if one of the intermediate process in the pipe crashes?
      // The process is killed and all the file descriptors related to the process are closed by the kernel.
      // Closing the write end of pipe file will send the EOF.
      // Therefore the input for the following process ends threby ending the process as well.
      // In this case the preceeding process might still be running.
      // When the preceeding process tries to write to the pipe,
      // the kernel will notice that the reading end has already closed
      // and will send the SIGPIPE signal to the preceeding process. (man 2 signal)
      // Assuming the SIGPIPE signal is not handled, the preceeding process will also be killed.
      // This way the whole pipeline finishes in cascading fashion.
      vector<array<int, 2>> pipes;
      unordered_map<int, int> read_pipe_map;
      unordered_map<int, int> write_pipe_map;
      for (int i=0; i<cmds.size(); i++) {
        auto cmd = cmds[i];
        if (cmd.out_redirect) {
          pipes.push_back(array<int, 2>{});
          create_pipe(pipes.back().data());
          write_pipe_map[i] = pipes.size()-1;
          read_pipe_map[i+1] = pipes.size()-1;
        }
      }
      vector<pid_t> children;
      for (int i=0; i<cmds.size(); i++) {
        auto command = cmds[i];
        optional<string> exec_path = myfilesystem::locate_executable_file_in_path(command.cmd.front());
        if (command.in_redirect && !exec_path.has_value() && i == cmds.size() - 1) {
          // must be the output redirect to a file
          auto read_pipe = pipes[read_pipe_map[i]];
          close_file(read_pipe[1]);
          write_pipe_data_to_file(read_pipe[0], command.cmd.front());
          close_file(read_pipe[0]);
          break;
        }
        auto child_id = process_mgnr.spawn_with_pipe(
          exec_path.value(),
          command.cmd,
          command.in_redirect ? make_optional<array<int,2>>(pipes[read_pipe_map[i]]) : nullopt,
          command.out_redirect ? make_optional<array<int,2>>(pipes[write_pipe_map[i]]) : nullopt
        );
        children.push_back(child_id);
      }
      bool pipeline_failed = false;
      for (auto pid: children) {
        int status;
        waitpid(pid, &status, 0);
        if (WEXITSTATUS(status) != 0) {
          pipeline_failed = true; 
        }
      }
      if (pipeline_failed) {
        cerr << "pipeline failed!" << endl;
      }
    }



    pair<bool, string> verify(Job& job) {
      for (auto command: job.cmds) {
        if (command.is_last_of_pipeline) {
          // could be the output to the file
          // so not testing for existing executable
          break;
        }
        if (!myfilesystem::locate_executable_file_in_path(command.cmd.front())) {
          return {false, "unknown command: " + command.cmd.front()};
        }
      }
      return {true, ""};
    }



    void write_pipe_data_to_file(int pipe_read_fd, string outfile) {
      assert(outfile.length() > 0);
      ofstream output_fs(outfile, ios::binary);
      const size_t BUF_SIZE = 1280; // 64 * 20
      char buf[BUF_SIZE];
      size_t bytes_read;
      while ((bytes_read = read(pipe_read_fd, buf, BUF_SIZE)) > 0) { // man 2 read
        output_fs.write(buf, bytes_read);
      }
      if (bytes_read < 0) {
        cerr << "Couldn't read data from the pipe." << endl;
      }
      output_fs.close();
    }

};


