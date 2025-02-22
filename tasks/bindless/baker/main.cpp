#include <iostream>
#include "baker.hpp"

int main(int argc, char* argv[])
{
  if (argc != 2) {
    std::cerr << "ERROR: need path arg";
    abort();
  }
  
  char* path = argv[1];

  bool bake_successful = bake(path);
  
  if (!bake_successful) {
    return -1;
  }
}
