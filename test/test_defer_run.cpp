//
// Created by henry on 24-1-23.
//
#include <iostream>
#include <string>
#include "include/defer.h"

using namespace std;

void testFun(const string& name) { cout << name; }

int main() {
  cout << "begin..." << endl;
  string str1 = "Hello world!";
  DEFER {
    testFun(str1);
  };
  cout << "end..." << endl;
  return 0;
}