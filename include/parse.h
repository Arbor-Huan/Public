//
// Created by Sixzeroo on 2018/6/6.
//

#ifndef CHARROOM_PARSE_H
#define CHARROOM_PARSE_H

#include <string>

#define MSG_CHAR_SIZE 4000
#define BUFF_SIZE 4096

/*return code*/
#define OP_OK 0X80000000                   //操作成功
#define NAME_FAIL 0X80000001           //注册信息，用户名已经存在或者不符合规范
#define NAME_PWD_NMATCH 0X80000002 //登录时，输入的用户名密码不对
#define USER_LOGED 0X80000003           //登录时，提示该用户已经在线
#define USER_NOT_REGIST 0X80000004 //提示用户不存在

enum MsgType {
    M_NORMAL = 1,   // 常规消息or公聊
    M_PRIVATE = 2,  // 私聊
    M_EXIT = 3,     //  退出
    M_CNAME = 4,    //  改名请求
    M_REGISTER = 5,  // 注册类型
    M_LOGIN = 6,     // 登录类型
    M_ONLINEUSER = 7  // 在线用户
};

// 消息格式
struct MsgPacket {
    int code;        // 消息类型
    int state;      //存储命令返回信息
    char name[32]; //用户名
    char context[MSG_CHAR_SIZE];
};

class Msg {
public:
    Msg() = default;

    Msg(int code, int state, std::string name, std::string context);

    int code;
    int state; 
    std::string name;
    std::string context;

    int send_diy(int fd);

    int recv_diy(int fd);
};

#endif //CHARROOM_PARSE_H
