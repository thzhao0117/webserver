#ifndef HTTPCON_H
#define HTTPCON_H

#include<sys/socket.h>
#include<signal.h>
#include<sys/epoll.h>
#include<sys/stat.h>
#include<pthread.h>
#include<sys/mman.h>

#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdarg.h>
#include<sys/uio.h>

#include "locker.h"


class http_con
{
public:
	//设置读取文件的名称
	static const int FILENAME_LEN=200;//文件名的最大长度

	static const int READ_BUFFER_SIZE = 2048;
	static const int WRITE_BUFFER_SIZE = 1024;
	//报文的请求方法
	enum METHOD
	{
		GET = 0,POST,HEAD,PUT,DELLATE//目前仅仅支持get
	};

	enum CHECK_STATE//主状态机状态
	{
		CHECK_STATE_REQUESTLINE = 0,
		CHECK_STATE_HEADER,
		CHECK_STATE_CONTENT
	};

	enum HTTP_CODE//报文解析的结果
	{
		NO_REQUEST,GET_REQUEST,BAD_REQUEST,
		NO_RESOURCE,FORBIDDEN_REQUEST,FILE_REQUEST,
		INTERBAL_ERRPOR,CLOSE_CONNECTION
	};

	enum LINE_STATUS
	{
		LINE_OK = 0, LINE_BAD, LINE_OPEN
	};


public:
	http_con() {}//构造函数和析构函数
	~http_con() {}

public:

	//初始化接收新的连接
	void init(int sockfd, const sockaddr_in& addr);
	void close_con(bool real_close = true);
	//处理客户请求
	void process();
	//读取客户端发来的全部数据
	bool read();
	//响应报文写入函数
	bool write();
	sockaddr_in* get_address()
	{
		return &m_address;
	}

private:
	void init();
	//从m_read_buf读取，并处理请求报文
	HTTP_CODE process_read();
	//向m_write_buf写入响应报文数据
	bool process_write(HTTP_CODE ret);

	//process_read调用分析http请求

	//主状态机解析报文中请求行的数据
	HTTP_CODE parse_request_line(char* text);
	//主状态机解析报文中的请求头数据
	HTTP_CODE parse_header(char* text);
	//主状态机解析报文中的请求内容
	HTTP_CODE parse_content(char* text);
	//生成响应报文
	HTTP_CODE do_request();



	//m_start_line是已经解析的字符
   //get_line用于将指针向后偏移，指向未处理的字符
	char* get_line()
	{
		return m_read_buf + m_start_line;
	
	}
	//从状态机读取一行，分析是请求报文的哪一部分
	LINE_STATUS parse_line();

	//这一组函数被process_write()调用填充应答
	void unmap();
	//根据响应报文格式，生成对应8个部分，以下函数均由do_request调用
	bool add_response(const char* format, ...);
	bool add_content(const char* content);
	bool add_status_line(int status, const char* title);
	bool add_headers(int content_length);
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();

public:
	static int m_epollfd;
	//统计用户数量
	static int m_user_count;

private:
	int m_sockfd;
	sockaddr_in m_address;

	//读缓冲区
	//存储读取的请求报文数据
	char m_read_buf[READ_BUFFER_SIZE];
	//缓冲区m_read_buf中，缓冲区中m_read_buf中数据的最后一个字节的下一个位置
	int m_read_idx;
	//m_read_buf读取的位置m_checked_idx
	int m_checked_idx;
	//当前正在解析的行的起始位置
	///m_read_buf中已经解析的字符个数
	int m_start_line;

	//写缓冲区
	char m_write_buf[WRITE_BUFFER_SIZE];
	char m_write_idx;

	//主状态机当前状态
	CHECK_STATE m_check_state;
	//请求方法
	METHOD m_method;

	//以下为解析请求报文中对应的6个变量

	//客户请求的目标文件的完整路径,内容等于doc_root+m_url,doc_root是网站根目录

	char m_real_file[FILENAME_LEN];
	
	char* m_url;
	char* m_version;
	char* m_host;
	int m_content_length;
	bool m_linger;//keep_alive,是否保持连接，长连接部分

	//客户请求的目标文件被mmap到内存中的位置
	//读取服务器上的文件地址
	char* m_file_address;
	//目标文件状态。
	struct stat m_file_stat;
	//writeV来写。
	struct iovec m_iv[2];
	int m_iv_count;//被写内存块得数目
	int bytes_to_send;
	int bytes_have_send;
	

};

#endif // !HTTP_CON

