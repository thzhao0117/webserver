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

// website roor dir��վ��Ŀ¼���ļ����ڴ���������Դ����ת��html�ļ�
//
const char* const doc_root = "/home/abbac/projects/webserv/bin/x64/Debug";

//���ļ�����������Ϊ����������Ϊ��epoll�ı�Ե����
int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	int new_option = old_option | O_NONBLOCK;
	fcntl(fd, F_SETFL, new_option);
	return old_option;
}

///�ں��¼���ע�����¼�������EPOLLONESHOT����Կͻ������ӵ���������listenfd���ÿ���


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
	setnonblocking(fd);//���ں�ע��fd�������÷�����
}

//�ں��¼���ɾ���¼�

void removefd(int epollfd, int fd)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
	close(fd);
}

//����EPOLLONESHOT�¼�
void modfd(int epollfd,int fd, int ev)//ev?
{

	epoll_event event;
	event.data.fd = fd;
	//ETģʽ
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
	//����2�н���Ϊ�˱���time_wait״̬�����ڵ��ԣ�ʵ��ȥ��
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

//��״̬��

/*��״̬�������ȡbuffer�е����ݣ���ÿ������ĩβ��\r\n��Ϊ\0\0�������´�״̬����buffer�ж�ȡ��λ��m_checked_idx���Դ���������״̬��������

��״̬����m_read_buf�����ֽڶ�ȡ���жϵ�ǰ�ֽ��Ƿ�Ϊ\r

���������ַ���\n����\r\n�޸ĳ�\0\0����m_checked_idxָ����һ�еĿ�ͷ���򷵻�LINE_OK

�������ﵽ��bufferĩβ����ʾbuffer����Ҫ�������գ�����LINE_OPEN

���򣬱�ʾ�﷨���󣬷���LINE_BAD

��ǰ�ֽڲ���\r���ж��Ƿ���\n��һ�����ϴζ�ȡ��\r�͵���bufferĩβ��û�н����������ٴν���ʱ��������������

���ǰһ���ַ���\r����\r\n�޸ĳ�\0\0����m_checked_idxָ����һ�еĿ�ͷ���򷵻�LINE_OK

��ǰ�ֽڼȲ���\r��Ҳ����\n

��ʾ���ղ���������Ҫ�������գ�����LINE_OPEN*/

http_con::LINE_STATUS http_con::parse_line()
{
	printf("begin parse line\n");//����

	char temp;
	for (; m_checked_idx < m_read_idx; ++m_checked_idx)
	{
		//tempΪ��Ҫ�������ֽ�
		temp = m_read_buf[m_checked_idx];

		if (temp == '\r')
		{
			//��������m_read_buf�����ݵ����һ���ֽڵ���һ��λ��,m_read_idx,
			//��仰��ʾ��һ���ַ��ﵽ��buffer��β������ղ���������Ҫ��������
			if ((m_checked_idx + 1) == m_read_idx)
			{
				return LINE_OPEN;//��û��������ֻ�и�\r
			}
			else if (m_read_buf[m_checked_idx + 1] == '\n')//��ȡ����һ�У�����\0
			{

				printf("parse_line get 1 complete line rn\n");
				//��һ���ַ���\n����\r\n��Ϊ\0\0��i++�����ã��ټ�
				m_read_buf[m_checked_idx++] = '\0';
				m_read_buf[m_checked_idx++] = '\0';
				return LINE_OK;

			}
			//����������ϣ������﷨����
			return LINE_BAD;
		}
		//�����ǰ�ַ���\n��Ҳ�п��ܶ�ȡ��������
	  //һ�����ϴζ�ȡ��\r�͵�bufferĩβ�ˣ�û�н����������ٴν���ʱ������������
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
	////��û���ҵ�\r\n����Ҫ��������

	return LINE_OPEN;
}


//ѭ����ȡ�ͻ������ݣ�ֱ�������ݿɶ����߶Է��ر�����
//��ȡ������˷������������ģ�ֱ�������ݿɶ���Է��ر����ӣ���ȡ��m_read_buffer�У�������m_read_idx��

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
		////���׽��ֽ������ݣ��洢��m_read_buf������
		bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
		if (bytes_read == -1)
		{
			// ������ETģʽ�£���Ҫһ���Խ����ݶ���
			if (errno == EAGAIN || errno == EWOULDBLOCK)//��������û�����ݣ�����ѭ��
			{
				break;
			}
			return false;//��ȡʧ��
		}
		else if (bytes_read == 0)
		{
			return false;//����0���Է��ر�����
		}

		m_read_idx += bytes_read;
	}

	printf("connfd %d has read %d bytes\n", m_sockfd, m_read_idx);

	return true;
}

