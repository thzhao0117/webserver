#include "http_con.h"
#include<stdio.h>

const char* const OK_200_TITILE = "OK";
const char* const ERROR_400_TITILE = "Bad Request";
const char* const ERROR_400_FORM = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* const ERROR_403_TITLE = "Forbidden";
const char* const ERROR_403_FORM = "You don't have the permission to get file from this server.\n";
const char* const ERROR_404_TITLE = "Not Fount";
const char* const ERROR_404_FORM = "The requested file was not found on this server.\n";
const char* const ERROR_500_TITLE = "Internal Error";
const char* const ERROR_500_FORM = "There was an unusual problem serving the requested file.\n";

// website roor dir网站根目录，文件夹内存放请求的资源和跳转的html文件
//
const char* const doc_root = "/home/abbac/projects/webserv/bin/x64/Debug";

//对文件描述符设置为非阻塞，因为是epoll的边缘触发
int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

///内核事件表注册新事件，开启EPOLLONESHOT，针对客户端连接的描述符，listenfd不用开启


void addfd(int epollfd, int fd, bool one_shot)
{
	epoll_event event;
	event.data.fd = fd;
	event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
	if (one_shot)
	{
		event.events |= EPOLLONESHOT;

	}
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);//向内核注册fd并且设置非阻塞
}

//内核事件表删除事件

void removefd(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

//重置EPOLLONESHOT事件
void modfd(int epollfd,int fd, int ev)//ev?
{

	epoll_event event;
	event.data.fd = fd;
	//ET模式
	event.events = ev|EPOLLIN | EPOLLET | EPOLLRDHUP;
	epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);

}

int http_con::m_user_count = 0;
int http_con::m_epollfd = -1;

void http_con::close_con(bool real_close)
{
	if (real_close && (m_sockfd != -1))
	{
		removefd(m_epollfd, m_sockfd);
		m_sockfd = -1;
		m_user_count--;


	}
}
void http_con::init(int sockfd, const sockaddr_in& addr)
{
	m_sockfd = sockfd;
	m_address = addr;

	bytes_have_send = 0;
	bytes_to_send = 0;
	//下面2行紧急为了避免time_wait状态，用于调试，实际去掉
	int reuse = 1;
	setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	addfd(m_epollfd, sockfd, true);
	printf("epoll_add %d\n", sockfd);
	m_user_count++;

	init();
}

void http_con::init()
{
	m_check_state = CHECK_STATE_REQUESTLINE;
	m_linger = false;

	m_method = GET;
	m_url = 0;
	m_version = 0;
	m_content_length = 0;
	m_host = 0;
	m_start_line = 0;
	m_checked_idx = 0;
	m_read_idx = 0;

	m_write_idx = 0;
	memset(m_read_buf, '\0', READ_BUFFER_SIZE);
	memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
	memset(m_real_file, '\0', FILENAME_LEN);

	printf("init() is arady\n");
}

//从状态机

/*从状态机负责读取buffer中的数据，将每行数据末尾的\r\n置为\0\0，并更新从状态机在buffer中读取的位置m_checked_idx，以此来驱动主状态机解析。

从状态机从m_read_buf中逐字节读取，判断当前字节是否为\r

接下来的字符是\n，将\r\n修改成\0\0，将m_checked_idx指向下一行的开头，则返回LINE_OK

接下来达到了buffer末尾，表示buffer还需要继续接收，返回LINE_OPEN

否则，表示语法错误，返回LINE_BAD

当前字节不是\r，判断是否是\n（一般是上次读取到\r就到了buffer末尾，没有接收完整，再次接收时会出现这种情况）

如果前一个字符是\r，则将\r\n修改成\0\0，将m_checked_idx指向下一行的开头，则返回LINE_OK

当前字节既不是\r，也不是\n

表示接收不完整，需要继续接收，返回LINE_OPEN*/

