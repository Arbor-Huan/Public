#ifndef CHARROOM_SERVER_H
#define CHARROOM_SERVER_H

#include <unordered_map>
#include <memory>
#include <sqlite3.h>
#include <unistd.h>
#include "socket_epoll.h"
#include "threadpool.h"

#define WELCOM_MES "Welcome to ChatRoom!\n请你根据提示进行注册、登录操作！"
#define ONLY_ONE_CAUTION "There is only one people in the chatroom!"
#define DATABASE_NAME "user.db"
#define TABLE_USER "user"
#define  OFFLINE -1 //用户下线（状态）
#define  ONLINE   1 //用户在线（状态）

class ServerEpollWatcher : public SocketEpollWatcher {
public:
    virtual int on_accept(EpollContext &epoll_context);
    virtual int on_readable(EpollContext &epoll_context);
    ServerEpollWatcher(); // 数据库初始化
    ~ServerEpollWatcher(); // 关闭数据库
    int db_user_if_reg(const char *name);
    int db_add_user(const char *name, const char *passwd);
    int db_user_if_online(const char *name, const char *passwd);
    int db_user_on_off(int fd,const char *name,unsigned int on_off);
    int db_broadcast(int fd, const std::string buf);
    int db_private(int fd, const char *dest, const std::string buf);
    int db_list_online_user(int fd);
    int db_fd_name(int fd, std::string &name);
    int db_change_name(int fd, const char *new_name);
    int db_change_password(int fd, const char *new_passwd);
private:
    sqlite3 *db;
};

class ChatRoomServer {
private:
    ServerEpollWatcher _server_handler; //多态，在Server构造时绑定地址
    SocketEpoll _socket_epoll; //关于Socket的所有操作

public:
    ChatRoomServer();

    int start_server(const std::string bind_ip, int port, int backlog, int max_events);

    int stop_server();
};

#endif //CHARROOM_SERVER_H
