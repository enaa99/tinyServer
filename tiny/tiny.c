/*
* tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
*          GET method to serve static and dynamic content
*
* tiny.c - GET 메서드를 사용하여 정적 및 동적 콘텐츠를 제공하는 단순 반복 HTTP/1.0 웹 서버
*/
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum,
char *shortmsg, char *longmsg);

// 서버의 main function, 클라이언트는 브라우저
// argc: 인자 개수, argv: 인자 배열
int main(int argc, char **argv) 
{
    // listen & connected에 필요한 file descriptor
    int listenfd, connfd;
    // client host name, port를 저장할 배열 선언
    char hostname[MAXLINE], port[MAXLINE];
    // unsigned int
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    // 입력 인자가 2개가 아닌 경우
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // 듣기 소켓 오픈
    // argv은 main 함수가 받는 각각의 인자들
    // argv[1]: 우리가 부여하는 포트
    // 해당 포트에 연결 요청을 받을 준비가 된 듣기 식별자 리턴
    // 즉, 입력ㅂ다은 port로 local에서 passive socket 생성
    listenfd = Open_listenfd(argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);

        // 서버는 accept 함수를 호출해서 클라이언트로부터의 연결 요청을 기다림
        // client 소켓은 server 소켓의 주소를 알고 있으니까
        // client에서 서버로 넘어올때 addr정보를 가지고 올 것이라고 가정
        // accept 함수는 연결되면 식별자를 리턴함
        // 듣기 식별자, 소켓 주소 구조체의 주소, 주소(소켓 구조체)
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // connfd 확인용
        // 책에는 없는 코드
        printf("connfd:%d\n", connfd);
        
        // clientaddr의 구조체에 대응되는 hostname, port를 확인
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);

        // 어떤 client가 들어왔는지 알려줌
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        // connfd로 트랜잭션 수행
        doit(connfd); 
        // connfd로 자신쪽의 연결 끝 닫기
        Close(connfd);
    }   
}

/*
 * doit - handle one HTTP request/response transaction
 *      - 하나의 HTTP 요청/응답 트랜잭션을 처리
 */
// 클라이언트의 요청 라인을 확인해, 정적 or 동적 콘텐츠를 확인하고 돌려줌
// fd는 connfd!
void doit(int fd) 
{
    int is_static;
    struct stat sbuf;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    rio_t rio;

    /* Read request line and headers */
    // 요청 라인을 읽고 분석
    // 한 개의 빈 버퍼를 설정하고, 이 버퍼와 한 개의 오픈한 파일 식별자와 연결
    // 식별자 fd를 rio_t 타입의 읽기 버퍼(rio)와 연결
    Rio_readinitb(&rio, fd);

    // 다음 텍스트 줄을 파일 rio에서 읽고, 이를 메모리 위치 buf로 복사후, 텍스트 라인을 null로 종료
    if (!Rio_readlineb(&rio, buf, MAXLINE))
        return;

    // 요청된 라인을 pritf로 보여줌(최초 요청 라인: GET / HTTP/1.1)
    printf("%s", buf);

    // buf의 내용을 method, uri, version이라는 문장열에 저장
    sscanf(buf, "%s %s %s", method, uri, version);

    // GET, HEAD, 메소드만 지원(두 문자가 같으면 0)
    if (strcasecmp(method, "GET")) {
        // 다른 메소드가 들어올 경우, 에러를 출력하고 리턴
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }

    // 요청 라인을 제외한 요청 헤더를 출력
    read_requesthdrs(&rio);

    /* Parse URI from GET request */
    // URI를 filename과 CGI argument string으로 parse 하고
    // request가 정적인지, 동적인지 확인하는 flag를 리턴함
    // 1이면 정적
    is_static = parse_uri(uri, filename, cgiargs);

    // 디스크에 파일이 없다면 filename을 sbuf에 넣음
    // 종류, 크기 등등이 sbuf에 저장 - 성공시 1, 실패시 -1
    if (stat(filename, &sbuf) < 0) {
        clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
        return;
    }

    /* Serve static content */
    // 서버 정적 콘텐츠
    if (is_static) {
        // S_ISREG-> 파일 종류 확인: 일반 파일인지 판별
        // 읽기 권한(S_IRUSR)을 가지고 있는지 판별
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
            // 읽기 권한이 없거나 정규 파일이 아니면 읽을 수 없음
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
            return;
	    }

        // fd: connfd
        // 정적 컨텐츠를 클라이언트에게 제공
	    serve_static(fd, filename, sbuf.st_size);
    }
    /* Serve dynamic content */
    // 서버 동적 컨텐츠
    else {
        // 파일이 실행 가능한지 검증
        if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
            clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't run the CGI program");
            return;
        }

        // 실행 가능하가면 동적 컨텐츠를 클라이언트에게 제공
        serve_dynamic(fd, filename, cgiargs);
    }
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];

    // Build the HTTP response body
    // sprintf는 출력하는 결과 값을 변수에 저장하게 해주는 기능있음
    sprintf(body, "<html><title>Tiny Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n",body);

    // Print the HTTP response
    // HTTP의 응답을 출력
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, strlen(body));
}