http_con::LINE_STATUS http_con::parse_line()
{
	printf("begin parse line\n");//调试

	char temp;
	for (; m_checked_idx < m_read_idx; ++m_checked_idx)
	{
		//temp为将要分析的字节
		temp = m_read_buf[m_checked_idx];

		if (temp == '\r')
		{
			//缓冲区中m_read_buf中数据的最后一个字节的下一个位置,m_read_idx,
			//这句话表示下一个字符达到了buffer结尾，则接收不完整，需要继续接收
			if ((m_checked_idx + 1) == m_read_idx)
			{
				return LINE_OPEN;//行没读完整，只有个\r
			}
			else if (m_read_buf[m_checked_idx + 1] == '\n')//读取完整一行，加上\0
			{

				printf("parse_line get 1 complete line rn\n");
				//下一个字符是\n，将\r\n改为\0\0，i++是先用，再加
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;

			}
			//如果都不符合，返回语法错误
			return LINE_BAD;
		}
		//如果当前字符是\n，也有可能读取到完整行
	  //一般是上次读取到\r就到buffer末尾了，没有接收完整，再次接收时会出现这种情况
		else if (temp == '\n')
		{
			if ((m_checked_idx > 1) && (m_read_buf[m_checked_idx - 1] == '\r'))
			{
				m_read_buf[m_checked_idx - 1] = '\0';
				m_read_buf[m_checked_idx++] = '\0';

				printf("parse_line  get 1 complete line nr \n");
				return LINE_OK;
			}
			return LINE_BAD;
		}
	}
	////并没有找到\r\n，需要继续接收

	return LINE_OPEN;
}


//循环读取客户端数据，直到无数据可读或者对方关闭连接
//读取浏览器端发送来的请求报文，直到无数据可读或对方关闭连接，读取到m_read_buffer中，并更新m_read_idx。

bool http_con::read()
{
	if (m_read_idx >= READ_BUFFER_SIZE)
	{
		return false;
	}
	int bytes_read = 0;
	printf("connfd %d begin to read %d\n",m_sockfd, m_read_idx);
	while (true)
	{
		////从套接字接收数据，存储在m_read_buf缓冲区
		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
		if (bytes_read == -1)
		{
			// 非阻塞ET模式下，需要一次性将数据读完
			if (errno == EAGAIN || errno == EWOULDBLOCK)//读缓冲区没有数据，跳出循环
			{
				break;
			}
			return false;//读取失败
		}
		else if (bytes_read == 0)
		{
			return false;//读到0，对方关闭连接
		}

		m_read_idx += bytes_read;
	}

	printf("connfd %d has read %d bytes\n", m_sockfd, m_read_idx);

	return true;
}

//解析http请求行,获得请求方法，目标url,以及http版本号

/*主状态机逻辑
主状态机初始状态是CHECK_STATE_REQUESTLINE，通过调用从状态机来驱动主状态机，在主状态机进行解析前，
从状态机已经将每一行的末尾\r\n符号改为\0\0，以便于主状态机直接取出对应字符串进行处理。

CHECK_STATE_REQUESTLINE

主状态机的初始状态，调用parse_request_line函数解析请求行

解析函数从m_read_buf中解析HTTP请求行，获得请求方法、目标URL及HTTP版本号

解析完成后主状态机的状态变为CHECK_STATE_HEADER*/


http_con::HTTP_CODE http_con::parse_request_line(char* text)

