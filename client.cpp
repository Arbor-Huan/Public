//
// Created by Sixzeroo on 2018/6/7.
//

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include "client.h"
#include "log.h"
#include "config.h"

ChatRoomClient::ChatRoomClient()
{
    _server_ip = "";
    _server_port = -1;
    _client_fd = -1;
    _epollfd = -1;
}

void ChatRoomClient::set_server_ip(const std::string &_server_ip) {
    LOG(DEBUG)<<"Set sever ip: "<<_server_ip<<std::endl;
    ChatRoomClient::_server_ip = _server_ip;
}

void ChatRoomClient::set_server_port(int _server_port) {
    LOG(DEBUG)<<"Set sever port: "<<_server_port<<std::endl;
    ChatRoomClient::_server_port = _server_port;
}

int ChatRoomClient::connect_to_server(std::string server_ip, int port) {
    if(_client_fd != -1)
    {
        LOG(ERROR)<<"Connect to server: epoll had created"<<std::endl;
        return -1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    unsigned int ip = inet_addr(server_ip.c_str());
    server_addr.sin_addr.s_addr = ip;

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
    {
        LOG(ERROR)<<"Connect to server: create socket error"<<std::endl;
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        LOG(ERROR)<<"Connect to server: connnect to server error"<<std::endl;
        return -1;
    }
    _client_fd = sockfd;

    // 读一次server信息
    Msg recv_m;
    recv_m.recv_diy(_client_fd);
    std::cout << recv_m.context << std::endl;

    return 0;
}

int ChatRoomClient::set_noblocking(int fd) {
    int flag = fcntl(fd, F_GETFL, 0);
    if(flag < 0)
        flag = 0;
    int ret = fcntl(fd, F_SETFL, flag | O_NONBLOCK);
    if(ret < 0)
    {
        // LOG ERROR
        return -1;
    }
    return 0;
}

int ChatRoomClient::addfd(int epollfd, int fd, bool enable_et) {
    struct epoll_event ev;
    ev.data.fd = fd;
    // 允许读
    ev.events = EPOLLIN;
    // 设置为边缘出发模式
    if(enable_et)
    {
        ev.events = EPOLLIN | EPOLLET;
    }
    // 添加进epoll中
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev);
    set_noblocking(fd);
    return 0;
}

std::string ChatRoomClient::get_time_str() {
    time_t tm;
    time(&tm);
    char time_string[128];
    ctime_r(&tm, time_string);
    std::string s_time_string = time_string;
    s_time_string.pop_back();
    s_time_string = "[" + s_time_string + "]";

    return s_time_string;
}

int ChatRoomClient::work_loop() { //【增加注释】
    // 检查epoll是否已经创建
    if(_epollfd != -1) {
        LOG(ERROR) << "Start client error: epoll created" << std::endl;
        return -1;
    }

    // 创建管道
    int pipefd[2];
    if(pipe(pipefd) < 0) {
        LOG(ERROR) << "Start client error: create pipe error" << std::endl;
        return -1;
    }

    // 创建epoll
    _epollfd = epoll_create(1024);
    LOG(DEBUG) << "Start client: create epoll : " << _epollfd << std::endl;
    if(_epollfd < 0) {
        // LOG ERROR
        return -1;
    }
    static struct epoll_event events[2];

    // 将客户端文件描述符和管道添加到epoll
    addfd(_epollfd, _client_fd, true);
    addfd(_epollfd, pipefd[0], true);

    bool isworking = true;

    // 创建子进程
    int pid = fork();
    if(pid < 0) {
        LOG(ERROR) << "Start client: fork error" << std::endl;
        return -1;
    } else if(pid == 0) { // 子进程
        LOG(DEBUG) << "Client: create child successful" << std::endl;
        close(pipefd[0]);
        LOG(DEBUG) << "Client: child process close pipefd[0] successful" << std::endl;
        char message[BUFF_SIZE];

        printf("Please input '%s' to exit chat room\n", EXIT_MSG);

        // 子进程循环，读取用户输入并写入管道
        while (isworking) {
            bzero(message, BUFF_SIZE);
            fgets(message, BUFF_SIZE, stdin);

            if(strncasecmp(message, EXIT_MSG, strlen(EXIT_MSG)) == 0) {
                LOG(INFO) << "Client exit" << std::endl;
                isworking = false;
            }
            if(write(pipefd[1], message, strlen(message)) < 0) {
                LOG(ERROR) << "Client child process: write error" << std::endl;
                return -1;
            }
        }
    } else { // 父进程
        close(pipefd[1]);
        LOG(DEBUG) << "Client: parent process close pipefd[1] successful" << std::endl;

        // 父进程循环，处理epoll事件
        while (isworking) {
            int epoll_event_count = epoll_wait(_epollfd, events, 2, -1);

            for(int i = 0; i < epoll_event_count; i++) {
                if(events[i].data.fd == _client_fd) { // 从服务器接收消息
                    LOG(DEBUG) << "Client epoll: get events from sockfd" << std::endl;
                    Msg recv_m;
                    recv_m.recv_diy(_client_fd);
                    // 1、服务器关闭连接
                    if(recv_m.code == M_EXIT) { 
                        LOG(INFO) << "Client epoll: close connetct" << std::endl;
                        isworking = false;
                    }
                    // 2、改名回执
                    else if(recv_m.code==M_CNAME) {
                        if(recv_m.state==OP_OK)
                            std::cout << "\n【响应】:改名成功！" << std::endl;
                        else
                            std::cout << "\n【响应】:改名失败！可能是字符过长或者用户不存在！" << std::endl;
                    }                    
                    // 3、收到公聊or私聊信息
                    else if(recv_m.code==M_NORMAL || recv_m.code==M_PRIVATE) {
                        std::string time_str = get_time_str();
                        std::cout << time_str << recv_m.context << std::endl;
                    }
                    // 4、其他系统回执(包括无多种情况的回执，以及错误回执。这里没必要使用状态码)
                    else {
                        std::cout << recv_m.context << std::endl;
                    }
                } 
                else { // 从终端读取消息
                    LOG(DEBUG) << "Client epoll: get events from terminal" << std::endl;
                    char message[BUFF_SIZE];
                    bzero(message, sizeof(message));
                    ssize_t ret = read(events[i].data.fd, message, BUFF_SIZE);

                    // 处理客户端退出聊天室的情况
                    if(strncasecmp(message, EXIT_MSG, strlen(EXIT_MSG)) == 0) {
                        isworking = false;
                        Msg send_m;
                        send_m.code = M_EXIT;
                        send_m.send_diy(_client_fd);
                        LOG(DEBUG) << "Client epoll: send exit msg to server" << std::endl;
                        continue;
                    } 
                    // 处理改名的情况
                    else if(strncasecmp(message, CHANGE_NAME_MSG, strlen(CHANGE_NAME_MSG)) == 0) {
                        Msg send_m;
                        send_m.code = M_CNAME;
                        std::cout << "输入更改后的用户名:" << std::endl << "#";
                        std::cin >> send_m.context;
                        std::cin.get();
                        send_m.send_diy(_client_fd);
                        std::cout << "等待服务器响应，waiting..." << std::endl;
                        LOG(DEBUG) << "Client epoll: send change name msg to server" << std::endl;
                        continue;
                    }
                    // 打印在线列表
                    else if(strncasecmp(message, ONLINE_LIST, strlen(ONLINE_LIST)) == 0) {
                        Msg send_m;
                        send_m.code = M_ONLINEUSER;
                        send_m.send_diy(_client_fd);
                        std::cout << "waiting" << std::endl;
                        continue;
                    }
                    // 处理私聊的情况
                    else if(strncasecmp(message, PRIVATE_CHAT, strlen(PRIVATE_CHAT)) == 0) {
                        Msg send_m;
                        send_m.code = M_PRIVATE;
                        std::cout << "输入接收的用户名:" << std::endl ;
                        std::cin >> send_m.name;
                        std::cin.get();

                        std::cout << "say:" << std::endl ;
                        std::cin >> send_m.context;
                        std::cin.get();
                        send_m.send_diy(_client_fd);
                        continue;
                    }
                    // 默认公聊：
                    Msg send_m;
                    send_m.code = M_NORMAL;
                    send_m.context = message; // 这个键盘读进来的message自带\n
                    send_m.send_diy(_client_fd);
                    LOG(DEBUG) << "Client epoll: send msg to server" << std::endl;
                }
            }
        }
    }

    // 清理资源
    if(pid) {
        close(pipefd[0]);
        close(_epollfd);
    } else {
        close(pipefd[1]);
    }

    return 0;
}

