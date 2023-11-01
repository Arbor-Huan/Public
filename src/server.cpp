#include <string>
#include <unordered_map>
#include <sqlite3.h>
#include <cstring>
#include "../include/server.h"
#include "../include/parse.h"
#include "../include/config.h"
#include "../include/log.h"

/**************class ChatRoomServer**************/
ChatRoomServer::ChatRoomServer() {
    _socket_epoll.set_watcher(&_server_handler);
}

int ChatRoomServer::start_server(const std::string bind_ip, int port, int backlog, int max_events) {
    LOG(INFO)<<"Server start: start"<<std::endl;
    std::cout<<"ChatRoom start!"<<std::endl;
    _socket_epoll.set_bind_ip(bind_ip);
    _socket_epoll.set_port(port);
    _socket_epoll.set_backlog(backlog);
    _socket_epoll.set_max_events(max_events);
    return _socket_epoll.start_epoll();
}

int ChatRoomServer::stop_server() {
    LOG(INFO)<<"Server: stop server"<<std::endl;
    return _socket_epoll.stop_epoll();
}

/**************class ServerEpollWatcher**************/
ServerEpollWatcher::ServerEpollWatcher()
{
	int ret;
	char *errmsg;	
	char sql[] =
	"CREATE TABLE IF NOT EXISTS user(name INT PRIMARY KEY NOT NULL, passwd TEXT NOT NULL, fd INT NOT NULL);";

	if (sqlite3_open(DATABASE_NAME, &db) < 0)
	{
		LOG(ERROR) << "fail to sqlite3_open : " << sqlite3_errmsg(db) << std::endl;
        exit(1);
	}

	ret = sqlite3_exec(db, sql, NULL, 0, &errmsg);

	if (ret != SQLITE_OK) {
	std::cerr << "SQL error: " << errmsg << std::endl;
	sqlite3_free(errmsg);
	} else {
	std::cout << "Table created successfully" << std::endl;
	}
}
ServerEpollWatcher::~ServerEpollWatcher()
{
	sqlite3_close(db);
}

// 处理接受请求逻辑
int ServerEpollWatcher::on_accept(EpollContext &epoll_context) {
    LOG(DEBUG)<<"Handle: on_accept start"<<std::endl;
    int client_fd = epoll_context.fd;

    printf("client %s:%d connected to server\n", epoll_context.client_ip.c_str(), epoll_context.client_port);
    // 返回欢迎信息和提示操作信息
    Msg m;
    m.code = M_NORMAL;
    m.context = WELCOM_MES;

    int ret = m.send_diy(client_fd);

    return ret;
}


