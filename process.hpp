#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <vector>

using namespace std;

class ProcessManager {
  public:
    ProcessManager() {
      // ignore CTRL + C, CTRL + Z
      signal(SIGINT, SIG_IGN);
      signal(SIGTSTP, SIG_IGN);
      // ingore bacgkround process (spawned child) trying to acccess stdin, stdout to write to terminal
      // Also important to be able to set itself back to foreground in the terminal session
      signal(SIGTTOU, SIG_IGN);
      signal(SIGTTIN, SIG_IGN);
    }

    bool spawn_with_pipe(const string& path, const vector<string>& command, bool pipe_read, bool pipe_write, int read_pipe[2], int write_pipe[2]) {
      auto child_pid = fork();
      if (child_pid == -1) {
        cerr << "CRASH! fork() failed" << endl;
        exit(1);
      }
      if (child_pid == 0) {
        close_file(read_pipe[1]);
        close_file(write_pipe[0]);
        if (pipe_read) {
          dup2(read_pipe[0], STDIN_FILENO); // man 2 dup2
        }
        if (pipe_write) {
          dup2(write_pipe[1], STDOUT_FILENO);
        }
        close_file(read_pipe[0]);
        close_file(write_pipe[1]);
        // Note: 
        // file descriptors are preserved during execve
        // duplicated file descriptors from dup2 are closed by OS when process terminates
        execve_child_process(path, command);
        return true; // never reached. Just to make warning disappear
      } else {
        close_file(read_pipe[0]);
        close_file(read_pipe[1]);
        setpgid(child_pid, child_pid);
        set_process_grp_to_fg(child_pid);
        int status;
        if (waitpid(child_pid, &status, 0) == -1) {
          cerr << "CRASH! failed to wait for the completion of child process" << endl;
          exit(1);
        }
        // immportant to bring parent to foreground to attach STDIN to terminal
        set_process_grp_to_fg(getpgrp());
        if (WIFEXITED(status)) {
          int exit_status = WEXITSTATUS(status);
          if (exit_status == 0) {
            return true;
          }
        }
        return false;
      }
    }

    void spawn(const string& path, const vector<string>& command) {
      auto child_pid = fork();
      if (child_pid == -1) {
        cerr << "CRASH! fork() failed" << endl;
        exit(1);
      }
      if (child_pid == 0) {
        execve_child_process(path, command);
      } else {
        // In MacOS the SIGTSTP (CTRL + Z) signal is sent to the entire process group.
        // To avoid other processes including the parent receiving the signal
        // we should isolate the child process by making it its own process group leader
        setpgid(child_pid, child_pid); // see `man 2 setpgid`
        // And the child process group should be brought to the foreground
        // such that the signals are received by only child 
        set_process_grp_to_fg(child_pid);

        int status;
        if (waitpid(child_pid, &status, WUNTRACED) == -1) {
          cerr << "CRASH! failed to wait for the completion of child process" << endl;
          exit(1);
        }
        if (WIFSTOPPED(status)) {
          if (WSTOPSIG(status) == SIGTSTP) {
            set_process_grp_to_fg(getpgrp());
            bg_job = child_pid;
            cout << "Process suspended to the background. Resume with `fg`" << endl;
          }
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
          set_process_grp_to_fg(getpgrp());
          bg_job = 0;
        }
      }
    }

    void spawn_in_bg(const string& path, vector<string>& command) {
      auto child_pid = fork();
      if (child_pid == -1) {
        cerr << "CRASH! fork() failed" << endl;
        exit(1);
      }
      if (child_pid == 0) {
        execve_child_process(path, command);
      } else {
        setpgid(child_pid, child_pid);
        bg_job = child_pid;
        cout << "Process launched in the background. Bring to foreground with `fg`" << endl;
      }
    }

    void bring2fg() {
      auto child_pid = bg_job;
      if (child_pid) {
        bg_job = 0;
        set_process_grp_to_fg(child_pid);
        kill(child_pid, SIGCONT);
        int status;
        if (waitpid(child_pid, &status, WUNTRACED) == -1) {
          cerr << "CRASH! failed to wait for the completion of child process" << endl;
          exit(1);
        }
        if (WIFSTOPPED(status)) {
          if (WSTOPSIG(status) == SIGTSTP) {
            set_process_grp_to_fg(getpgrp());
            bg_job = child_pid;
            cout << "Process suspended to the background. Resume with `fg`" << endl;
          }
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            set_process_grp_to_fg(getpgrp());
            bg_job = 0;
        }
      }
    }


  private:
    pid_t bg_job;

    void set_process_grp_to_fg(pid_t pid) {
      // Note:
      // `tcsetpgrp` can be called by any process in the terminal session
      // However, within the terminal session there are foreground and bacckground proccess.
      // The `tcsetpgrp` can be called by the foreground proccess to set 
      // other process group into foreground taking itself into background.
      // However, if the process is in background within the same terminal session,
      // it can still call `tcsetpgrp` and bring itself into foreground 
      // if the `SIGTTOU` signal is ingored within.
      //
      // Therefore this function must only be called from the parent process.
      // It is the job of the parent (shell process) to manage this.
      // The child process will be transformed to other process via `execve`,
      // so there is no easy way to mange this from child process.
      //
      // Also note that it is not necessary to set all STDIN, STDOUT, STDERR 
      // file descriptor to the process group to make foregound.
      // Doing any one suchh as STDIN is enough.
      if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
        cerr << "CRASH! tcsetpgrp() failed to assign process group to foreground. Error: " << errno << endl;
        exit(1);
      }
    }

    void execve_child_process(const string& path, const vector<string>& command) {
      // SIG_IGN handlers are inherited in children
      // So set to default
      signal(SIGINT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);

      vector<char*> argv;
      for (auto c: command) {
        argv.push_back(const_cast<char*>(c.c_str()));
      }
      argv.push_back(nullptr);
      if (execve(path.c_str(), argv.data(), nullptr) == -1) {
        cerr << "CRASH! failed to spawn the process" << endl;
        exit(1);
      }
    }

    void close_file(int fd) {
      if (close(fd) == -1) {
        cerr << "CRASH! close() failed" << endl;
        exit(1);
      }
    }
};