{
	//调试用
	printf("begin to parse request line\n");


	// //在HTTP报文中，请求行用来说明请求类型,要访问的资源以及所使用的HTTP版本，其中各个部分之间通过\t或空格分隔。
	//请求行中最先含有空格和\t任一字符的位置并返回

	m_url = strpbrk(text, " \t");//text中检索\t的数目，返回数目返回值】
	//如果s1、s2含有相同的字符，那么返回指向s1中第一个相同字符的指针，否则返回NULL。


	/*如果要查找多个字符，需要使用 strpbrk 函数。该函数在源字符串（s1）中按从前到后顺序找出最先含有搜索字符串（s2）中任一字符的位置并返回，空字符 null('\0') 不包括在内，若找不到则返回空指针。其函数原型的一般格式如下：
char *strpbrk(const char *s1,const char *s2);*/

	////如果没有空格或\t，则报文格式有误
	if (!m_url)
	{
		return BAD_REQUEST;
	}
	////将该位置改为\0，用于将前面数据取出
	*m_url++ = '\0';
	
	////取出数据，并通过与GET和POST比较，以确定请求方式
	char* method = text;
	if (strcasecmp(method, "GET") == 0)
	{
		m_method = GET;
		printf("method is %s\n", method);//调试用
	}
	else
	{
		return BAD_REQUEST;
	}
	/*//m_url此时跳过了第一个空格或\t字符，但不知道之后是否还有
 //将m_url向后偏移，通过查找，继续跳过空格和\t字符，指向请求资源的第一个字符*/
	m_url += strspn(m_url, " \t");
	/*strspn() 从参数 str 字符串的开头计算连续的字符，而这些字符都完全是 accept 
	所指字符串中的字符。简单的说，若 strspn() 返回的数值为n，则代表字符串 str 开头
	连续有 n 个字符都是属于字符串 accept 内的字符。*/


	////使用与判断请求方式的相同逻辑，判断HTTP版本号
	m_version = strpbrk(m_url, " \t");
	if (!m_version)
	{
		return BAD_REQUEST;
	}
	*m_version++ = '\0';
	//连续跳过url和http版本号中间的 \t,之前先加个\0，使得url成为一个字符串
	m_version += strspn(m_version, " \t");
	printf("parse_request_line:: m_version is %s\n", m_version);
	if (strcasecmp(m_version, "HTTP/1.1") != 0)
	{
		return BAD_REQUEST;

	}

	printf("parse_rrquest_line:: m_url is now::  %s\n", m_url);//调试


	/*//对请求资源前7个字符进行判断
    //这里主要是有些报文的请求资源中会带有http://，这里需要对这种情况进行单独处理*/
	if (strncasecmp(m_url, "http://", 7) == 0)
	{
		m_url+= 7;
		/*C 库函数 char *strchr(const char *str, int c) 在参数 str 所指向的字符串中搜索第一次出现
		字符 c（一个无符号字符）的位置。*/
		m_url= strchr(m_url, '/');
	}
	//同样增加https情况
	    if (strncasecmp(m_url, "https://", 8) == 0)
		    {
		        m_url += 8;
		        m_url = strchr(m_url, '/');
		    }

		//一般的不会带有上述两种符号，直接是单独的/或/后面带访问资源
	if (!m_url || m_url[0] != '/')
	{
		return BAD_REQUEST;
	}

	//当url为/时，显示欢迎界面
	if (strlen(m_url) == 1)
	{
		//strcat(m_url, "index.html");
		printf("parse_request_line:，begin to printf index.html::  %s\n", m_url);
	}
	  

	//请求行处理完毕，将主状态机转移处理请求头
	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}



//解析http 请求头

/*解析完请求行后，主状态机继续分析请求头。在报文中，请求头和空行的处理使用的同一个函数，
这里通过判断当前的text首位是不是\0字符，若是，则表示当前处理的是空行，若不是，则表示当前处理的是请求头。

CHECK_STATE_HEADER

调用parse_headers函数解析请求头部信息

判断是空行还是请求头，若是空行，进而判断content-length是否为0，如果不是0，表明是POST请求，
	则状态转移到CHECK_STATE_CONTENT，否则说明是GET请求，则报文解析结束。

若解析的是请求头部字段，则主要分析connection字段，content-length字段，其他字段可以直接跳过，
	各位也可以根据需求继续分析。

connection字段判断是keep-alive还是close，决定是长连接还是短连接

content-length字段，这里用于读取post请求的消息体长度*/


http_con::HTTP_CODE http_con::parse_header(char* text)
{
	printf("parse header 调试\n");
	//  读取到空行 说明头部解析完毕
	if (text[0] == '\0')
	{
		// 如果有消息体 则需要解析消息体
		if (m_content_length != 0)
		{
			////POST需要跳转到消息体处理状态
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;//已经得到一个完整的http请求
	}

	
	//解析请求头部连接字段
	else if (strncasecmp(text, "Connection:", 11) == 0)
	{
		text += 11;
		//跳过空格和\t字符
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-alive") == 0)
		{
			//如果是长连接，则将linger标志设置为true
			m_linger = true;
		}
	}
	//解析请求头部内容长度字段
	else if (strncasecmp(text, "Content-Length:", 15) == 0)
	{
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text);
	}
	//解析请求头部HOST字段
	else if (strncasecmp(text, "Host:", 5) == 0)
	{
		text += 5;
		text += strspn(text, " \t");
		m_host= text;
	}
	else
	{
		printf("oop! unknow header %s\n", text);
	}
	return NO_REQUEST;
}