// 处理来信请求, 出错返回-1， 退出连接返回-2
int ServerEpollWatcher::on_readable(EpollContext &epoll_context) {
    int client_fd = epoll_context.fd;
    Msg recv_m;
    int ret = recv_m.recv_diy(client_fd);
    // 1.处理客户端退出的情况(ret作为异常断连的标志)
    if(recv_m.code == M_EXIT || ret == -1)
    {
		// 设置fd为下线
		db_user_on_off(client_fd, nullptr, OFFLINE);
        return -2; // 让上层去除epoll检测
    }
    // 2.1改名请求
    else if(recv_m.code == M_CNAME)
    {
		Msg msg_back;
		msg_back.code = M_CNAME; 
		if(ret = db_change_name(client_fd, recv_m.context.c_str()) == 0) 
        {
            msg_back.state = OP_OK; 
			// msg_back.context = "Name Changed Successfully!";
        }
        else if(ret == -2)
        {
            msg_back.state = USER_NOT_REGIST; 
			// msg_back.context = "Name Change Failed!";
        }
		msg_back.send_diy(client_fd);
        return 0;
    }
	// 2.2更改密码请求
	else if(recv_m.code == M_CPASSWORD)
	{
		Msg msg_back;
		msg_back.code = M_CPASSWORD; 
		if(db_change_password(client_fd, recv_m.context.c_str()) == 0) 
        {
            msg_back.state = OP_OK; 
			// msg_back.context = "Password Changed Successfully!";
        }
        else
        {
            msg_back.state = USER_NOT_REGIST;
			// msg_back.context = "Password Change Failed!";
        }
		msg_back.send_diy(client_fd);
        return 0;
	}
    // 3.注册请求
    else if(recv_m.code == M_REGISTER)
    {
        int dest_index;
        Msg msg_back;
        msg_back.code = M_REGISTER;	
        
        //查询用户是否注册
        dest_index = db_user_if_reg(recv_m.name.c_str());

        if(dest_index == -1)
        {	
            if(db_add_user(recv_m.name.c_str(), recv_m.context.c_str()) == -1)
			{
				//用户名或密码不符合要求
				msg_back.state = NAME_FAIL;
				LOG(INFO) << "user " << recv_m.name << " regist fail!" << std::endl;
			}
            msg_back.state = OP_OK;	
			LOG(INFO) << "user " << recv_m.name << " regist success!" << std::endl;
        }else{ //失败，已注册
            msg_back.state = NAME_FAIL;
			LOG(INFO) << "user " << recv_m.name << " exist!" << std::endl;
        }
        msg_back.send_diy(client_fd);
        return 0;
    }

    // 4.登录请求
    else if(recv_m.code == M_LOGIN)
    {
        int ret;
        int dest_index;
        std::string content;
        Msg msg_back;
        msg_back.code = M_LOGIN;	

        //查询用户是否注册
        // dest_index = db_user_if_reg(recv_m.name.c_str());
        // if(dest_index == -1)
        // {
        //     msg_back.state = USER_NOT_REGIST;
        //     std::cout << "user " << recv_m.name << " login fail!" << std::endl;
        //     msg_back.send_diy(client_fd);
        //     return 0;
        // }

        //判断某个用户是否在线,同时验证密码
		/*返回值 1： 在线
				-1：不在线
				-2：用户不存在*/
        ret = db_user_if_online(recv_m.name.c_str(),recv_m.context.c_str());
        LOG(DEBUG) << "login() ret=" << ret << std::endl;
        if(ret == -2) 
        {
            msg_back.state = USER_NOT_REGIST;
            LOG(WARN) << "user " << recv_m.name << " login fail!" << std::endl;
            msg_back.send_diy(client_fd);
            return 0;
        }else if(ret == -1){
			//存在，可以登录
            db_user_on_off(client_fd,recv_m.name.c_str(),ONLINE);

            msg_back.state = OP_OK;
            LOG(INFO) << "user " << recv_m.name << " login success!" << std::endl;
            msg_back.send_diy(client_fd);
        }else{ //已经在线
            msg_back.state = USER_LOGED;
            LOG(WARN)<<"user had login already\n"<<std::endl;
    
            msg_back.send_diy(client_fd);
            return 0;
        }
        //通知所有客户端，某个用户上线了
		content = "[NOTICE]:" + recv_m.name + " online";
        LOG(INFO)<<recv_m.name<<" online"<<std::endl;
        db_broadcast(client_fd, content);
    }

    // 5.私聊
    else if(recv_m.code == M_PRIVATE)
    {
        std::string content, client_name;
		// 获取客户端名字
		db_fd_name(client_fd, client_name);
		content = "[Private]" + client_name + " say:" + recv_m.context;
		LOG(DEBUG) << "\n" << __func__ << "() to " << recv_m.name << " : content:" << recv_m.context << std::endl;
        db_private(client_fd, recv_m.name.c_str(), content);
    }
    // 6.打印在线列表
    else if(recv_m.code == M_ONLINEUSER)
    {
        db_list_online_user(client_fd);
    }
    // 7.默认公聊：
    else
    {
		// 处理消息格式
		std::string meg_str, client_name;
		db_fd_name(client_fd, client_name);
		meg_str = "[Public]" + client_name + " say:" + recv_m.context;
		db_broadcast(client_fd, meg_str);
		
    }
    return 0;
}

//添加一个用户
int ServerEpollWatcher::db_add_user(const char *name, const char *passwd)
{
	char *errmsg;
	char sqlstr[1024]={0};

	if((name == NULL)||(passwd == NULL))
	{
		perror("invalide name or passwd\n");
		return -1;
	}
	if((strlen(name)>32)||(strlen(passwd)>32))
	{
		perror("invalide name or passwd length\n");
		return -1;
	}
	// std::cout << name <<std::endl;
	sprintf(sqlstr, "insert into %s values('%s', '%s', -1)",
		TABLE_USER,name, passwd);
	
    LOG(DEBUG) << "cmd:" << sqlstr << std::endl;

	if (sqlite3_exec(db, sqlstr, NULL, NULL, &errmsg) != 0)
	{
		printf("error : %s\n", sqlite3_errmsg(db));
	}
	else
	{
		printf("insert is done\n");
	}
	printf("\n");

	return 0;	
}

