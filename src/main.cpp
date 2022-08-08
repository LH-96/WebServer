#include "webserver.h"

int main() {
    webserver server("127.0.0.1", "10240", 8);

    server.run();
}
