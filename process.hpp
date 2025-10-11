#include <unistd.h>
#include <sys/wait.h>
#include <csignal>
#include <vector>

namespace process {
  using namespace std;

  namespace __internal {
    pid_t bg_job;

    void sigint_handler(int) {
      cout << "Handling CTRL + Z" << endl;
    }
  }

  void init() {
    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
  }

  void spawn(const string& path, vector<string>& command) {
    // create pipe for IPC to order parent event before child's
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) { // man 2 pipe
      cerr << "CRASH! pipe() failed. Not possible to configure spawned process correctly!" << endl;
      exit(1);
    }
    
    auto pid = fork();
    if (pid == -1) {
      cerr << "CRASH! fork() failed" << endl;
      exit(1);
    }
    if (pid == 0) {
      // we want child to be 
      // SIG_IGN handlers are inherited in children
      // So set to default
      signal(SIGINT, SIG_DFL);
      signal(SIGTSTP, SIG_DFL);

      // set child to be in the foreground group
      // important because in macOS the ctrl + z 
      // is sent to the whole process group
      // but before that we must wait until the
      // parent has set child to be in its own process group
      //
      // Note: The file descriptors are copid during fork()
      // so we must close the unused descripter here so that 
      // there are no duplicate producer/consumer on the pipe
      // thus preventing the pipe close when closed from the 
      // other process
      close(pipe_fd[1]);
      char dummy;
      if (read(pipe_fd[0], &dummy, 1) == -1) { // man 2 read
        cerr << "CRASH! failed to read from the IPC pipe" << endl;
        exit(1);
      }
      close(pipe_fd[0]);
      // setting foregroud proccess group id 
      if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1) {
        cerr << "CRASH! tcsetpgrp() failed to assign child into foreground" << endl;
        exit(1);
      } 

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
      // in MacOS the SIGTSTP signal is sent to the entire process group
      // to avoid other processes including the parent receiving the signal
      // we should isolate the child process by making it its own process group leader
      setpgid(pid, pid); // see `man 2 setpgid`

      // Note: The file descriptors are copid during fork()
      // so we must close the unused descripter here so that 
      // there are no duplicate producer/consumer on the pipe
      // thus preventing the pipe close when closed from the 
      // other process
      close(pipe_fd[0]); // man 2 close
      char dummy = '-';
      if (write(pipe_fd[1], &dummy, 1) == -1) { // man 2 write
        cerr << "CRASH! failed to write to the IPC pipe" << endl;
        exit(1);
      }
      close(pipe_fd[1]);


      int status;
      if (waitpid(pid, &status, WUNTRACED) == -1) {
        cerr << "CRASH! failed to wait for the completion of child process" << endl;
        exit(1);
      }
      if (WIFSTOPPED(status)) {
        if (WSTOPSIG(status) == SIGTSTP) {
          cout << "Parent: child stopped with ctrl + z" << endl;
          __internal::bg_job = pid;
        }
      }
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
          cout << "Parent: child finished or killed" << endl;
        __internal::bg_job = 0;
      }
    }
  }

  void bring2fg() {
    auto child_pid = __internal::bg_job;
    if (child_pid) {
      kill(child_pid, SIGCONT);
      __internal::bg_job = 0;
      int status;
      if (waitpid(child_pid, &status, WUNTRACED) == -1) {
        cerr << "CRASH! failed to wait for the completion of child process" << endl;
        exit(1);
      }
      if (WIFSTOPPED(status)) {
        if (WSTOPSIG(status) == SIGTSTP) {
          cout << "Parent: child stopped with ctrl + z" << endl;
          __internal::bg_job = child_pid;
        }
      }
      if (WIFEXITED(status) || WIFSIGNALED(status)) {
          cout << "Parent: child finished or killed" << endl;
        __internal::bg_job = 0;
      }
    }
  }
}
