#include "webserver.h"

int main() {
    webserver server("127.0.0.1", "1024", 8);

    server.run();
}