//����http������,������󷽷���Ŀ��url,�Լ�http�汾��

/*��״̬���߼�
��״̬����ʼ״̬��CHECK_STATE_REQUESTLINE��ͨ�����ô�״̬����������״̬��������״̬�����н���ǰ��
��״̬���Ѿ���ÿһ�е�ĩβ\r\n���Ÿ�Ϊ\0\0���Ա�����״̬��ֱ��ȡ����Ӧ�ַ������д���

CHECK_STATE_REQUESTLINE

��״̬���ĳ�ʼ״̬������parse_request_line��������������

����������m_read_buf�н���HTTP�����У�������󷽷���Ŀ��URL��HTTP�汾��

������ɺ���״̬����״̬��ΪCHECK_STATE_HEADER*/


http_con::HTTP_CODE http_con::parse_request_line(char* text)

{
	//������
	printf("begin to parse request line\n");


	// //��HTTP�����У�����������˵����������,Ҫ���ʵ���Դ�Լ���ʹ�õ�HTTP�汾�����и�������֮��ͨ��\t��ո�ָ���
	//�����������Ⱥ��пո��\t��һ�ַ���λ�ò�����

	m_url = strpbrk(text, " \t");//text�м���\t����Ŀ��������Ŀ����ֵ��
	//���s1��s2������ͬ���ַ�����ô����ָ��s1�е�һ����ͬ�ַ���ָ�룬���򷵻�NULL��


	/*���Ҫ���Ҷ���ַ�����Ҫʹ�� strpbrk �������ú�����Դ�ַ�����s1���а���ǰ����˳���ҳ����Ⱥ��������ַ�����s2������һ�ַ���λ�ò����أ����ַ� null('\0') ���������ڣ����Ҳ����򷵻ؿ�ָ�롣�亯��ԭ�͵�һ���ʽ���£�
char *strpbrk(const char *s1,const char *s2);*/

	////���û�пո��\t�����ĸ�ʽ����
	if (!m_url)
	{
		return BAD_REQUEST;
	}
	////����λ�ø�Ϊ\0�����ڽ�ǰ������ȡ��
	*m_url++ = '\0';
	
	////ȡ�����ݣ���ͨ����GET��POST�Ƚϣ���ȷ������ʽ
	char* method = text;
	if (strcasecmp(method, "GET") == 0)
	{
		m_method = GET;
		printf("method is %s\n", method);//������
	}
	else
	{
		return BAD_REQUEST;
	}
	/*//m_url��ʱ�����˵�һ���ո��\t�ַ�������֪��֮���Ƿ���
 //��m_url���ƫ�ƣ�ͨ�����ң����������ո��\t�ַ���ָ��������Դ�ĵ�һ���ַ�*/
	m_url += strspn(m_url, " \t");
	/*strspn() �Ӳ��� str �ַ����Ŀ�ͷ�����������ַ�������Щ�ַ�����ȫ�� accept 
	��ָ�ַ����е��ַ����򵥵�˵���� strspn() ���ص���ֵΪn��������ַ��� str ��ͷ
	������ n ���ַ����������ַ��� accept �ڵ��ַ���*/


	////ʹ�����ж�����ʽ����ͬ�߼����ж�HTTP�汾��
	m_version = strpbrk(m_url, " \t");
	if (!m_version)
	{
		return BAD_REQUEST;
	}
	*m_version++ = '\0';
	//��������url��http�汾���м�� \t,֮ǰ�ȼӸ�\0��ʹ��url��Ϊһ���ַ���
	m_version += strspn(m_version, " \t");
	printf("parse_request_line:: m_version is %s\n", m_version);
	if (strcasecmp(m_version, "HTTP/1.1") != 0)
	{
		return BAD_REQUEST;

	}

	printf("parse_rrquest_line:: m_url is now::  %s\n", m_url);//����


	/*//��������Դǰ7���ַ������ж�
    //������Ҫ����Щ���ĵ�������Դ�л����http://��������Ҫ������������е�������*/
	if (strncasecmp(m_url, "http://", 7) == 0)
	{
		m_url+= 7;
		/*C �⺯�� char *strchr(const char *str, int c) �ڲ��� str ��ָ����ַ�����������һ�γ���
		�ַ� c��һ���޷����ַ�����λ�á�*/
		m_url= strchr(m_url, '/');
	}
	//ͬ������https���
	    if (strncasecmp(m_url, "https://", 8) == 0)
		    {
		        m_url += 8;
		        m_url = strchr(m_url, '/');
		    }

		//һ��Ĳ�������������ַ��ţ�ֱ���ǵ�����/��/�����������Դ
	if (!m_url || m_url[0] != '/')
	{
		return BAD_REQUEST;
	}

	//��urlΪ/ʱ����ʾ��ӭ����
	if (strlen(m_url) == 1)
	{
		//strcat(m_url, "index.html");
		printf("parse_request_line:��begin to printf index.html::  %s\n", m_url);
	}
	  

	//�����д�����ϣ�����״̬��ת�ƴ�������ͷ
	m_check_state = CHECK_STATE_HEADER;
	return NO_REQUEST;
}



