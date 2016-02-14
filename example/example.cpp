// This example showcases how to use the library for an example flow.
//
// The graph represents the example flow:
//      +---+Query A+-+-----+Query B+---+
//      |             |         +       |
//      |             |         |       |
//      +-----------------------+       |
//      |             |                 |
//      |             |                 |
//      V             V                 V
//   Perform X    Perform Y         Perform Z
//      +             +                 +
//      |             V                 |
//      +------->Log Results<-----------+
//  The task is to first query two services - A and B - and perform actions, X,
//  Y, Z based on results. At the end we log the results of performed actions.

#include <cstdint>
#include <future>
#include <iostream>
#include <memory>

#include "flow_graph.hpp"

using std::string;

using namespace flow_graph;

string query(const string& serviceName);
bool performX(string queryA, string queryB);
bool performY(string queryA, string queryB);
bool performZ(string queryB);
bool logResults(bool a, bool b, bool c);

static std::mutex printMutex;

int main(int argc, char* argv[]) {
  SharedExecutor executor = getThreadExecutor();
  std::pair<std::function<void()>, SharedFuture<string>> queryA =
      lift0(static_cast<std::function<string()>>(std::bind(query, "A")),
          executor);
  std::pair<std::function<void()>, SharedFuture<string>> queryB =
      lift0(static_cast<std::function<string()>>(std::bind(query, "B")),
          executor);

  SharedFuture<bool> x = lift2(
      static_cast<std::function<bool(string, string)>>(performX),
      queryA.second, queryB.second);
  SharedFuture<bool> y = lift2(
      static_cast<std::function<bool(string, string)>>(performY),
      queryA.second, queryB.second);
  SharedFuture<bool> z = lift1(
      static_cast<std::function<bool(string)>>(performZ), queryB.second);

  SharedFuture<bool> logResult = lift3(
      static_cast<std::function<bool(bool, bool, bool)>>(logResults),
      x, y, z);

  queryA.first();
  queryB.first();

  logResult->wait();
  logResult->get();
  return 0;
}

string query(const string& serviceName) {
  std::lock_guard<mutex> lock(printMutex);
  std::cout << "Performing query " << serviceName << "." << std::endl;
  return serviceName;
}

bool performX(string queryA, string queryB) {
  std::lock_guard<mutex> lock(printMutex);
  std::cout << "Performing X with resources: " << queryA << ", " << queryB
            << "." << std::endl;
  return true;
}

bool performY(string queryA, string queryB) {
  std::lock_guard<mutex> lock(printMutex);
  std::cout << "Performing Y with resources: " << queryA << ", " << queryB
            << "." << std::endl;
  return true;
}

bool performZ(string queryB) {
  std::lock_guard<mutex> lock(printMutex);
  std::cout << "Performing Z with resource: " << queryB << "." << std::endl;
  return true;
}


bool logResults(bool x, bool y, bool z) {
  std::lock_guard<mutex> lock(printMutex);
  std::cout << "The tasks have finished with results: x = " << x
            << ", y = " << y
            << ", z = " << z
            << "." << std::endl;
  return true;
}
