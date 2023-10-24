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
    // 初始化LOG，指定路径和文件名
    init_logger("./_LOG/server_log", "debug.log", "info.log", "warn.log", "error.log", "all.log");
    set_logger_mode(2); // DEBUG级别
    ChatRoomServer server;
    server.start_server(config["ip"], std::stoi(config["port"]), 20, 200);
    return 0;
}