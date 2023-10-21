#include <iostream>
#include <unistd.h>
#include <string.h>
#include "log.h"
#include "client.h"
#include "config.h"

int main()
{
    std::map<std::string, std::string> config;
    get_config_map("client.config", config);
    init_logger("client_log", "debug.log", "info.log", "warn.log", "error.log", "all.log");
    set_logger_mode(1);
    ChatRoomClient client;
    client.start_client(config["ip"], std::stoi(config["port"]));
    return 0;
}