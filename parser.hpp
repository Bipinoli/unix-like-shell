#pragma once 

#include <string>
#include <sstream>
#include <cctype>
#include <algorithm>
#include <vector>


namespace parser {
  using namespace std;
   namespace __utils {
     void lowercase(string &s) {
       transform(s.begin(), s.end(), s.begin(), [&](auto c) { return tolower(c); });
     }
   }
   vector<string> parse(string& s) {
     __utils::lowercase(s);
     vector<string> ans;
     istringstream stream(s);
     string buf;
     while (stream >> buf) {
       ans.push_back(buf);
     }
     return ans;
   }
}