int ServerEpollWatcher::db_user_if_reg(const char *name)
{
	int state = -1;
	char **result, *errmsg;
	int nrow, ncolumn;
	char sqlstr[1024]={0};

	// 【up】去除了regist这一列，改为fd
	sprintf(sqlstr, "select fd from %s where name='%s'",
		TABLE_USER,name);

	if (sqlite3_get_table(db, sqlstr, &result, &nrow, &ncolumn, &errmsg) != 0)
	{
		printf("error : %s\n", errmsg);
		sqlite3_free(errmsg);
	}

	LOG(DEBUG) << "ncolumn:" << ncolumn 
    << "  nrow:" << nrow << std::endl;

	if(nrow > 0)
	{
		//找到
		state = 0;
	}else{
		//没找到
		state = -1;
	}
	sqlite3_free_table(result);

	return state;
}

/*
返回值 1： 在线
      -1：不在线
      -2：用户不存在
*/
//判断某个用户是否在线
int ServerEpollWatcher::db_user_if_online(const char *name, const char *passwd)
{
	int ret;
	int state = -1;
	char **result, *errmsg;
	int nrow, ncolumn, index;
	char sqlstr[1024]={0};

	sprintf(sqlstr, "select fd from %s where name='%s' and passwd='%s'",
		TABLE_USER,name,passwd);

	if (sqlite3_get_table(db, sqlstr, &result, &nrow, &ncolumn, &errmsg) != 0)
	{
		printf("error : %s\n", errmsg);
		sqlite3_free(errmsg);
	}
	
	index = ncolumn;

    LOG(DEBUG) << __func__ << "() ncolumn:" << ncolumn << " nrow=" << nrow << std::endl;

	if(nrow == 1)
	{
		//找到
		state = atoi(result[index]);
		if(state > 0)
		{
			ret = 1;
		}else{
			ret = -1;
		}
	}else{
		//没找到
		ret = -2;
	}
	sqlite3_free_table(result);

	return ret;
}

// 改名
int ServerEpollWatcher::db_change_name(int fd, const char *new_name)
{
	char* errmsg;
    char sqlstr[1024] = {0};
	if((strlen(new_name)>32) || (new_name==nullptr))
		return -1;
    sprintf(sqlstr, "UPDATE %s SET name = '%s' WHERE fd = %d", TABLE_USER, new_name, fd);

    if (sqlite3_exec(db, sqlstr, NULL, NULL, &errmsg) != SQLITE_OK) {
        LOG(ERROR) << "Error updating name: " << errmsg << std::endl;
        sqlite3_free(errmsg);
        return -2;
    }
    return 0;
}

// 改密码
int ServerEpollWatcher::db_change_password(int fd, const char *new_passwd)
{
	char* errmsg;
    char sqlstr[1024] = {0};
    sprintf(sqlstr, "UPDATE %s SET passwd = '%s' WHERE fd = %d", TABLE_USER, new_passwd, fd);

    if (sqlite3_exec(db, sqlstr, NULL, NULL, &errmsg) != SQLITE_OK) {
        LOG(ERROR) << "Error updating passwd: " << errmsg << std::endl;
        sqlite3_free(errmsg);
        return -1;
    }
    return 0;
}

// 某个用户上线
// ：UPDATE student SET fd=%d WHERE name='张立';
// 某个用户下线
// ：UPDATE student SET fd=-1 WHERE name='张立';
int ServerEpollWatcher::db_user_on_off(int fd, const char *name,unsigned int on_off)
{
	char *errmsg;
	char sqlstr[1024]={0};
	if(name != nullptr)
	{
		if(strlen(name)>32)
		{
			perror("invalide name or passwd length\n");
			return -1;
		}
	}

	if(on_off == ONLINE)
	{
		// 涉及登录，需要用户名
		sprintf(sqlstr, "UPDATE %s set fd=%d where name='%s'",
		TABLE_USER, fd, name);
	}else if(on_off == OFFLINE){
		// 下线不需要用户名
		sprintf(sqlstr, "UPDATE %s set fd=-1 where fd=%d",
		TABLE_USER, fd);
	}
	
	LOG(DEBUG) << "cmd: " << sqlstr << std::endl;

	if (sqlite3_exec(db, sqlstr, NULL, NULL, &errmsg) != 0)
	{
		printf("error : %s\n", sqlite3_errmsg(db));
	}
	else
	{
		printf("insert is done\n");
	}

	return 0;	
}

