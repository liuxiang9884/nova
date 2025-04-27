#include <iostream>

#include "nova/common/hardware.h"

int main() {
  std::cout << "Hello World!\n";
  std::cout << "123456789012345678901234567890123456789012345678901234567890123"
               "45678901234567890\n";
  std::cout << nova::kCacheLineSize << std::endl;
  return 0;


}