int ChatRoomClient::registe(int fd) {
    Msg msg, msgback;
    msg.code = M_REGISTER;
    std::cout << "请输入您的用户名：" << std::endl;
    std::cin >> msg.name;
    std::cin.get();
    std::cout << "请输入您的密码：" << std::endl;
    std::cin >> msg.context;
    std::cin.get();
    std::cout<<"yonghu:"<<msg.name<<msg.context<<std::endl;
    msg.send_diy(_client_fd);

    msgback.recv_diy(_client_fd);
    if (msgback.state != OP_OK) {
        std::cout << "用户名已存在，请重试！" << std::endl;
        return -1;
    } else {
        std::cout << "注册成功！" << std::endl;
        return 0;
    }

}

int ChatRoomClient::login(int fd) {
    Msg msg, msgback;
    msg.code = M_LOGIN;
    std::cout << "请输入您的用户名：" << std::endl;
    std::cin >> msg.name;
    std::cin.get();
    std::cout << "请输入您的密码：" << std::endl;
    std::cin >> msg.context;
    std::cin.get();

    msg.send_diy(_client_fd);
    msgback.recv_diy(_client_fd);
    if (msgback.state != OP_OK) {
        std::cout << "用户名或密码不匹配，请重试！" << std::endl;
        return -1;
    } else {
        std::cout << "登录成功！" << std::endl;
        return 0;
    }

}

int ChatRoomClient::start_client(std::string ip, int port) {
    set_server_port(port);

    set_server_ip(ip);

    if(connect_to_server(_server_ip, _server_port) < 0)
    {
        LOG(ERROR)<<"Client start: connect to server error"<<std::endl;
        return -1;
    }

    int sel;
    std::cout << "\t 1 注册" << std::endl;
    std::cout << "\t 2 登录" << std::endl;

    while (true)
    {  
        std::cin >> sel;
        std::cin.get();
        if (sel == 1) {
            registe(_client_fd);
        } 
        else if (sel == 2) {
            if(login(_client_fd) != -1)
                break; //只有登录成功才往下执行
        } 
        else {
            std::cout << "Please try again!" << std::endl;
            std::cin.clear(); // 清除错误状态
            char ch;
            // 清除输入缓冲区
            while((ch = std::cin.get()) != '\n' && ch != EOF);
        }
    }
    std::cout << "指令表：" << std::endl;
    std::cout << "\t/pvt--私聊" << std::endl;
    std::cout << "\t/ol--在线列表" << std::endl;
    std::cout << "\t/rn--更改名字" << std::endl;
    std::cout << "\t/exit--退出" << std::endl;
    // 先注册登录再循环的好处：减少“空跑”
    return work_loop();
}

