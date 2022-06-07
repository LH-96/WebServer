#include "webserver.h"

int main(int, char**) {
    webserver server("127.0.0.1", "1024", "ET");

    server.run();
}
