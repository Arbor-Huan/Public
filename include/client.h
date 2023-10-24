#ifndef CHARROOM_CLIENT_H
#define CHARROOM_CLIENT_H

#include <string>

#include "parse.h"

#define EXIT_MSG "/exit"
#define CHANGE_NAME_MSG "/rn"
#define ONLINE_LIST "/ol"
#define PRIVATE_CHAT "/pvt"

class ChatRoomClient {
private:
    std::string _server_ip;
    int _server_port;
    int _server_fd;

public:

    ChatRoomClient();

    ~ChatRoomClient() = default;

    void set_server_ip(const std::string &_server_ip);

    void set_server_port(int _server_port);

    int connect_to_server(std::string server_ip, int port);

    int work_loop();

    int registe(int fd);
    
    int login(int fd);

    int start_client(std::string ip, int port);

    std::string get_time_str();
};

#endif //CHARROOM_CLIENT_H 
