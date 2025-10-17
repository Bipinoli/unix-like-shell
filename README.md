# A Unix-like shell

<img width="668" height="547" alt="Image" src="https://github.com/user-attachments/assets/36c8ce8d-b4b6-425c-800e-54dc58e26398" />

## Features
- [X] Shell prompt input and parsing, exiting shell
- [X] Built in `cd`, `pwd`
- [X] Spawning processes & controlling them
- [X] Handling `Ctrl + C` to kill a process
- [X] `Ctrl + Z` and `&` to run in the background
- [X] `fg` to bring to foreground
- [X] Locate and run executable files from `PATH`
- [X] Pipe `|`
- [X] IO redirection `>`

## Noteworthy encountered challenges
### Signal handling in MacOS & terminal STDIN access
I wanted child processes to respond to job control signals like `CTRL+C` and `CTRL+Z`, while the parent shell remained unaffected. To achieve this, I initially set the parent to ignore these signals and restored the default signal handlers in the child. However, on macOS, I discovered that these signals are delivered to the entire process group, not just the individual foreground process. As a result, `CTRL+Z` would still suspend the parent shell itself. Somehow, `CTRL + Z` is not fully handled in the parent. I guess the terminal process also receives this signal, being in the same process group as the parent shell. 

To resolve this, I isolated the child by placing it in its own process group using `setpgid`, ensuring that job control signals would only target the child. I then needed to use `tcsetpgrp` to bring the child’s process group to the foreground, so it could access the terminal STDIO & STDOUT. After the child process exited or was suspended, the parent shell must call `tcsetpgrp` again to get the terminal control back and resume handling of user prompts.


### Deadlock in write due to the pipe buffer being full without a reader
Initially, my design for the pipe and redirection feature involved spawning processes serially (`fork`, `execve`), setting up their STDIN and STDOUT to point to pipes as needed. Each process would run, write its output to a `pipe`, and the parent would wait for it to finish before launching the next one, feeding the previous pipe’s output as the next process’s input.

However, this design caused a deadlock due to how pipe buffers work. The kernel provides a fixed-size pipe buffer, and if a process writes more data than the buffer can hold, it blocks until another process reads from the pipe. But, in my setup, the next process wasn't running yet. It was waiting for the previous one to finish. As a result, the writer would block waiting for a reader, which hadn't started, leading to a deadlock.

To fix this, I changed the design to spawn all processes upfront, wiring their input and output pipes correctly. Only after all processes are running does the parent wait for them. This avoids the deadlock by ensuring that readers and writers are active concurrently.