//没有真的解析消息体，只是判断是否被完整的读入
http_con::HTTP_CODE http_con::parse_content(char* text)
{
	printf("parse content 调试\n");
	//判断buffer中是否读取了消息体
	if (m_read_idx >= (m_content_length + m_checked_idx))
	{
		text[m_content_length] = '\0';
		return GET_REQUEST;
	}
	return NO_REQUEST;
}


//主状态机





http_con::HTTP_CODE http_con::process_read()
{
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = 0;

	printf("begin process_read\n");

	//判断条件里面，前面涉及解析请求行或者请求体，parse_line()是从状态机的具体实现

	while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK)||((line_status=parse_line())==LINE_OK))
	{
		//从缓冲区读取一行放在text，并且缓冲区标记读了一行
		text = get_line();
		
		m_start_line = m_checked_idx;
		//m_start_line是每一个数据行在m_read_buf中的起始位置
      //m_checked_idx表示从状态机在m_read_buf中读取的位置
		printf("got 1 http line: %s\n", text);//调试用

		printf("process_read begin 主状态机\n");//调试
		//主状态机的三种逻辑转移
		switch (m_check_state)
		{

		case CHECK_STATE_REQUESTLINE:
		{
			//解析请求行
			ret = parse_request_line(text);
			if (ret == BAD_REQUEST)
			{
				return BAD_REQUEST;
			}
			break;
		}
		case CHECK_STATE_HEADER:
		{
			//解析请求头
			ret = parse_header(text);
			{
				if (ret == BAD_REQUEST)
				{
					return BAD_REQUEST;
				}
				// 完整解析GET请求后，跳转到报文响应函数
				else if (ret == GET_REQUEST)
				{
					return do_request();
				}
				break;
			}
		}

		case CHECK_STATE_CONTENT:
		{
			//解析消息体
			ret = parse_content(text);

			// 完整解析POST请求后，跳转到报文响应函数
			if (ret == GET_REQUEST)
			{
				return do_request();
			}
			//解析完消息体即完成报文解析，避免再次进入循环，更新line_status
			line_status = LINE_OPEN;
			break;
		}

		default:
		{
			return INTERBAL_ERRPOR;
		}

		}
	}
	//请求不完整，需要继续读取请求报文
	return NO_REQUEST;

}



//当得到一个完整，正确的http骑牛，就分析目标文佳的属性。如果文件存在且对所有用户可读，
//不是目录，就mmap映射到m_file_address那里，并且告诉调用者获取文件成功

http_con::HTTP_CODE http_con::do_request()
{

	printf("begin to do_request:: 1\n");
	//将初始化的m_real_file赋值为网站根目录
	strcpy(m_real_file, doc_root);
	int len = strlen(doc_root);


	printf("m-real-file IS :: %s\n", m_real_file);//调试

	/*直接将url与网站目录拼接
      //这里的情况是welcome界面，请求服务器上的一个图片
     strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);*/
	strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

	printf("m_real_file IS(2):: %s\n", m_real_file);
	//通过stat获取请求资源文件信息，成功则将信息更新到m_file_stat结构体
   //失败返回NO_RESOURCE状态，表示资源不存在
	if (stat(m_real_file, &m_file_stat) < 0)
	{
		printf("do_request,file result is :: no_resource\n");
		return NO_RESOURCE;
	}
	///判断文件的权限，是否可读，不可读则返回FORBIDDEN_REQUEST状态
	if (!(m_file_stat.st_mode & S_IROTH))
	{
		printf("do_request,file result is :: file_forbidden\n");
		return FORBIDDEN_REQUEST;
	}
	// 判断文件类型，如果是目录，则返回BAD_REQUEST，表示请求报文有误
	if (S_ISDIR(m_file_stat.st_mode))
	{
		printf("do_request,file result is :: is_dir\n");
		return BAD_REQUEST;

	}
	//以只读方式获取文件描述符，通过mmap将该文件映射到内存中
	int fd = open(m_real_file, O_RDONLY);
	m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	printf("this file is ready to BE READ::\n");
	////避免文件描述符的浪费和占用
	close(fd);
    //表示请求文件存在，且可以访问
	return FILE_REQUEST;
}

