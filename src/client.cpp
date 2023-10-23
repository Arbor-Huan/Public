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
#include <signal.h>

#include "../include/client.h"
#include "../include/log.h"
#include "../include/config.h"

ChatRoomClient::ChatRoomClient()
{
    _server_ip = "";
    _server_port = -1;
    _server_fd = -1;
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
    if(_server_fd != -1)
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
    _server_fd = sockfd;

    // 读一次server信息（欢迎信息）
    Msg recv_m;
    recv_m.recv_diy(_server_fd);
    std::cout << recv_m.context << std::endl;

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

int ChatRoomClient::work_loop() { 
    // 注册退出信号
    signal(SIGUSR1, exit);
    // 创建子进程
    int pid = fork();
    if(pid < 0) {
        LOG(ERROR) << "Start client: fork error" << std::endl;
        return -1;
    } else if(pid == 0) { // 子进程
        char message[BUFF_SIZE];
        // 子进程循环，读取用户输入并写入管道
        while (1) {
            bzero(message, BUFF_SIZE);
            fgets(message, BUFF_SIZE, stdin);
            // 处理客户端退出聊天室的情况
            if(strncasecmp(message, EXIT_MSG, strlen(EXIT_MSG)) == 0) {
                Msg send_m;
                send_m.code = M_EXIT;
                send_m.send_diy(_server_fd);
                // 发送信号让父进程退出
                kill(getppid(), SIGUSR1);
                LOG(DEBUG) << "Client epoll: send exit msg to server" << std::endl;
                break;
            } 
            // 处理改名的情况
            else if(strncasecmp(message, CHANGE_NAME_MSG, strlen(CHANGE_NAME_MSG)) == 0) {
                Msg send_m;
                send_m.code = M_CNAME;
                std::cout << "输入更改后的用户名:" << std::endl << "#";
                std::cin >> send_m.context;
                std::cin.get();
                send_m.send_diy(_server_fd);
                std::cout << "等待服务器响应，waiting..." << std::endl;
                LOG(DEBUG) << "Client epoll: send change name msg to server" << std::endl;
                continue;
            }
            // 打印在线列表
            else if(strncasecmp(message, ONLINE_LIST, strlen(ONLINE_LIST)) == 0) {
                Msg send_m;
                send_m.code = M_ONLINEUSER;
                send_m.send_diy(_server_fd);
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
                send_m.context += "\n";
                send_m.send_diy(_server_fd);
                continue;
            }
            // 默认公聊：
            else {
                Msg send_m;
                send_m.code = M_NORMAL;
                send_m.context = message; // 这个键盘读进来的message自带\n
                send_m.send_diy(_server_fd);
                LOG(DEBUG) << "Client epoll: send msg to server" << std::endl;
            }
            
        }
    } else { // 父进程
        while (1) {
            Msg recv_m;
            int ret = recv_m.recv_diy(_server_fd);
            // 1、服务器关闭连接
            if(ret == -1) { 
                LOG(INFO) << "Client epoll: close connetct" << std::endl;
                std::cout << "服务器已下线" << std::endl;
                // 发送信号让子进程退出
                kill(pid, SIGUSR1);
                break;
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
            else 
                std::cout << recv_m.context << std::endl;
        }
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
    msg.send_diy(_server_fd);

    msgback.recv_diy(_server_fd);
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

    msg.send_diy(_server_fd);
    msgback.recv_diy(_server_fd);
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
        std::cout << "连接服务器失败！" << std::endl;
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
            registe(_server_fd);
        } 
        else if (sel == 2) {
            if(login(_server_fd) != -1)
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
    // 先注册登录再循环，减少“空跑”
    return work_loop();
}

