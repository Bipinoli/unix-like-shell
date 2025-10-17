#include <cstdlib>
#include <iostream>
#include <string>
#include <optional>

#include "parser.hpp"
#include "job.hpp"


using namespace std;

class Shell {
public:
  void run() {
    string prompt; 
    while (1) {
      cout << "[" << job_mgnr.cwd << "]$ ";
      getline(cin, prompt);
      auto parse_result = parser.parse(prompt);
      if (parse_result.error_msg.has_value()) {
        cout << parse_result.error_msg.value() << endl;
        continue;
      }
      if (!parse_result.job.has_value()) {
        continue;
      }
      // job_mgnr.display(parse_result.job.value());
      job_mgnr.run(parse_result.job.value());
    }
  }

private:
  Parser parser;
  JobManager job_mgnr;
};


int main() {
  Shell shell;
  shell.run();
  return 0;
}
