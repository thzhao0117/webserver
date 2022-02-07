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
	//���ö�ȡ�ļ�������
	static const int FILENAME_LEN=200;//�ļ�������󳤶�

	static const int READ_BUFFER_SIZE = 2048;
	static const int WRITE_BUFFER_SIZE = 1024;
	//���ĵ����󷽷�
	enum METHOD
	{
		GET = 0,POST,HEAD,PUT,DELLATE//Ŀǰ����֧��get
	};

	enum CHECK_STATE//��״̬��״̬
	{
		CHECK_STATE_REQUESTLINE = 0,
		CHECK_STATE_HEADER,
		CHECK_STATE_CONTENT
	};

	enum HTTP_CODE//���Ľ����Ľ��
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
	http_con() {}//���캯������������
	~http_con() {}

public:

	//��ʼ�������µ�����
	void init(int sockfd, const sockaddr_in& addr);
	void close_con(bool real_close = true);
	//����ͻ�����
	void process();
	//��ȡ�ͻ��˷�����ȫ������
	bool read();
	//��Ӧ����д�뺯��
	bool write();
	sockaddr_in* get_address()
	{
		return &m_address;
	}

private:
	void init();
	//��m_read_buf��ȡ��������������
	HTTP_CODE process_read();
	//��m_write_bufд����Ӧ��������
	bool process_write(HTTP_CODE ret);

	//process_read���÷���http����

	//��״̬�����������������е�����
	HTTP_CODE parse_request_line(char* text);
	//��״̬�����������е�����ͷ����
	HTTP_CODE parse_header(char* text);
	//��״̬�����������е���������
	HTTP_CODE parse_content(char* text);
	//������Ӧ����
	HTTP_CODE do_request();



	//m_start_line���Ѿ��������ַ�
   //get_line���ڽ�ָ�����ƫ�ƣ�ָ��δ������ַ�
	char* get_line()
	{
		return m_read_buf + m_start_line;
	
	}
	//��״̬����ȡһ�У������������ĵ���һ����
	LINE_STATUS parse_line();

	//��һ�麯����process_write()�������Ӧ��
	void unmap();
	//������Ӧ���ĸ�ʽ�����ɶ�Ӧ8�����֣����º�������do_request����
	bool add_response(const char* format, ...);
	bool add_content(const char* content);
	bool add_status_line(int status, const char* title);
	bool add_headers(int content_length);
	bool add_content_length(int content_length);
	bool add_linger();
	bool add_blank_line();

public:
	static int m_epollfd;
	//ͳ���û�����
	static int m_user_count;

private:
	int m_sockfd;
	sockaddr_in m_address;

	//��������
	//�洢��ȡ������������
	char m_read_buf[READ_BUFFER_SIZE];
	//������m_read_buf�У���������m_read_buf�����ݵ����һ���ֽڵ���һ��λ��
	int m_read_idx;
	//m_read_buf��ȡ��λ��m_checked_idx
	int m_checked_idx;
	//��ǰ���ڽ������е���ʼλ��
	///m_read_buf���Ѿ��������ַ�����
	int m_start_line;

	//д������
	char m_write_buf[WRITE_BUFFER_SIZE];
	char m_write_idx;

	//��״̬����ǰ״̬
	CHECK_STATE m_check_state;
	//���󷽷�
	METHOD m_method;

	//����Ϊ�����������ж�Ӧ��6������

	//�ͻ������Ŀ���ļ�������·��,���ݵ���doc_root+m_url,doc_root����վ��Ŀ¼

	char m_real_file[FILENAME_LEN];
	
	char* m_url;
	char* m_version;
	char* m_host;
	int m_content_length;
	bool m_linger;//keep_alive,�Ƿ񱣳����ӣ������Ӳ���

	//�ͻ������Ŀ���ļ���mmap���ڴ��е�λ��
	//��ȡ�������ϵ��ļ���ַ
	char* m_file_address;
	//Ŀ���ļ�״̬��
	struct stat m_file_stat;
	//writeV��д��
	struct iovec m_iv[2];
	int m_iv_count;//��д�ڴ�����Ŀ
	int bytes_to_send;
	int bytes_have_send;
	

};

#endif // !HTTP_CON