// 列出所有在线用户
int ServerEpollWatcher::db_list_online_user(int fd)
{
	char **result, *errmsg;
	Msg msg;	
	int nrow, ncolumn, i, index;
	char sqlstr[1024]={0};
	msg.code = M_ONLINEUSER;
	
	sprintf(sqlstr, "select name from %s where fd>0",
		TABLE_USER);

	if (sqlite3_get_table(db, sqlstr, &result, &nrow, &ncolumn, &errmsg) != 0)
	{
		printf("error : %s\n", errmsg);
		sqlite3_free(errmsg);
		return -1;
	}
	
	index = ncolumn;

    LOG(DEBUG) << "ncolumn:" << ncolumn << " nrow=" << nrow << std::endl;
	std::string append;
	msg.context += "[在线列表]:";
	for(i=0; i<nrow; i++)
	{
		append = result[index];
		msg.context = msg.context + " " + append;
		LOG(DEBUG) << "list online name =" << result[index] << " fd=" << fd  <<std::endl;
		index++;
	}
	msg.send_diy(fd);

	sqlite3_free_table(result);

	return 1;
}

// 公聊
int ServerEpollWatcher::db_broadcast(int fd, const std::string buf)
{
	int peer_fd;
	int state = -1;
	char **result, *errmsg;
	int nrow, ncolumn, i, index;
	char sqlstr[1024]={0};
	Msg msg;
	msg.code = M_NORMAL;
	msg.context = buf;

	sprintf(sqlstr, "select fd from %s where fd>0",TABLE_USER);

	if (sqlite3_get_table(db, sqlstr, &result, &nrow, &ncolumn, &errmsg) != 0)
	{
		printf("error : %s\n", errmsg);
		sqlite3_free(errmsg);
		return -1;
	}
	
	index = ncolumn;

    LOG(DEBUG) << "ncolumn:" << ncolumn << " nrow=" << nrow << std::endl;

	for(i=0;i<nrow;i++)
	{
		peer_fd = atoi(result[index]);
		index++;
		LOG(DEBUG) << "peer_fd:" << peer_fd << std::endl;
		if(peer_fd != fd)
		{	
			msg.send_diy(peer_fd);
		}	
	}
	sqlite3_free_table(result);

	return state;
}

// 私聊
int ServerEpollWatcher::db_private(int fd, const char *dest, const std::string buf)
{
	int peer_fd;
	int state = -1;
	char **result, *errmsg;
	int nrow, ncolumn, index;
	char sqlstr[1024]={0};
	Msg msg;

	sprintf(sqlstr, "select fd from %s where name='%s'",TABLE_USER,dest);

	LOG(DEBUG) << "sqlstr:" << sqlstr << std::endl;

	if (sqlite3_get_table(db, sqlstr, &result, &nrow, &ncolumn, &errmsg) != 0)
	{
		printf("error : %s\n", errmsg);
		sqlite3_free(errmsg);
		return -1;
	}
	
	index = ncolumn;

	if(nrow>0)
	{
		//find
		peer_fd = atoi(result[index]);
		index++;
		msg.code = M_PRIVATE;
		msg.context = buf;
		msg.send_diy(peer_fd);
	}else{
		//not find
		// 这部分不需要做state，直接用常规消息发回去错误信息，提醒用户即可
		msg.code = M_NORMAL;
		msg.context = "there is no user :" + std::string(dest) + "\n";
		msg.send_diy(fd);
	}

	sqlite3_free_table(result);
	
	return state;
}

// fd查name
int ServerEpollWatcher::db_fd_name(int fd, std::string &name)
{
    char **result, *errmsg;
	char buff[512];
    int nrow, ncolumn;
	sprintf(buff, "SELECT name FROM %s WHERE fd = %d;", TABLE_USER, fd);
    if (sqlite3_get_table(db, buff, &result, &nrow, &ncolumn, &errmsg) != 0)
    {
		printf("error : %s\n", errmsg);
		sqlite3_free(errmsg);
		return -1;
    }
	// 第一个数组是列名
	if (nrow > 0) {
		name = result[ncolumn];
        // strncpy(name, result[ncolumn], name_len);
		// name[name_len - 1] = '\0';
    } else {
		LOG(WARN) << "No results found for fd " << fd << "." << std::endl;
		return -1;
    }
    sqlite3_free_table(result);
    return 0;
}




