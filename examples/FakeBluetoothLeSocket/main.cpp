
#include <iostream>
#include "LinuxUdpClientFake.h"

int main(int argc, char *argv[]) {
    LinuxUdpClientFake clientFake;
    clientFake.start_loop();


    while (true) {
        std::string input;
        getline(std::cin, input);
        if (input.compare("exit") == 0) {
            break;
        }
        const char *c = input.c_str();
        clientFake.send((uint8_t *) c, input.length());
    }
    clientFake.stop_loop();
}