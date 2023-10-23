//
// Created by Sixzeroo on 2018/6/7.
//

#include <string.h>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>

#include "../include/parse.h"
#include "../include/log.h"

Msg::Msg(int code, int state, std::string name, std::string context)
 {
    this->code = code;
    this->state = state;
    this->name = name;
    this->context = context;
}

int Msg::send_diy(int fd) {
    struct MsgPacket data;
    data.code = code;
    data.state = state;
    strncpy(data.context, context.c_str(), sizeof(data.context));
    strncpy(data.name, name.c_str(), sizeof(data.name));
    char message[BUFF_SIZE];
    bzero(message, sizeof(message));
    memcpy(message, &data, sizeof(data));
    if(send(fd, message, BUFF_SIZE, 0) < 0)
    {
        LOG(ERROR)<<"send error"<<std::endl;
        return -1;
    }
    return 0;
}

int Msg::recv_diy(int fd) {
    char buf[BUFF_SIZE];
    bzero(buf, sizeof(buf));
    ssize_t len = recv(fd, buf, BUFF_SIZE, 0);

    if(len == 0)
    {
        LOG(ERROR)<<"receve len = 0 msg"<<std::endl;
        return -1;
    }

    struct MsgPacket data;
    memcpy(&data, buf, sizeof(data));
    state = data.state;
    name = data.name;
    code = data.code;
    context = data.context;
    LOG(DEBUG)<<"Parse: Receive msg : msg code = "<<code<<std::endl;
}