void http_con::unmap()
{
	if (m_file_address)
	{
		munmap(m_file_address, m_file_stat.st_size);
		m_file_address = nullptr;
	}
}

//写http响应
/*服务器子线程调用process_write完成响应报文，随后注册epollout事件。
服务器主线程检测写事件，并调用http_conn::write函数将响应报文发送给浏览器端。*/

bool http_con::write()
{
	int temp = 0;
	int bytes_have_send = 0;
	//int bytes_to_send = m_write_idx;
	int newadd = 0;
	//如果要发送的数据长度为0，表示响应报文是空的，一般不会出现这种情况
	if (bytes_to_send == 0)
	{
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		init();
		return true;
	}

	while (1)
	{
		//发送相应报文给浏览器，这个函数是主线程调用的
		temp = writev(m_sockfd, m_iv, m_iv_count);

		if (temp > 0)
		{
			//更新发送字节
			bytes_have_send += temp;
			//偏移文件指针
			newadd = bytes_have_send - m_write_idx;
		}
		
		else if (temp <= -1)
		{
			//判断是不是缓冲区满了
			if (errno == EAGAIN)
			{
				//第一个iovec中的数据已经发送完，发送第二个
				if (bytes_have_send >= m_iv[0].iov_len)
				{
					m_iv[0].iov_len = 0;
					m_iv[1].iov_len = bytes_to_send;
					m_iv[1].iov_base = m_file_address+newadd;
				}
				//
				else//继续发送第一个iovec头部的信息
				{
					m_iv[0].iov_base = m_write_buf + bytes_to_send;
					m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
				}
				//重新注册写事件
				modfd(m_epollfd, m_sockfd, EPOLLOUT);
				return true;
			}
			//如果发送失败，但不是缓冲区问题，取消映射
			unmap();
			return false;
		}
		bytes_to_send -= temp;
		//bytes_have_send += temp;
		//判断条件，数据已经发送完
		if (bytes_to_send <=0)
		{
			//发送http响应成功，根据http请求中的connection字段决定是否立即关闭连接
			unmap();
			//在epoll树上重新注册epoll_oneshot事件
			modfd(m_epollfd, m_sockfd, EPOLLIN);
			//如果浏览器请求长连接
			if (m_linger)
			{
				init();//重新初始化为http对象
				//modfd(m_epollfd,m_sockfd, EPOLLIN);
				return true;
			}
			else
			{
				//modfd(m_epollfd, m_sockfd, EPOLLIN);
				return false;
			}
		}

	}

}

//往写缓冲区写入待发送的数据

bool http_con::add_response(const char* format, ...)
{
	//如果写入内容超出m_write_buf大小则报错
	//
	if (m_write_idx >= WRITE_BUFFER_SIZE)
	{
		return false;
	}
	////定义可变参数列表
	va_list arg_list;
	//将变量arg_list初始化为传入参数
	va_start(arg_list, format);

	//将数据format从可变参数列表写入缓冲区写，返回写入数据的长度
	int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

	//如果写入的数据长度超过缓冲区剩余空间，则报错
	if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
	{
		return false;
	}
	//更新m_write_idx位置
	m_write_idx += len;
	//清空可变参列表
	va_end(arg_list);
	return true;

}



