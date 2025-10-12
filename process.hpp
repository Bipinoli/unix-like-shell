#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <vector>

namespace process {
  using namespace std;

  namespace __internal {
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
  }

  void init() {
    // ignore CTRL + C, CTRL + Z
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    // ingore bacgkround process (spawned child) trying to acccess stdin, stdout to write to terminal
    // Also important to be able to set itself back to foreground in the terminal session
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
  }

  void spawn(const string& path, vector<string>& command) {
    auto child_pid = fork();
    if (child_pid == -1) {
      cerr << "CRASH! fork() failed" << endl;
      exit(1);
    }
    if (child_pid == 0) {
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
    } else {
      // In MacOS the SIGTSTP (CTRL + Z) signal is sent to the entire process group.
      // To avoid other processes including the parent receiving the signal
      // we should isolate the child process by making it its own process group leader
      setpgid(child_pid, child_pid); // see `man 2 setpgid`
      // And the child process group should be brought to the foreground
      // such that the signals are received by only child 
      __internal::set_process_grp_to_fg(child_pid);

      int status;
      if (waitpid(child_pid, &status, WUNTRACED) == -1) {
        cerr << "CRASH! failed to wait for the completion of child process" << endl;
        exit(1);
      }
      if (WIFSTOPPED(status)) {
        if (WSTOPSIG(status) == SIGTSTP) {
          __internal::set_process_grp_to_fg(getpgrp());
          __internal::bg_job = child_pid;
          cout << "Process suspended to the background. Resume with `fg`" << endl;
        }
      }
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
          __internal::set_process_grp_to_fg(getpgrp());
          __internal::bg_job = 0;
      }
    }
  }

  void bring2fg() {
    auto child_pid = __internal::bg_job;
    if (child_pid) {
      __internal::bg_job = 0;
      __internal::set_process_grp_to_fg(child_pid);
      kill(child_pid, SIGCONT);
      int status;
      if (waitpid(child_pid, &status, WUNTRACED) == -1) {
        cerr << "CRASH! failed to wait for the completion of child process" << endl;
        exit(1);
      }
      if (WIFSTOPPED(status)) {
        if (WSTOPSIG(status) == SIGTSTP) {
          __internal::set_process_grp_to_fg(getpgrp());
          __internal::bg_job = child_pid;
          cout << "Process suspended to the background. Resume with `fg`" << endl;
        }
      }
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
          __internal::set_process_grp_to_fg(getpgrp());
          __internal::bg_job = 0;
      }
    }
  }
}