/*
 * read_requesthdrs - read HTTP request headers
 *                  - HTTP 요청 헤더 읽기
 */

// request 헤더의 정보를 하나도 사용하지 않음
// 요청 라인 한줄, 요청 헤더 여러줄을 받음
// 요청 라인은 저장(Tiny에서 필요함), 요청 헤더들은 그냥 출력
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE];

    // 한 줄을 읽어들임('\0'을 만나면 종료되는 식)
    Rio_readlineb(rp, buf, MAXLINE);
    // 읽어들인 버퍼 내용 출력
    printf("%s", buf);
    
    // strcmp(str1, str2) 같은 경우 0을 반환 -> 이 경우에만 탈출
    // buf가 "\r\n"이 되는 경우는 모든 줄을 읽고 마지막 줄에 도착한 경우
    // 헤더의 마지막 줄은 비어있음
    while(strcmp(buf, "\r\n")) {
        // 한줄 한줄 읽은 것을 출력(최대 MAXLINE라인까지 읽을 수 있음)
	    Rio_readlineb(rp, buf, MAXLINE);
	    printf("%s", buf);
    }
    
    return;
}

/*
 * parse_uri - parse URI into filename and CGI args
 *             return 0 if dynamic content, 1 if static
 * 			 - URI를 파일 이름 및 CGI 인수로 구문 분석
 *			   동적 내용인 경우 0을 반환하고 정적인 경우 1을 반환
 * 
 */

// Tiny는 정적 컨텐츠를 위한 홈 디렉토리가 자신의 현재 디렉토리이고,
// 실행파일의 홈 디렉토리는 /cgi-bin 이라고 가정
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

	// strstr(대상 문자열, 검색할 문자열) -> 검색된 문자열(대상 문자열)뒤에 모든 문자열이 나옴
	// uri에서 'cgi-bin'이라는 문자열이 없으면 정적 컨텐츠
    if (!strstr(uri, "cgi-bin")) {
		// cgiargs 인자 string을 지움
		strcpy(cgiargs, "");
		// 상대 리눅스 경로 이름으로 변환 - '.'
		strcpy(filename, ".");
		// 상대 리눅스 경로 이름으로 변환 - '.' + '/index.html'
		strcat(filename, uri);
	// URI가 '/'문자로 끝나는 경우
	if (uri[strlen(uri)-1] == '/')
		// 기본 파일 이름인 home.html을 추가
	    strcat(filename, "home.html");

		return 1;
    }
	// 동적 컨텐츠
    else {
		// 모든 CGI인자를 추출
		ptr = index(uri, '?');

		// index: 첫번째에서 두번째 인자를 찾음 - 찾으면 위치 포인터를, 못찾으면 NULL을 반환
		if (ptr) {
			strcpy(cgiargs, ptr+1);
			*ptr = '\0';
		}
		// ?가 없는 경우
		else
			// 빈칸으로 둠
			strcpy(cgiargs, "");

		// 나머지 URI 부분을 상대 리눅스 파일이름으로 변환
		strcpy(filename, ".");
		strcat(filename, uri);

		return 0;
    }
}

/*
 * serve_static - copy a file back to the client
 *              - 파일을 클라이언트에 복사
 */

