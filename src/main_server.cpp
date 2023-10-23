#include <string>
#include <unordered_map>

#include "../include/server.h"
#include "../include/parse.h"
#include "../include/config.h"
#include "../include/log.h"

int main()
{
    // 设置配置
    std::map<std::string, std::string> config;
    get_config_map("server.config", config);
    init_logger("./_LOG/server_log", "debug.log", "info.log", "warn.log", "error.log", "all.log");
    set_logger_mode(1);
    // 其构造函数会进行多态的地址传递
    ChatRoomServer server;
    server.start_server(config["ip"], std::stoi(config["port"]), 20, 200);
    return 0;
}