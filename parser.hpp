#pragma once 

#include <cctype>
#include <string>
#include <sstream>
#include <cctype>
#include <vector>
#include <algorithm>
#include <optional>
#include <cassert>
#include <unordered_set>

using namespace std;

struct Command {
    vector<string> cmd;
    bool in_redirect;
    bool out_redirect;
    bool is_last_of_pipeline;
  };

struct Job {
  vector<Command> cmds;
  bool in_bg;
};

struct ParseResult {
  optional<Job> job;
  optional<string> error_msg;
};


class Parser {
  public:
    ParseResult parse(string& s) {
      vector<string> tokens = tokenizer(s);
      if (tokens.empty()) {
        return {nullopt, nullopt};
      }
      bool job_in_bg = false;
      if (tokens.back() == "&") {
        tokens.pop_back();
        job_in_bg = true;
      } 
      clean_tokens(tokens);
      auto [is_valid, err_msg] = verify_tokens(tokens);
      if (!is_valid) {
        return {nullopt, err_msg};
      }
      Job job = {
        vector<Command> {
          Command {
            .cmd = vector<string> {},
            .in_redirect = false,
            .out_redirect = false,
            .is_last_of_pipeline = false
          }
        },
        job_in_bg
      };
      consume_tokens(job.cmds, tokens, 0);
      return {job, nullopt};
    }

  private:
    void consume_tokens(vector<Command>& cmds, vector<string>& tokens, int tok_indx) {
      assert(cmds.size() > 0);
      while (tok_indx < tokens.size()) {
        const string token = tokens[tok_indx];
        if (token[0] == '|') {
          cmds.back().out_redirect = true;
          cmds.push_back(Command {
            .cmd = vector<string>{},
            .in_redirect = true,
            .out_redirect = false,
            .is_last_of_pipeline = false
          });
          return consume_tokens(cmds, tokens, tok_indx + 1);
        }
        if (token[0] == '>') {
          cmds.back().out_redirect = true;
          cmds.push_back(Command {
            .cmd = vector<string>{},
            .in_redirect = true,
            .out_redirect = false,
            .is_last_of_pipeline = true
          });
          return consume_tokens(cmds, tokens, tok_indx + 1);
        }
        cmds.back().cmd.push_back(token);
        tok_indx += 1;
      }
    }

    void clean_tokens(vector<string>& tokens) {
      for (auto& token: tokens) {
        if (token.length() > 1 && (token.front() == '"' || token.front() == '\'')) {
          token = token.substr(1, token.length() - 2);
        }
      }
    }

    pair<bool, string> verify_tokens(vector<string>& tokens) {
      // Invalid in following cases:
      // - starting with non alphabetic chars for commands
      // - subsequenct occurance of modifier tokens like '|', '>'
      unordered_set<char> valid_starts = unordered_set<char> {'.', '/', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};
      bool modifier_expected_next = false;
      for (int i = 0; i<tokens.size(); i++) {
        auto tok = tokens[i];
        unsigned char first_c = static_cast<unsigned char>(tok[0]);
        if (!modifier_expected_next && !isalpha(first_c) && valid_starts.find(first_c) == valid_starts.end()) {
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
          if (!isalpha(next_f_c) && valid_starts.find(next_f_c) == valid_starts.end()) {
            modifier_expected_next = true;
          }
        }
      }
      return {true, ""};
    }

    vector<string> tokenizer(string& input) {
      vector<std::string> tokens;
      string current;
      bool in_quote = false;
      char quote_char = 0;
      for (auto i = 0; i < input.size(); ++i) {
          char c = input[i];
          if (in_quote) {
              if (c == quote_char) {
                  in_quote = false;
                  tokens.push_back(current);
                  current.clear();
              } else {
                  current += c;
              }
          } else {
              if (std::isspace(c)) {
                  if (!current.empty()) {
                      tokens.push_back(current);
                      current.clear();
                  }
              } else if (c == '\'' || c == '"') {
                  in_quote = true;
                  quote_char = c;
              } else {
                  current += c;
              }
          }
      }
      if (!current.empty()) {
          tokens.push_back(current);
      }
      return tokens;
    }
};
