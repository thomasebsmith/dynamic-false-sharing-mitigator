#include "AccessInfo.h"

bool operator==(const conflicting_addr &left, const conflicting_addr &right) {
  return (left.addr1 == right.addr1 && left.addr2 == right.addr2) ||
         (left.addr1 == right.addr2 && left.addr2 == right.addr1);
}

bool operator<(const global_var &left, const global_var &right) {
  return left.start_addr < right.start_addr;
}
