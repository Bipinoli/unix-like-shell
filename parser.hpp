#pragma once 

#include <cctype>
#include <string>
#include <sstream>
#include <cctype>
#include <vector>
#include <optional>
#include <algorithm>
#include <memory>

namespace parser {
  using namespace std;

  struct Command {
     vector<string> cmd;
     bool out_redirect_to_file;
     unique_ptr<parser::Command> out_redirect;
   };

  struct Job {
    parser::Command cmd;
    bool in_bg;
  };

  struct ParseResult {
    optional<parser::Job> job;
    bool has_error;
    string error_msg;
  };


  class Parser {
    public:
      ParseResult parse(string& s) {
        vector<string> tokens = tokenizer(s);
        if (tokens.empty()) {
          return {nullopt, false, ""};
        }
        bool job_in_bg = false;
        if (tokens.back() == "&") {
          tokens.pop_back();
          job_in_bg = true;
        } 
        auto [is_valid, err_msg] = verify_tokens(tokens);
        if (!is_valid) {
          return {nullopt, true, err_msg};
        }
        Job job = {
          make_unique<Command>(),
          job_in_bg
        };
        job.cmd.out_redirect_to_file = false;
        consume_tokens(job.cmd.get(), tokens, 0);
        return {job, false, ""};
      }

    private:
      void consume_tokens(Command& cmd, vector<string>& tokens, int tok_indx) {
        while (tok_indx < tokens.size()) {
          const string token = tokens[tok_indx];
          if (token[0] == '|') {
            cmd.out_redirect = make_unique<Command>();
            cmd.out_redirect_to_file = false;
            return consume_tokens(cmd.out_redirect.get(), tokens, tok_indx + 1);
          }
          if (token[0] == '>') {
            cmd.out_redirect = make_unique<Command>();
            cmd.out_redirect_to_file = true;
            return consume_tokens(cmd.out_redirect.get(), tokens, tok_indx + 1);
          }
          cmd.cmd.push_back(token);
          tok_indx += 1;
        }
      }

      pair<bool, string> verify_tokens(vector<string>& tokens) {
        // Invalid in following cases:
        // - starting with non alphabetic chars for commands
        // - subsequenct modifier tokens like '|', '>'
        bool modifier_expected_next = false;
        for (int i = 0; i<tokens.size(); i++) {
          auto tok = tokens[i];
          unsigned char first_c = static_cast<unsigned char>(tok[0]);
          if (!modifier_expected_next && !isalpha(first_c)) {
            return {false, "illegal: " + tok};
          }
          if (modifier_expected_next && (tok.length() > 1  || (first_c != '|' && first_c != '>'))) {
            return {false, "illegal: " + tok};
          }
          if (modifier_expected_next) {
            modifier_expected_next = false;
            continue;
          }
          if (i < tokens.size() - 1) {
            auto next_token = tokens[i+1];
            auto next_f_c = static_cast<unsigned char>(next_token[0]);
            if (!isalpha(next_f_c)) {
              modifier_expected_next = true;
            }
          }
        }
        return {true, ""};
      }

      vector<string> tokenizer(string& s) {
        lowercase(s);
        vector<string> ans;
        istringstream stream(s);
        string buf;
        while (stream >> buf) {
          ans.push_back(buf);
        }
        return ans;
      }

      void lowercase(string &s) {
        transform(s.begin(), s.end(), s.begin(), [&](auto c) { return tolower(c); });
      }
  };
}