//����http ����ͷ

/*�����������к���״̬��������������ͷ���ڱ����У�����ͷ�Ϳ��еĴ���ʹ�õ�ͬһ��������
����ͨ���жϵ�ǰ��text��λ�ǲ���\0�ַ������ǣ����ʾ��ǰ������ǿ��У������ǣ����ʾ��ǰ�����������ͷ��

CHECK_STATE_HEADER

����parse_headers������������ͷ����Ϣ

�ж��ǿ��л�������ͷ�����ǿ��У������ж�content-length�Ƿ�Ϊ0���������0��������POST����
	��״̬ת�Ƶ�CHECK_STATE_CONTENT������˵����GET�������Ľ���������

��������������ͷ���ֶΣ�����Ҫ����connection�ֶΣ�content-length�ֶΣ������ֶο���ֱ��������
	��λҲ���Ը����������������

connection�ֶ��ж���keep-alive����close�������ǳ����ӻ��Ƕ�����

content-length�ֶΣ��������ڶ�ȡpost�������Ϣ�峤��*/


http_con::HTTP_CODE http_con::parse_header(char* text)
{
	printf("parse header ����\n");
	//  ��ȡ������ ˵��ͷ���������
	if (text[0] == '\0')
	{
		// �������Ϣ�� ����Ҫ������Ϣ��
		if (m_content_length != 0)
		{
			////POST��Ҫ��ת����Ϣ�崦��״̬
			m_check_state = CHECK_STATE_CONTENT;
			return NO_REQUEST;
		}
		return GET_REQUEST;//�Ѿ��õ�һ��������http����
	}

	
	//��������ͷ�������ֶ�
	else if (strncasecmp(text, "Connection:", 11) == 0)
	{
		text += 11;
		//�����ո��\t�ַ�
		text += strspn(text, " \t");
		if (strcasecmp(text, "keep-alive") == 0)
		{
			//����ǳ����ӣ���linger��־����Ϊtrue
			m_linger = true;
		}
	}
	//��������ͷ�����ݳ����ֶ�
	else if (strncasecmp(text, "Content-Length:", 15) == 0)
	{
		text += 15;
		text += strspn(text, " \t");
		m_content_length = atol(text);
	}
	//��������ͷ��HOST�ֶ�
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

//û����Ľ�����Ϣ�壬ֻ���ж��Ƿ������Ķ���
http_con::HTTP_CODE http_con::parse_content(char* text)
{
	printf("parse content ����\n");
	//�ж�buffer���Ƿ��ȡ����Ϣ��
	if (m_read_idx >= (m_content_length + m_checked_idx))
	{
		text[m_content_length] = '\0';
		return GET_REQUEST;
	}
	return NO_REQUEST;
}


//��״̬��





http_con::HTTP_CODE http_con::process_read()
{
	LINE_STATUS line_status = LINE_OK;
	HTTP_CODE ret = NO_REQUEST;
	char* text = 0;

	printf("begin process_read\n");

	//�ж��������棬ǰ���漰���������л��������壬parse_line()�Ǵ�״̬���ľ���ʵ��

	while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK)||((line_status=parse_line())==LINE_OK))
	{
		//�ӻ�������ȡһ�з���text�����һ�������Ƕ���һ��
		text = get_line();
		
		m_start_line = m_checked_idx;
		//m_start_line��ÿһ����������m_read_buf�е���ʼλ��
      //m_checked_idx��ʾ��״̬����m_read_buf�ж�ȡ��λ��
		printf("got 1 http line: %s\n", text);//������

		printf("process_read begin ��״̬��\n");//����
		//��״̬���������߼�ת��
		switch (m_check_state)
		{

		case CHECK_STATE_REQUESTLINE:
		{
			//����������
			ret = parse_request_line(text);
			if (ret == BAD_REQUEST)
			{
				return BAD_REQUEST;
			}
			break;
		}
		case CHECK_STATE_HEADER:
		{
			//��������ͷ
			ret = parse_header(text);
			{
				if (ret == BAD_REQUEST)
				{
					return BAD_REQUEST;
				}
				// ��������GET�������ת��������Ӧ����
				else if (ret == GET_REQUEST)
				{
					return do_request();
				}
				break;
			}
		}

		case CHECK_STATE_CONTENT:
		{
			//������Ϣ��
			ret = parse_content(text);

			// ��������POST�������ת��������Ӧ����
			if (ret == GET_REQUEST)
			{
				return do_request();
			}
			//��������Ϣ�弴��ɱ��Ľ����������ٴν���ѭ��������line_status
			line_status = LINE_OPEN;
			break;
		}

		default:
		{
			return INTERBAL_ERRPOR;
		}

		}
	}
	//������������Ҫ������ȡ������
	return NO_REQUEST;

}