// 서버가 디스크에서 파일을 찾아서 메모리 영역으로 복사하고, 복사한 것을 clientfd로 복사
void serve_static(int fd, char *filename, int filesize) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
 
    /* Send response headers to client */
    // 5개 중 무슨 파일 형식인지 검사해서 filetype을 채움
    get_filetype(filename, filetype);

    // clinet에 응답줄과 헤더를 보냄
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    // while을 한번 돌면 close가 되고, 새로 연결하면 새로 connect 하므로 close가 디폴트
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    // 여기\r\n 빈줄 하나가 헤더 종료 표시
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);

    // buf에서 strlen(buf) 바이트만큼 fd로 전송
    Rio_writen(fd, buf, strlen(buf));
    printf("Response headers:\n");
    printf("%s", buf);

    /* Send response body to client */
    // open(열려고 하는 대상 파일의 이름, 파일을 열때 적용되는 열기 옵션, 파일 열때의 접근 권한 설명)
    // return :  파일 디스크립터
    // O_RDONLY : 읽기 전용으로 파일 열기
    // 즉, filename의 파일을 읽기 전용으로 열어서 식별자를 받아옴
    srcfd = Open(filename, O_RDONLY, 0);

    // 요청한 파일을 디스크에서 가상 메모리로 mapping함
    // mmap을 호출하면 파일 srcfd의 첫번째 filesize 바이트를
    // 주소 srcp에서 시작하는 읽기-허용 가상 메모리 영역으로 mapping
    // 말록과 비슷한데 값도 복사해줌
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);

    // Mmap 대신 malloc 이용
    // srcp = (char*)malloc(filesize);
    // Rio_readn(srcfd, srcp, filesize);

    // 파일 메모리로 매핑한 후 더이상 이 식별자는 필요없으므로 닫기(메모리 누수 방지)
    Close(srcfd);
    // 실제로 파일을 client에 전송
    // Rio_writen함수는 주소 srcp에서 시작하는 filesize를 클라이언트의 연결 식별자 fd로 복사
    Rio_writen(fd, srcp, filesize);

    // 매핑된 가상 메모리 주소를 반환(메모리 누수 방지)
    Munmap(srcp, filesize);
    // free(srcp);
}

/*
 * get_filetype - derive file type from file name
 *              - 파일 이름에서 파일 형식 가져오기
 */
// Tiny는 5개의 파일 형식만 지원
void get_filetype(char *filename, char *filetype) 
{
    // filename 문자열 안에 '.html'이 있는지 검사
    if (strstr(filename, ".html"))
	    strcpy(filetype, "text/html"); 
    else if (strstr(filename, ".gif"))
	    strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))
	    strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))
	    strcpy(filetype, "image/jpeg");
    else
	    strcpy(filetype, "text/plain");
}

/*
 * serve_dynamic - run a CGI program on behalf of the client
 */

void serve_dynamic(int fd, char *filename, char *cgiargs) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    // 클라이언트에 성공을 알려주는 응답 라인을 보내는 것으로 시작
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); 
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    Rio_writen(fd, buf, strlen(buf));
  
    // 새로운 자식의 프로세스를 fork함
    if (Fork() == 0) { /* Child */
        /* Real server would set all CGI vars here */
        // 이때 부모 프로세스는 자식의 PID(process id)를, 자식 프로세스는 0을 반환받음
        // QUERY_STRING 환경변수를 요청 - URI의 CGI 인자들을 넣겠다는 뜻
        // 세번째 인자는 기존 환경변수의 유무와 상관없이 값을 변경하겠다면 1, 아니라면 0
        setenv("QUERY_STRING", cgiargs, 1);
        /* Redirect stdout to client */
        // dup2 함수를 통해 표준 출력을 클라이언트와 연계된 연결 식별자로 재지정
        // -> CGI 프로그램이 표준 출력으로 쓰는 모든 것은 클라이언트로 바로 감(부모프로세스의 간섭 없이)
        Dup2(fd, STDOUT_FILENO);
        /* Run CGI program */
        // CGI 프로그램을 실행 - adder을 실행
        Execve(filename, emptylist, environ);
    }
    
    /* Parent waits for and reaps child */
    // 자식이 아니면
    // 즉, 부모는 자식이 종료되어 정리되는 것을 기다리기 위해 wait함수에서 블록됨
    Wait(NULL);
}