bool http_con::add_content(const char* content)
{
	return add_response("%s", content);
}
//添加状态行
bool http_con::add_status_line(int status, const char* title)
{
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
//添加消息报头，具体的添加文本长度、连接状态和空行

bool http_con::add_headers(int content_length)
{
	add_content_length(content_length);
	add_linger();
	add_blank_line();
}
//添加Content-Length，表示响应报文的长度
bool http_con::add_content_length(int content_length)
{
	return add_response("Content-Length: %d\r\n", content_length);
}

bool http_con::add_linger() 
{
	return add_response("Connection: %s\r\n", (m_linger==true) ? "keep-alive" : "close");
}

bool http_con::add_blank_line()
{
	return add_response("%s", "\r\n");
}



//根据服务器处理http请求的结果，决定返回给客户端的内容

/*process_write
根据do_request的返回状态，服务器子线程调用process_write向m_write_buf中写入响应报文。

add_status_line函数，添加状态行：http/1.1 状态码 状态消息

add_headers函数添加消息报头，内部调用add_content_length和add_linger函数

content-length记录响应报文长度，用于浏览器端判断服务器是否发送完数据

connection记录连接状态，用于告诉浏览器端保持长连接

add_blank_line添加空行

上述涉及的5个函数，均是内部调用add_response函数更新m_write_idx指针和缓冲区m_write_buf中的内容。*/

bool http_con::process_write(HTTP_CODE ret)
{

	printf("begin process_write ,to m_write_buf 调试\n");
	switch (ret)
	{
	////内部错误，500
	case INTERBAL_ERRPOR:
	{
		add_status_line(500, ERROR_500_TITLE);
		add_headers(strlen(ERROR_500_FORM));
		if (!add_content(ERROR_500_FORM))
		{
			return false;
		}
		break;

	}
	//报文语法有误，400
	case BAD_REQUEST:
	{
		add_status_line(400, ERROR_400_TITILE);
		add_headers(strlen(ERROR_400_FORM));
		if (!add_content(ERROR_400_FORM))
		{
			return false;
		}
		break;

	}
	//404
	case NO_RESOURCE:
	{
		add_status_line(404, ERROR_404_TITLE);
		add_headers(strlen(ERROR_404_FORM));
		if (!add_content(ERROR_404_FORM))
		{
			return false;
		}
		break;

	}
	//资源没有访问权限，403
	case FORBIDDEN_REQUEST:
	{
		add_status_line(403, ERROR_403_TITLE);
		add_headers(strlen(ERROR_403_FORM));
		if (!add_content(ERROR_403_FORM))
		{
			return false;
		}
		break;

	}
	//文件存在，200
	case FILE_REQUEST:
	{
		add_status_line(200, OK_200_TITILE);
		//如果请求的资源存在
		if (m_file_stat.st_size != 0)
		{
			add_headers(m_file_stat.st_size);
			//第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
			m_iv[0].iov_base = m_write_buf;
			m_iv[0].iov_len = m_write_idx;
			////第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
			m_iv[1].iov_base = m_file_address;
			m_iv[1].iov_len = m_file_stat.st_size;
			m_iv_count = 2;

			//发送的全部数据为响应报头加上文件大小
			bytes_to_send = m_write_idx + m_file_stat.st_size;
			return true;
		}
		else
		{
			//如果请求的资源大小为0，则返回空白html文件

			const char* ok_string = "<html><body></body></html>";
			add_headers(strlen(ok_string));
			if (!add_content(ok_string))
			{
				return false;
			}
		}

	}
	default:
	
		return false;

	}
	//除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区
	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	return true;
}

//线程池中工作的线程调用,处理http请求的入口
void http_con::process()
{
	printf("begin process\n");
	//先从m_read_buf读取并分析，完成请求解析
	HTTP_CODE read_ret = process_read();

	//NO_REQUEST，请求不完整，需要继续接收请求数据
	if (read_ret == NO_REQUEST)
	{
		printf("process_no_request\n");
		//注册并监听读事件
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		return;
	}

	//调用process_write()完成报文响应，写入m_write_buf缓冲区
	bool write_ret = process_write(read_ret);


	if (!write_ret)
	{
		close_con();
	}
 //注册并监听写事件，等待可以写并把缓冲区数据写入socket

	modfd(m_epollfd, m_sockfd, EPOLLOUT);
}