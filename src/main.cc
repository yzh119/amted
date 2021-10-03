#include <iostream>
#include <string>
#include <cassert>

int main(int argc, char *argv[]) {
  if (argc == 3) {
    std::string ip = argv[1];
    int port = std::stoi(argv[2]);
  } else if (argc == 1) {
    std::cout << "IP address and port must be specified, see " << argv[0] << " --help for usage.";
    exit(0);
  } else if (argc == 2 && std::string(argv[1]) == "--help") {
    std::cout << "usage: " << argv[0] << " ip_address port" << std::endl;
    std::cout << "ip_address  : " << std::endl;
    std::cout << "port        : " << std::endl;
    exit(0);
  } else {
    std::cerr << "Unrecognized arguments, see " << argv[0] << " --help for usage." << std::endl;
    exit(-1);
  }
  return 0;
}