//���õ�һ����������ȷ��http��ţ���ͷ���Ŀ���ļѵ����ԡ�����ļ������Ҷ������û��ɶ���
//����Ŀ¼����mmapӳ�䵽m_file_address������Ҹ��ߵ����߻�ȡ�ļ��ɹ�

http_con::HTTP_CODE http_con::do_request()
{

	printf("begin to do_request:: 1\n");
	//����ʼ����m_real_file��ֵΪ��վ��Ŀ¼
	strcpy(m_real_file, doc_root);
	int len = strlen(doc_root);


	printf("m-real-file IS :: %s\n", m_real_file);//����

	/*ֱ�ӽ�url����վĿ¼ƴ��
      //����������welcome���棬����������ϵ�һ��ͼƬ
     strncpy(m_real_file+len,m_url,FILENAME_LEN-len-1);*/
	strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

	printf("m_real_file IS(2):: %s\n", m_real_file);
	//ͨ��stat��ȡ������Դ�ļ���Ϣ���ɹ�����Ϣ���µ�m_file_stat�ṹ��
   //ʧ�ܷ���NO_RESOURCE״̬����ʾ��Դ������
	if (stat(m_real_file, &m_file_stat) < 0)
	{
		printf("do_request,file result is :: no_resource\n");
		return NO_RESOURCE;
	}
	///�ж��ļ���Ȩ�ޣ��Ƿ�ɶ������ɶ��򷵻�FORBIDDEN_REQUEST״̬
	if (!(m_file_stat.st_mode & S_IROTH))
	{
		printf("do_request,file result is :: file_forbidden\n");
		return FORBIDDEN_REQUEST;
	}
	// �ж��ļ����ͣ������Ŀ¼���򷵻�BAD_REQUEST����ʾ����������
	if (S_ISDIR(m_file_stat.st_mode))
	{
		printf("do_request,file result is :: is_dir\n");
		return BAD_REQUEST;

	}
	//��ֻ����ʽ��ȡ�ļ���������ͨ��mmap�����ļ�ӳ�䵽�ڴ���
	int fd = open(m_real_file, O_RDONLY);
	m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	printf("this file is ready to BE READ::\n");
	////�����ļ����������˷Ѻ�ռ��
	close(fd);
    //��ʾ�����ļ����ڣ��ҿ��Է���
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

//дhttp��Ӧ
/*���������̵߳���process_write�����Ӧ���ģ����ע��epollout�¼���
���������̼߳��д�¼���������http_conn::write��������Ӧ���ķ��͸�������ˡ�*/

bool http_con::write()
{
	int temp = 0;
	int bytes_have_send = 0;
	//int bytes_to_send = m_write_idx;
	int newadd = 0;
	//���Ҫ���͵����ݳ���Ϊ0����ʾ��Ӧ�����ǿյģ�һ�㲻������������
	if (bytes_to_send == 0)
	{
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		init();
		return true;
	}

	while (1)
	{
		//������Ӧ���ĸ��������������������̵߳��õ�
		temp = writev(m_sockfd, m_iv, m_iv_count);

		if (temp > 0)
		{
			//���·����ֽ�
			bytes_have_send += temp;
			//ƫ���ļ�ָ��
			newadd = bytes_have_send - m_write_idx;
		}
		
		else if (temp <= -1)
		{
			//�ж��ǲ��ǻ���������
			if (errno == EAGAIN)
			{
				//��һ��iovec�е������Ѿ������꣬���͵ڶ���
				if (bytes_have_send >= m_iv[0].iov_len)
				{
					m_iv[0].iov_len = 0;
					m_iv[1].iov_len = bytes_to_send;
					m_iv[1].iov_base = m_file_address+newadd;
				}
				//
				else//�������͵�һ��iovecͷ������Ϣ
				{
					m_iv[0].iov_base = m_write_buf + bytes_to_send;
					m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
				}
				//����ע��д�¼�
				modfd(m_epollfd, m_sockfd, EPOLLOUT);
				return true;
			}
			//�������ʧ�ܣ������ǻ��������⣬ȡ��ӳ��
			unmap();
			return false;
		}
		bytes_to_send -= temp;
		//bytes_have_send += temp;
		//�ж������������Ѿ�������
		if (bytes_to_send <=0)
		{
			//����http��Ӧ�ɹ�������http�����е�connection�ֶξ����Ƿ������ر�����
			unmap();
			//��epoll��������ע��epoll_oneshot�¼�
			modfd(m_epollfd, m_sockfd, EPOLLIN);
			//����������������
			if (m_linger)
			{
				init();//���³�ʼ��Ϊhttp����
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

//��д������д������͵�����

bool http_con::add_response(const char* format, ...)
{
	//���д�����ݳ���m_write_buf��С�򱨴�
	//
	if (m_write_idx >= WRITE_BUFFER_SIZE)
	{
		return false;
	}
	////����ɱ�����б�
	va_list arg_list;
	//������arg_list��ʼ��Ϊ�������
	va_start(arg_list, format);

	//������format�ӿɱ�����б�д�뻺����д������д�����ݵĳ���
	int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);

	//���д������ݳ��ȳ���������ʣ��ռ䣬�򱨴�
	if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
	{
		return false;
	}
	//����m_write_idxλ��
	m_write_idx += len;
	//��տɱ���б�
	va_end(arg_list);
	return true;

}



bool http_con::add_content(const char* content)
{
	return add_response("%s", content);
}
//���״̬��
bool http_con::add_status_line(int status, const char* title)
{
	return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
//�����Ϣ��ͷ�����������ı����ȡ�����״̬�Ϳ���

bool http_con::add_headers(int content_length)
{
	add_content_length(content_length);
	add_linger();
	add_blank_line();
}
//���Content-Length����ʾ��Ӧ���ĵĳ���
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



//���ݷ���������http����Ľ�����������ظ��ͻ��˵�����

/*process_write
����do_request�ķ���״̬�����������̵߳���process_write��m_write_buf��д����Ӧ���ġ�

add_status_line���������״̬�У�http/1.1 ״̬�� ״̬��Ϣ

add_headers���������Ϣ��ͷ���ڲ�����add_content_length��add_linger����

content-length��¼��Ӧ���ĳ��ȣ�������������жϷ������Ƿ���������

connection��¼����״̬�����ڸ���������˱��ֳ�����

add_blank_line��ӿ���

�����漰��5�������������ڲ�����add_response��������m_write_idxָ��ͻ�����m_write_buf�е����ݡ�*/

bool http_con::process_write(HTTP_CODE ret)
{

	printf("begin process_write ,to m_write_buf ����\n");
	switch (ret)
	{
	////�ڲ�����500
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
	//�����﷨����400
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
	//��Դû�з���Ȩ�ޣ�403
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
	//�ļ����ڣ�200
	case FILE_REQUEST:
	{
		add_status_line(200, OK_200_TITILE);
		//����������Դ����
		if (m_file_stat.st_size != 0)
		{
			add_headers(m_file_stat.st_size);
			//��һ��iovecָ��ָ����Ӧ���Ļ�����������ָ��m_write_idx
			m_iv[0].iov_base = m_write_buf;
			m_iv[0].iov_len = m_write_idx;
			////�ڶ���iovecָ��ָ��mmap���ص��ļ�ָ�룬����ָ���ļ���С
			m_iv[1].iov_base = m_file_address;
			m_iv[1].iov_len = m_file_stat.st_size;
			m_iv_count = 2;

			//���͵�ȫ������Ϊ��Ӧ��ͷ�����ļ���С
			bytes_to_send = m_write_idx + m_file_stat.st_size;
			return true;
		}
		else
		{
			//����������Դ��СΪ0���򷵻ؿհ�html�ļ�

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
	//��FILE_REQUEST״̬�⣬����״ֻ̬����һ��iovec��ָ����Ӧ���Ļ�����
	m_iv[0].iov_base = m_write_buf;
	m_iv[0].iov_len = m_write_idx;
	m_iv_count = 1;
	return true;
}

//�̳߳��й������̵߳���,����http��������
void http_con::process()
{
	printf("begin process\n");
	//�ȴ�m_read_buf��ȡ������������������
	HTTP_CODE read_ret = process_read();

	//NO_REQUEST��������������Ҫ����������������
	if (read_ret == NO_REQUEST)
	{
		printf("process_no_request\n");
		//ע�Ტ�������¼�
		modfd(m_epollfd, m_sockfd, EPOLLIN);
		return;
	}

	//����process_write()��ɱ�����Ӧ��д��m_write_buf������
	bool write_ret = process_write(read_ret);


	if (!write_ret)
	{
		close_con();
	}
 //ע�Ტ����д�¼����ȴ�����д���ѻ���������д��socket

	modfd(m_epollfd, m_sockfd, EPOLLOUT);
}