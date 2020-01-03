#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define true 1
#define false 0
#define HEADER_NUM 4
#define BUFLEN 1024
#define FUNCTION_SIZE 3
#define MAXLEN 50

char jfName[BUFLEN];  // Java 파일 이름
char cfName[BUFLEN];  //   C  파일 이름 
char ffName[BUFLEN];  // class 파일 이름 
char mfName[BUFLEN];  // Makefile 파일 이름 
FILE *jfp;            // Java 파일 포인터
FILE *cfp;            //   C  파일 포인터
FILE *ffp;            // class 파일 포인터 
FILE *mfp;            // Makefile 파일 포인터 
int classPar = -1;    // class 괄호 정보 (class 파일 끝을 확인)

char make[10][BUFLEN];  // 파일명_Makefile 만들기 위한 재료
int makeNum = 0;
char mfName[BUFLEN];

int jOption = false;
int cOption = false;
int pOption = false;
int fOption = false;
int lOption = false;
int rOption = false;

struct Header {
    char type[20];
    char lines[BUFLEN];
    int flag;
};
struct Header hd[HEADER_NUM];

pid_t pid, wpid;  //  r option 시 사용할 process ID 변수 
int convertLine = 0;
int tab_cnt = 0;  //  tab 갯수 카운터 

char java_function[FUNCTION_SIZE][MAXLEN] = {"System.out.printf(", "nextInt(", "new FileWriter("};
char c_function[FUNCTION_SIZE][MAXLEN] = {"printf", "scanf", "open"};
int saved_p[FUNCTION_SIZE];

int check_option(int argc, char *argv[]);
void get_header(void);
void print_file(char *path);
void write_tab(int fileFlag);
void write_header(char *type, int fileFlag);
void convert(char *linebuf, int fileFlag);
void do_pOption(void);
void do_fOption(void);
void do_lOption(void);
void make_Makefile(void);

void ssu_convert(int argc, char *argv[])
{
    char charbuf[2];
    char linebuf[BUFLEN];
    int i;

    // 인자가 2개 미만인 경우: 사용법 출력
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <FILE NMAE> [OPTION]\n", argv[0]);
        exit(1);
    }

    // 인자가 2개 이상인 경우
    if (argc >= 2) {
        strcpy(jfName, argv[1]);  // 첫 번째 인자를 jfName으로 복사해준다.
        if (access(jfName, F_OK) < 0) {  // Java file이 없는 경우 error 처리
            fprintf(stderr, "%s doesn't exist\n", jfName);
            exit(1);
        }

        if (!check_option(argc, argv)) // option 처리 
            exit(1);
    }

    // Makefile 재료 초기화
    for (i = 0; i < sizeof(make) / sizeof(make[0]); i++) {
        strcpy(make[i], "");
    }

    // .java 파일 열기
    if ((jfp = fopen(jfName, "r")) == NULL) {
        fprintf(stderr, "file open error for %s\n", jfName);
        exit(1);
    }

    //  Makefile에 등록  
    strcpy(mfName, jfName);
    for (i = strlen(jfName) - 1; i >= 0; i--) {
        if (jfName[i] != '.')
            mfName[i] = '\0';
        else {
            mfName[i] = '\0';
            break;
        }
    }
    strcpy(make[makeNum++], mfName);

    //  .c 파일 만들기
    strcpy(cfName, jfName);
    for (i = strlen(jfName) - 1; i >= strlen(jfName) - 4; i--) {
        if (i > strlen(jfName) -4)
            cfName[i] = '\0';
        else
            cfName[i] = 'c';
    }
    if ((cfp= fopen(cfName, "w+")) == NULL) {
        fprintf(stderr, "file open error for %s\n", cfName);
        exit(1);
    }
    //  class 파일 초기화
    strcpy(ffName , "");

    //  header 파일에서 헤더 파일 구조체로 받아오기
    get_header();

    //  line 별로 변환 함수로 보내주기
    fseek(jfp, 0 , SEEK_SET);  //  파일의 처음부터 
    memset(charbuf, 0, 1);  memset(linebuf, 0, BUFLEN);  //  buf들 초기화 
    int jsize;
    while (fread(charbuf, sizeof(char), 1, jfp) == 1) {

        strcat(linebuf, charbuf);  //  linebuf에 한 문자씩 넣어주기

        if (charbuf[0] == '\n') {  // 개행을 만나면 convert 함수로 넘겨주기 

            if ((strstr(linebuf, "class") != NULL) && (strstr(linebuf, "public") == NULL)) { // class 생성 
                for (i = 0; i < strlen(linebuf); i++) {
                    if ((linebuf[i] == 'c') && (linebuf[i+1] == 'l') && (linebuf[i+2] == 'a') && (linebuf[i+3] == 's') && (linebuf[i+4] == 's')) {  //  class를 만나면 
                        i += 5;
                        while ((linebuf[i] == ' ') || (linebuf[i] == '\t')) {  // class 이름 전 공백들 
                            i++;
                        }

                        int j = 0;
                        while ( ((linebuf[i] >= 65) && (linebuf[i] <= 90)) || ((linebuf[i] >= 97) && (linebuf[i] <= 122)) || ((linebuf[i] >= 48) && (linebuf[i] <= 57)) || (linebuf[i] == 95)) {  // class 이름 알아내기 
                            ffName[j++] = linebuf[i++];
                        }
                        ffName[j] = '\0';
                        strcpy(make[makeNum++], ffName);
                        ffName[j] = '.';
                        ffName[j+1] = 'c';
                        ffName[j+2] = '\0';

                        while (i < strlen(linebuf)) {
                            if (linebuf[i++] == '{')
                                classPar = 0;
                        }
                    }
                }
                memset(linebuf, 0, BUFLEN);  // linebuf 초기화 
                if ((ffp= fopen(ffName, "w+")) == NULL) {
                    fprintf(stderr, "file open error for %s\n", ffName);
                    exit(1);
                }

                while (fread(charbuf, sizeof(char), 1, jfp) == 1) {
                    if (charbuf[0] == '{')
                        classPar++;
                    if (charbuf[0] == '}')
                        classPar--;

                    if (classPar < 0) {  //  class 입력 완료 
                        break;
                    }

                    strcat(linebuf, charbuf);
                    if (charbuf[0] == '\n') {
                        if ((ffp = fopen(ffName, "r+")) == NULL) {
                            fprintf(stderr, "file open error for %s\n", ffName);
                            exit(1);
                        }
                        fseek(ffp, 0, SEEK_END);
                        convert(linebuf, 1);
                        convertLine++;
                        memset(linebuf, 0, BUFLEN);  // linebuf 초기화 
                    }
                }
                for (i = 0; i < sizeof(hd) / sizeof(struct Header); i++) { // header 쓰임 기록 초기화 
                    hd[i].flag = false;
                }
            }

            else {  // main 클래스 
                if ((cfp = fopen(cfName, "r+")) == NULL) {
                    fprintf(stderr, "file open error for %s\n", cfName);
                    exit(1);
                }
                fseek(cfp, 0, SEEK_END);
                convert(linebuf, 0);
                convertLine++;
                memset(linebuf, 0, BUFLEN);  // linebuf 초기화 
            }

        }
    }

    //  .java, .c 파일 닫기 
    fclose(jfp);
    fclose(cfp);

    //  파일명_Makefile  만들기 
    make_Makefile();

    //  converting 끝남을 알림 
    if (rOption)
        system("clear");
    printf("%s converting is finished\n", cfName);

    //  j option : .java 출력
    if (jOption) {
        printf("\n*%s\n", jfName);
        print_file(jfName);
    }

    //  c option : .c 출력
    if (cOption) {
        if (strcmp(ffName, "") != 0) {  //  class 파일이 있었다면 class 파일도 출력 
            printf("\n*%s\n", ffName);
            print_file(ffName);
        }
        printf("\n*%s\n", cfName);
        print_file(cfName);
    }

    //  p option : 대응되는 라이브러리 함수 출력
    if (pOption) {
        do_pOption();
    }

    //  f option : 파일 크기 출력
    if (fOption) {
        do_fOption();
    }

    //  l option : 파일 라인 수  출력
    if (lOption) {
        do_lOption();
    }

    return;
}

int check_option(int argc, char *argv[]) // option 처리를 해주는 함수이다.
{
    int option;
    //  getopt 함수를 이용하여 j, c, p, f, l ,r 옵션을 받는다. 
    while ((option = getopt(argc, argv, "jcpflr")) != -1) {
        switch (option) {
            case 'j':
                jOption = true;
                break;
            case 'c':
                cOption = true;
                break;
            case 'p':
                pOption = true;
                break;
            case 'f':
                fOption = true;
                break;
            case 'l':
                lOption = true;
                break;
            case 'r':
                rOption = true;
                break;
            case '?':
                printf("Unknown option %c\n", optopt);
                return false;
        }
    }
    return true;
}

void get_header(void)
{
    FILE *hfp;
    char onebyte[2];
    int find_type;
    int tbn = 0;
    int lbn = 0;
    int i, j;
    if ((hfp = fopen("header", "r")) == NULL) {
        fprintf(stderr, "fopen error for header\n");
        exit(1);
    }
    j = 0;
    for (i = 0; i < sizeof(hd) / sizeof(struct Header); i++) {
        find_type = false;  //  line 마다 다른 type의 헤더들을 찾아야 한다.
        tbn = 0;
        lbn = 0;
        while (1) {
            if (fread(onebyte, 1, 1, hfp) != 1)  // 1 byte 씩 읽어오기 
                break;

            if (!(find_type)) {  //  header 타입 찾기 
                if (onebyte[0] == ' ') {  // 공백 만나면 #include 찾는 단계로 넘어가기 
                    find_type = true;
                    hd[i].type[tbn] = '\0';
                }
                else
                    hd[i].type[tbn++] = onebyte[0];
            }
            else {  //  #include 라인들 받아오기 
                if (onebyte[0] == '\n') {  // 한 line이 끝난것이다. 
                    hd[i].lines[lbn++] = onebyte[0];
                    break;
                }
                if (onebyte[0] == '#') {
                    if (lbn != 0)
                        hd[i].lines[lbn - 1] = '\n';
                }
                hd[i].lines[lbn++] = onebyte[0];
            }
        }
        hd[i].flag = false;
    }
    fclose(hfp);
}

void print_file(char *path)  // .java, .c 파일을 출력해주는 함수이다.
{
    char charbuf[2];
    char linebuf[BUFLEN];
    FILE *fp;
    int linecnt = 1;

    if ((fp = fopen(path, "r")) == NULL) {   // fopen error
        fprintf(stderr, "file open error for %s\n", path);
        exit(1);
    }

    fseek(fp, 0, SEEK_SET);   //  파일 처음부터 읽기 
    memset(charbuf, 0, 2); memset(linebuf, 0, BUFLEN);  // buf 초기화 
    while (fread(charbuf, sizeof(char), 1, fp) == 1) {  // 한 문자 씩 읽어오기  
        strcat(linebuf, charbuf);  //  linebuf 만들기 
        if (charbuf[0] == '\n') {  //  한 줄 완성
            if (linecnt < 10)
                printf("  %d ", linecnt++);
            else if (linecnt < 100)
                printf(" %d ", linecnt++);
            else
                printf("%d ", linecnt++);
            printf("%s", linebuf);
            memset(linebuf, 0, BUFLEN);
        }
    }
    fclose(fp);   // 파일 닫기  
    return;
}

void write_tab(int fileFlag)  //  .c file에 가독성을 위한 탭을 써주는 함수이다. 
{
    for (int i = 0; i < tab_cnt; i++) {
        if (fileFlag == 0)
            fwrite("\t", 1, 1, cfp);
        else
            fwrite("\t", 1, 1, ffp);
    }
}

// convert 함수에서 header을 써주는 역할을 하는 함수이다.
// header의 type을 받아와서 파일 맨 앞에 header을 써주고
// 쓴 크기만큼 원래 파일을 뒤로 복사, 붙여넣기를 하여
// header line을 삽입한 효과를 만든다.
void write_header(char *type, int fileFlag)  
{
    int i;
    off_t org_off, chg_off;
    char *copybuf;

    if (fileFlag == 0) {
    org_off = ftell(cfp);  //  원래 파일 offset위치 기억 = 파일 크기 

    copybuf = malloc(org_off + 1);  //  copybuf에 원래 파일을 저장해두기
    memset(copybuf, 0, org_off + 1);
    fseek(cfp, 0, SEEK_SET);
    fread(copybuf, org_off, 1, cfp);

    for (i = 0; i < sizeof(hd) / sizeof(struct Header); i++) {  // 파일 맨 앞에 header 써주기
        if ((strcmp(hd[i].type, type) == 0) && (!(hd[i].flag)) ) {
            fseek(cfp, 0, SEEK_SET);
            fwrite(hd[i].lines, strlen(hd[i].lines), 1, cfp);
            fwrite(copybuf, strlen(copybuf), 1, cfp);  //  복사해둔 원래 파일을 header크기만큼 뒤로 복붙!
            hd[i].flag = true;
            break;
        }
    }
    }
    else {
    org_off = ftell(ffp);  //  원래 파일 offset위치 기억 = 파일 크기 

    copybuf = malloc(org_off + 1);  //  copybuf에 원래 파일을 저장해두기
    memset(copybuf, 0, org_off + 1);
    fseek(ffp, 0, SEEK_SET);
    fread(copybuf, org_off, 1, ffp);

    for (i = 0; i < sizeof(hd) / sizeof(struct Header); i++) {  // 파일 맨 앞에 header 써주기
        if ((strcmp(hd[i].type, type) == 0) && (!(hd[i].flag)) ) {
            fseek(ffp, 0, SEEK_SET);
            fwrite(hd[i].lines, strlen(hd[i].lines), 1, ffp);
            fwrite(copybuf, strlen(copybuf), 1, ffp);  //  복사해둔 원래 파일을 header크기만큼 뒤로 복붙!
            hd[i].flag = true;
            break;
        }
    }
    }

    free(copybuf);

    return;
}

//  .java -> .c 로 변환하는 함수이다
void convert(char *linebuf, int fileFlag)
{
    int i;
    char copypart[BUFLEN];  //  printf 변환 때 사용 
    char fileObject[BUFLEN];  //  파일 입출력 변환 시 File 객체 변수명
    char newline[BUFLEN];  // 변환한 새로운 줄을 입력할 때 사용 
    int state; 
    if (strchr(linebuf, '{') != NULL) {
        if (strstr(linebuf, "public class"))
            return;
        tab_cnt++;
    }
    else if (strchr(linebuf, '}') != NULL) {
        tab_cnt--;
        if (tab_cnt < 0)
            return;
    }

    if (strcmp(linebuf, "\n") == 0) {
        if (fileFlag == 0)
            fwrite("\n", 1, 1, cfp);
        else
            fwrite("\n", 1, 1, ffp);
    }
    else if ((fileFlag == 1) && (classPar == 0) && (strstr(linebuf, "[]") != NULL)) {  // 전역 변수로 [] 가 나오면 * 로 바꾸어주기 
            char val[BUFLEN];
            char *ptr;
            int j = 0;
            i = 0;
            while ((linebuf[i] == '\t') || (linebuf[i] == ' '))
                i++;
            while (linebuf[i] != '[') {
                val[j++] = linebuf[i++];
            }
            val[j] = '\0';

            ptr = strstr(linebuf, "[]");
            ptr += 2;

            strcpy(newline, val);
            strcat(newline, " * ");
            strcat(newline, ptr);
            //  class 파일에 넣어주기 
            fwrite(newline, strlen(newline), 1, ffp);
    }
    // printf의 " "안에 뒤에 검사할 함수들이 들어가 있을 경우
    // 인식하면 안되므로 printf를 먼저 검사해야한다. 
    else if (strstr(linebuf, java_function[0]) != NULL) {  // printf() 변환 
        char *afterPrt;
        char *afterPar;
        saved_p[0] = true;

        afterPrt = strstr(linebuf, "System.out.printf");
        afterPrt += 17;
        strcpy(copypart, afterPrt);
        i = 0;
        while (copypart[i] != '"')
            i++;
        i++;
        while (copypart[i] != '"')
            i++;
        i++;
        afterPar = copypart + i;  //  "~~~" 다음 부분 
        if ((strchr(afterPar, ',') != NULL) && (strchr(afterPar, '.') != NULL) && (strchr(afterPar, '(') != NULL)) {  //  "" 다음에 , . ( 가 나오면 java의 멤버함수가 사용된 것이다.
            i = strlen(copypart) - 1;
            while (copypart[i] != '"') {
                if (copypart[i] == '.') {
                    while ((copypart[i] != ' ') && (copypart[i] != ',')) {
                        copypart[i--] = ' ';
                    }
                }
                i--;
            }
        }

        strcpy(newline, c_function[0]);  //  newline 만들기 
        strcat(newline, copypart);
        // .c 파일에 써주기 
        if (fileFlag == 0) {
            write_tab(0);
            fwrite(newline, strlen(newline), 1, cfp);
        }
        else {
            write_tab(1);
            fwrite(newline, strlen(newline), 1, ffp);
        }

        if (fileFlag == 0)
            write_header("io", 0);
        else
            write_header("io", 1);
    }
    else if (strstr(linebuf, "import") != NULL) {  // import ~
    }
    else if (strstr(linebuf, "public static void main") != NULL) {  // main 함수 line
        int par_exist = false;
        if (strchr(linebuf, '{') != NULL) {
            tab_cnt--;
            par_exist = true;
        }

        if (fileFlag == 0)
            write_tab(0);
        else
            write_tab(1);
        if (fileFlag == 0)
            fwrite("int main(void) ", 15, 1, cfp);
        else
            fwrite("int main(void) ", 15, 1, ffp);

        if (par_exist) {
            tab_cnt++;
            if (fileFlag == 0)
                fwrite("{", 1, 1, cfp);
            else
                fwrite("{", 1, 1, ffp);
        }
        if (fileFlag == 0)
            fwrite("\n", 1, 1, cfp);
        else
            fwrite("\n", 1, 1, ffp);
    }
    else if ((strstr(linebuf, "public ") != NULL) && (strchr(linebuf, '(') != NULL) && (strchr(linebuf, ')') != NULL) && (strstr(linebuf, "class") == NULL) && (strstr(linebuf, "main") == NULL)) {  //  class 생성시 함수 선언부
        char *ptr;
        char className[BUFLEN];  //  생성자인지 검사하기 위한 string 
        className[0] = ' ';
        for (i = 0; i < strlen(ffName); i++) {
            className[i+1] = ffName[i];
        }
        className[strlen(ffName)-1] = '(';
        className[strlen(ffName)] = ')';
        className[strlen(ffName)+1] = '\0';

        ptr = strstr(linebuf, "public ");
        ptr += 6;

        if (strstr(linebuf, className) != NULL) { // 생성자 함수일 때 앞에 void 써주기 
            if (fileFlag == 0) {
                fwrite("void ", 5, 1, cfp);
            }
            else {
                fwrite("void ", 5, 1, ffp);
            }
        }

        if (fileFlag == 0) {
            fwrite(ptr+1, strlen(ptr)-1, 1, cfp);
        }
        else {
            fwrite(ptr+1, strlen(ptr)-1, 1, ffp);
        }
    }
    else if (strstr(linebuf, "public static final ") != NULL) {  //  #define으로 변환 
        char *ptr;
        char val[BUFLEN];  // 자료형 
        char value[BUFLEN];  // 변수값 
        ptr = strstr(linebuf, "public static final ");
        ptr += 20;

        int j = 0;
        for (i = 0; i < strlen(ptr); i++) {  //  자료형과 변수값 구하기 
            if (ptr[0] == ' ') {  //  자료형 전에 빈칸이 있다면
                while(ptr[i] == ' ')
                    i++;
            }
            while (ptr[i] != ' ')  // 자료형 부분
                i++;
            while (ptr[i] == ' ')  // 자료형과 변수명 사이 공백 
                i++;
            while (ptr[i] != '=')  //  변수명 구하기 
                val[j++] = ptr[i++];
            val[j] = '\0';
            j = 0;
            i++;
            while (ptr[i] == ' ')  //  변수값 나오기 전 공백 
                i++;
            while (ptr[i] != ';')
                value[j++] = ptr[i++];
            value[j] = '\0';
            break;
        }
        
        strcpy(newline, "#define ");
        strcat(newline, val);
        strcat(newline, " ");
        strcat(newline, value);

        if (fileFlag == 0) {
            write_tab(0);
            fwrite(newline, strlen(newline), 1, cfp);
        }
        else {
            write_tab(1);
            fwrite(newline, strlen(newline), 1, ffp);
        }
    }
    else if (strstr(linebuf, "new Scanner(") != NULL) {  // Scanner 변수 = new Scanner(System.in)
    }
    else if (strstr(linebuf, java_function[1]) != NULL) {  // scanf() 변환
        char scan_var[BUFLEN];
        int var = 0;
        saved_p[1] = true;
        for (i = 0; i < strlen(linebuf); i++) {
            if (linebuf[i] != '\t'){
                if (linebuf[i] == ' ')
                    break;
                scan_var[var++] = linebuf[i];
            }
        }
        scan_var[var] = '\0';
        strcpy(newline, c_function[1]);
        strcat(newline, "(\"\%d\", &");
        strcat(newline, scan_var);
        strcat(newline, ");\n");
        if (fileFlag == 0)
            write_tab(0);
        else
            write_tab(1);
        if (fileFlag == 0)
            fwrite(newline, strlen(newline), 1, cfp);
        else
            fwrite(newline, strlen(newline), 1, ffp);

        if (fileFlag == 0)
            write_header("io", 0);
        else
            write_header("io", 1);
    }
    else if (strstr(linebuf, "new File(") != NULL) {  //  new File 변환
        char fNameVal[BUFLEN];  // 파일 이름의 변수이다. 
        int fNameFlag = false;
        char fileName[BUFLEN];  // new File("~"); 에 있는 파일 이름
        
        for(i = 0; i < strlen(linebuf); i++) {  //  파일 이름/변수  찾는 작업이다. 
            if (fNameFlag == false) {  //  아직 파일 이름 변수를 찾지 못했다면 
                if ((linebuf[i] == 'F') && (linebuf[i+1] == 'i') && (linebuf[i+2] == 'l') && (linebuf[i+3] == 'e') && (linebuf[i+4] == ' ')) {  //  'File '을 만난다면 
                    i += 5;
                    int j = 0;
                    while (linebuf[i] != '=') {
                        fNameVal[j++] = linebuf[i++];  //  파일 이름 변수의  이름
                    }
                    fNameVal[j] = '\0';
                    fNameFlag = true;
                    i++;
                }
            }
            if (linebuf[i] == '"') {  //  파일 이름이 나오는 곳 
                i++;
                for (int j = 0; i < strlen(linebuf); j++) {
                    if (linebuf[i] == '"') {
                        fileName[j] = '\0';
                        break;
                    }
                    else {
                        fileName[j] = linebuf[i++];  //  파일 이름 
                    }
                }
            }
        }
        // char* [파일이름 변수의 이름] = "[파일 이름]";
        strcpy(newline, "char* ");
        strcat(newline, fNameVal);
        strcat(newline, " = \"");
        strcat(newline, fileName);
        strcat(newline, "\";\n");
        //  .c 파일에 써주는 작업 
        if (fileFlag == 0)
            write_tab(0);
        else
            write_tab(1);
        if (fileFlag == 0)
            fwrite(newline, strlen(newline), 1, cfp);
        else
            fwrite(newline, strlen(newline), 1, ffp);
    }
    else if (strstr(linebuf, java_function[2]) != NULL) {  //  new FileWriter 변환
        char fdVal[BUFLEN];  //  파일 디스크립터로 쓰일 변수 이름이다.
        int fdFlag = false;
        char fNameVal[BUFLEN]; // 파일이름의 변수의 이름이다.
        char fileExist[10];  //  FileWriter(file, ____); 에 쓰이는 파일 존재여부 문자열이다.
        
        saved_p[2] = true;  //  등록된 3번째 함수 변환 
        
        for (i = 0; i < strlen(linebuf); i++) {

            if (fdFlag == false) {  //  아직 파일 디스크립터의 이름을 찾지 못했다면 
                if ((linebuf[i] == 'F') && (linebuf[i+1] == 'i') && (linebuf[i+2] == 'l') && (linebuf[i+3] == 'e') && (linebuf[i+4] == 'W') && (linebuf[i+5] == 'r') && (linebuf[i+6] == 'i') && (linebuf[i+7] == 't') && (linebuf[i+8] == 'e') && (linebuf[i+9] == 'r')) {  // FileWriter을 만나면 
                    i += 10;
                    int j = 0;
                    while (linebuf[i] != '=') {
                        fdVal[j++] = linebuf[i++];
                    }
                    fdVal[j] = '\0';  //  파일디스크립터 이름 
                    fdFlag = true;
                    i++;
                }
            }

            if (linebuf[i] == '(') {  //  파일이름 변수의 이름, 파일 존재여부 가 나오는 곳 
                i++;
                int j = 0;
                while (linebuf[i] != ',') {  //  파일이름 변수의 이름 
                    fNameVal[j++] = linebuf[i++];
                }
                fNameVal[j] = '\0';
                j = 0; i++;
                while (linebuf[i] != ')') {  //  파일 존재여부의 문자열 
                    fileExist[j++] = linebuf[i++];
                }
                fileExist[j] = '\0';
            }
        }

        // int [파일 디스크립터];
        strcpy(newline, "int ");
        strcat(newline, fdVal);
        strcat(newline, ";\n");
        //  .c 파일에 써주는 작업 
        if (fileFlag == 0) {
            write_tab(0);
            fwrite(newline, strlen(newline), 1, cfp);
        }
        else {
            write_tab(1);
            fwrite(newline, strlen(newline), 1, ffp);
        }
        strcpy(newline, "");

        // if (([파일 디스크립터] = open([파일이름 변수의 이름], O_WRONLY | O_APPEND, 0666)) < 0) {
        // if (([파일 디스크립터] = open([파일이름 변수의 이름], O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {
        if (strstr(fileExist, "true") != NULL) {  // FileWriter(___, true);
            strcpy(newline, "if ((");
            strcat(newline, fdVal);
            strcat(newline, " = open(");
            strcat(newline, fNameVal);
            strcat(newline, ", O_WRONLY | O_APPEND, 0666)) < 0) {\n");
            for (int k = 0; k < tab_cnt+1; k++)
                strcat(newline, "\t");
            strcat(newline, "fprintf(stderr, \"open error\");\n");
            for (int k = 0; k < tab_cnt+1; k++)
                strcat(newline, "\t");
            strcat(newline, "exit(1);\n");
            for (int k = 0; k < tab_cnt; k++)
                strcat(newline, "\t");
            strcat(newline, "}\n");
        }
        else {  //  FileWriter(___, false);
            strcpy(newline, "if ((");
            strcat(newline, fdVal);
            strcat(newline, " = open(");
            strcat(newline, fNameVal);
            strcat(newline, ", O_WRONLY | O_CREAT | O_TRUNC, 0666)) < 0) {\n");
            for (int k = 0; k < tab_cnt + 1; k++)
                strcat(newline, "\t");
            strcat(newline, "fprintf(stderr, \"open error\\n\");\n");
            for (int k = 0; k < tab_cnt + 1; k++)
                strcat(newline, "\t");
            strcat(newline, "exit(1);\n");
            for (int k = 0; k < tab_cnt; k++)
                strcat(newline, "\t");
            strcat(newline, "}\n");
        }
        //  .c 파일에 써주는 작업 
        if (fileFlag == 0) {
            write_tab(0);
            fwrite(newline, strlen(newline), 1, cfp);
        }
        else {
            write_tab(1);
            fwrite(newline, strlen(newline), 1, ffp);
        }

        if (fileFlag == 0) {
            write_header("exit", 0);
            write_header("open", 0);
        }
        else {
            write_header("exit", 1);
            write_header("open", 1);
        }
    }
    else if (strstr(linebuf, "write(") != NULL) {  //  write() 변환
        char fdName[BUFLEN];
        int fdFlag = false;
        char content[BUFLEN];
        for (i = 0; i < strlen(linebuf); i++) {
            if (fdFlag == false) {
                if ((linebuf[i] != '\t') && (linebuf[i] != ' ')) {
                    int j = 0;
                    while (linebuf[i] != '.') {
                        fdName[j++] = linebuf[i++];
                    }
                    fdName[j] = '\0';
                    fdFlag = true;
                    i++;
                }
            }
            if (linebuf[i] == '(') {
                int j = 0;
                i++;
                while (linebuf[i] != ')') {
                    content[j++] = linebuf[i++];
                }
                content[j] = '\0';
                i++;
            }
        }
        // newline 만들어주기
        strcpy(newline, "write(");
        strcat(newline, fdName);
        strcat(newline, ", ");
        strcat(newline, content);
        if (strstr(content, "\"") != NULL) {
            int size;
            char sizeStr[10];
            size = strlen(content) - 2;
            sprintf(sizeStr, "%d", size);
            strcat(newline, ", ");
            strcat(newline, sizeStr);
        }
        else {
            strcat(newline, ", sizeof(");
            strcat(newline, content);
            strcat(newline, ")");
        }
        strcat(newline, ");\n");
        // .c 파일에 써주기 
        if (fileFlag == 0)
            write_tab(0);
        else
            write_tab(1);
        if (fileFlag == 0)
            fwrite(newline, strlen(newline), 1, cfp);
        else
            fwrite(newline, strlen(newline), 1, ffp);

        if (fileFlag == 0)
            write_header("read", 0);
        else
            write_header("read", 1);
    }
    else if (strstr(linebuf, "flush(") != NULL) {  //  flush() 변환
        char fdName[BUFLEN];
        int fdFlag = false;
        for(i = 0; i < strlen(linebuf); i++) {
            if (fdFlag == false) {
                if ((linebuf[i] != '\t') && (linebuf[i] != ' ')) {
                    int j = 0;
                    while (linebuf[i] != '.') {
                        fdName[j++] = linebuf[i++];
                    }
                    fdName[j] = '\0';
                    fdFlag = true;
                    i++;
                }
            }
        }
        // newline 만들기 
        strcpy(newline, "if(fsync(");
        strcat(newline, fdName);
        strcat(newline, ") < 0) {\n");
        for (int k = 0; k < tab_cnt+1; k++)
            strcat(newline, "\t");
        strcat(newline, "fprintf(stderr, \"fsync error\\n\");\n");
        for (int k = 0; k < tab_cnt+1; k++)
            strcat(newline, "\t");
        strcat(newline, "exit(1);\n");
        for (int k = 0; k < tab_cnt; k++)
            strcat(newline, "\t");
        strcat(newline, "}\n");
        // .c 파일에 써주기 
        if (fileFlag == 0)
            write_tab(0);
        else
            write_tab(1);
        if (fileFlag == 0)
            fwrite(newline, strlen(newline), 1, cfp);
        else
            fwrite(newline, strlen(newline), 1, ffp);

        if (fileFlag == 0)
            write_header("read", 0);
        else
            write_header("read", 1);
    }
    else if ((strstr(linebuf, "if") != NULL) && (strstr(linebuf, "!= null") != NULL)) {
    }
    else if (strstr(linebuf, "close(") != NULL) { // close() 변환 
        char fdName[BUFLEN];
        int fdFlag = false;
        //  파일 디스크립터 변수명 찾기
        for(i = 0; i < strlen(linebuf); i++) {  
            if (fdFlag == false) {
                if ((linebuf[i] != '\t') && (linebuf[i] != ' ')) {
                    int j = 0;
                    while (linebuf[i] != '.') {
                        fdName[j++] = linebuf[i++];
                    }
                    fdName[j] = '\0';
                    fdFlag = true;
                    i++;
                }
            }
        }
        //  newline 만들기 
        strcpy(newline, "close(");
        strcat(newline, fdName);
        strcat(newline, ");\n");
        //  .c 파일에 써주기
        if (fileFlag == 0) {
            write_tab(0);
            fwrite(newline, strlen(newline), 1, cfp);
        }
        else {
            write_tab(1);
            fwrite(newline, strlen(newline), 1, ffp);
        }
    }
    else if (strchr(linebuf, '.') != NULL) {
        char *ptr = strchr(linebuf, '.');
        if ((strchr(ptr, '(') != NULL) && (strchr(ptr, ')') != NULL)) { // .뒤에 ()가 나와 함수일 경우 
            if (fileFlag == 0) {
                write_tab(0);
                fwrite(ptr+1, strlen(ptr) - 1, 1, cfp);
            }
            else {
                write_tab(1);
                fwrite(ptr+1, strlen(ptr) - 1, 1, ffp);
            }
        }
        else { // . 뒤에 () 가 안나오면 멤버 변수이므로 그대로 출력 
            if (linebuf[0] == '\t')
                linebuf++;
            if (fileFlag == 0)
                fwrite(linebuf, strlen(linebuf), 1, cfp);
            else
                fwrite(linebuf, strlen(linebuf), 1, ffp);
        }
    }
    else if (strstr(linebuf, " new ") != NULL) {
        char *ptr;
        char valType[20];  //  자료형
        char valNum[50];  // 배열일 경우 배열 크기 
        char className[BUFLEN];  //  생성자인지 검사하기 위한 string 
        if (strcmp(ffName, "") != 0) {
            className[0] = ' ';
            for (i = 0; i < strlen(ffName); i++) {
               className[i+1] = ffName[i];
            }
            className[strlen(ffName)-1] = '(';
            className[strlen(ffName)] = ')';
            className[strlen(ffName)+1] = '\0';
        }
        
        if ((strcmp(ffName, "") != 0) && (fileFlag == 0) && (strstr(linebuf, className) != NULL)) {
            strcpy(newline, className+1);  //  .c 파일에서 생성자 함수가 나왔을 시 
            strcat(newline, ";\n");
            write_tab(0); 
            fwrite(newline, strlen(newline), 1, cfp);
        }
        else { // 동적 자료형 변환 ex) stack = (int *) malloc(sizeof(int) * STACK_SIZE);
            strcpy(newline, "");
            int j = 0;
            for (i = 0; i < strlen(linebuf); i++) {  // new 나오기 전 부분을  newline에 입력하기 
                if ((linebuf[i] != ' ') && (linebuf[i] != '\t')) {
                    while ((linebuf[i] != 'n') || (linebuf[i+1] != 'e') || (linebuf[i+2] != 'w')) {
                        newline[j++] = linebuf[i++];
                    }
                    newline[j] = '\0';
                    break;
                }
            }

            ptr = strstr(linebuf, " new ");
            ptr += 5;
            if (strchr(ptr, '[') == NULL) {  //  '['가 없는 그냥 자료형인 경우 
                j = 0;
                for (i = 0; i < strlen(ptr); i++) {  //  자료형 구하기 
                    if (ptr[i] == ';') {
                        valType[j] = '\0';
                        break;
                    }
                    valType[j++] = ptr[i];
                }
                
                strcat(newline, "(");
                strcat(newline, valType);
                strcat(newline, ")");
                strcat(newline, " malloc(sizeof(");
                strcat(newline, valType);
                strcat(newline, "));\n");
            }
            else {  //  '['가 있어 배열 형태일 경우 
                j = 0;
                for (i = 0; i < strlen(ptr); i++) { // 자료형 구하기 
                    if (ptr[i] == '[') {
                        valType[j] = '\0';
                        i++;
                        break;
                    }
                    valType[j++] = ptr[i];
                }
                
                j = 0;
                for (; i < strlen(ptr); i++) {  // 배열 크기 구하기 
                    if (ptr[i] == ']') {
                        valNum[j] = '\0';
                        break;
                    }
                    valNum[j++] = ptr[i];
                }

                strcat(newline, "(");
                strcat(newline, valType);
                strcat(newline, " *) malloc(sizeof(");
                strcat(newline, valType);
                strcat(newline, ") * ");
                strcat(newline, valNum);
                strcat(newline, ");\n");
            }
            
            if (fileFlag == 0) {
                write_tab(0);
                fwrite(newline, strlen(newline), 1, cfp);
                write_header("exit", 0);  //  malloc 은 <stdlib.h>이 있어야 한다.
            }
            else {
                write_tab(1);
                fwrite(newline, strlen(newline), 1, ffp);
                write_header("exit", 1);
            }
        }
    }
    else {
        if (linebuf[0] == '\t')
            linebuf++;
        if (fileFlag == 0)
            fwrite(linebuf, strlen(linebuf), 1, cfp);
        else
            fwrite(linebuf, strlen(linebuf), 1, ffp);
    }

    if (fileFlag == 0)
        fclose(cfp);
    else
        fclose(ffp);
    
    //  r 옵션 실행하는 부분이다.
    //  convertLine 이라는 변수에 java파일 변환 줄 개수를 저장해 놓아
    //  변환되는 줄까지 출력해주며
    //  현재 변환되고 있는 파일을 밑에 출력해준다. 
    if (rOption) {
        if ((pid = fork()) < 0) {
            fprintf(stderr, "fork error\n");
            exit(1);
        }
        else if (pid == 0) {  // 자식 프로세스 
            char buf[BUFLEN];
            int size;
            system("clear");
            printf("%s Converting....\n", jfName);
            printf("- - - - - - - - - -\n");
            printf("%s\n", jfName);
            printf("- - - - - - - - - -\n");
            fseek(jfp, 0, SEEK_SET);
            for (int j = 0; j <= convertLine; j++) {
                fgets(buf, sizeof(buf), jfp);
                buf[strlen(buf) - 1] = '\0';
                if (j+1 < 10)
                    printf("  %d ", j+1);
                else if (j+1 < 100)
                    printf(" %d ", j+1);
                else
                    printf("%d ", j+1);
                puts(buf);
            }
            printf("- - - - - - - - - -\n");
            exit(0);
        }

        wpid = wait(&state);
        if (fileFlag == 0) {
            if (strcmp(ffName, "") != 0) {
                printf("%s\n", ffName);
                printf("- - - - - - - - - -\n");
                print_file(ffName);
                printf("- - - - - - - - - -\n");
            }
            printf("%s\n", cfName);
        }
        else
            printf("%s\n", ffName);
        printf("- - - - - - - - - -\n");
        if (fileFlag == 0)
            print_file(cfName);
        else
            print_file(ffName);
        sleep(1);
    }

    return;
}

void do_pOption(void)   // 사용된 라이브러리 함수를 출력하는 함수이다.
{
    printf("\n*Converting Library Function\n");
    for (int i = 0; i < FUNCTION_SIZE; i++) {
        if (saved_p[i] == true)
            printf("  %s) -> %s()\n", java_function[i], c_function[i]);
    }
}

void do_fOption(void)  // file의 크기를 출력하는 함수이다. 
{
    int size;
    printf("\n*File Size\n");
    //  .java 파일 크기 출력
    if ((jfp = fopen(jfName, "r")) == NULL) {
        fprintf(stderr, "open error for %s in f option\n", jfName);
        exit(1);
    }
    fseek(jfp, 0, SEEK_END);
    size = ftell(jfp);
    printf("  %s file size is %d bytes\n", jfName, size);
    fclose(jfp);
    
    //  class 파일 크기 출력 
    if (strcmp(ffName, "") != 0) {
        if ((ffp = fopen(ffName, "r")) == NULL) {
            fprintf(stderr, "open error for %s in f option\n", ffName);
            exit(1);
        }
        fseek(ffp, 0, SEEK_END);
        size = ftell(ffp);
        printf("  %s file size is %d bytes\n", ffName, size);
        fclose(ffp);
    }

    //  .c 파일 크기 출력
    if ((cfp = fopen(cfName, "r")) == NULL) {
        fprintf(stderr, "open error for %s in f option\n", cfName);
        exit(1);
    }
    fseek(cfp, 0, SEEK_END);
    size = ftell(cfp);
    printf("  %s file size is %d bytes\n", cfName, size);
    fclose(cfp);

    return;
}

void do_lOption(void)  // file의 라인 수를 출력하는 함수이다.
{
    char charbuf[2];
    int cnt = 0;
    printf("\n*File Line Number\n");
    //  .java 라인 수  출력
    if ((jfp = fopen(jfName, "r")) == NULL) {
        fprintf(stderr, "open error for %s in l option\n", jfName);
        exit(1);
    }
    while (fread(charbuf, sizeof(char), 1, jfp) == 1) {
        if (charbuf[0] == '\n')
            cnt++;
    }
    printf("  %s line number is %d lines\n", jfName, cnt);
    fclose(jfp);

    //  class 라인 수 출력
    cnt = 0;
    if (strcmp(ffName, "") != 0) {
        if ((ffp = fopen(ffName, "r")) == NULL) {
            fprintf(stderr, "open error for %s in l option\n", ffName);
            exit(1);
        }
        while (fread(charbuf, sizeof(char), 1, ffp) == 1) {
            if (charbuf[0] == '\n')
                cnt++;
        }
        printf("  %s line number is %d lines\n", ffName, cnt);
        fclose(ffp);
    }

    //  .c 라인 수  출력
    cnt = 0;
    if ((cfp = fopen(cfName, "r")) == NULL) {
        fprintf(stderr, "open error for %s in l option\n", cfName);
        exit(1);
    }
    while (fread(charbuf, sizeof(char), 1, cfp) == 1) {
        if (charbuf[0] == '\n')
            cnt++;
    }
    printf("  %s line number is %d lines\n", cfName, cnt);
    fclose(cfp);

    return;
}

// 파일명_Makefile을 생성하는 함수이다. 
void make_Makefile(void) 
{
    int i;
    char line[BUFLEN];

    // 파일생성
    strcpy(mfName, make[0]);
    strcat(mfName, "_Makefile\0");
    if ((mfp = fopen(mfName, "w")) == NULL) {
        fprintf(stderr, "open error for %s\n", mfName);
        exit(1);
    }

    // 목적파일 만드는 매크로 만들기  
    strcpy(line, make[0]);
    strcat(line, " : ");
    for (i = 0; i < makeNum; i++) {
        strcat(line, make[i]);
        strcat(line, ".o ");
    }
    strcat(line, "\n");
    fwrite(line, strlen(line), 1, mfp);
    strcpy(line, "\tgcc ");
    for (i = 0; i < makeNum; i++) {
        strcat(line, make[i]);
        strcat(line, ".o ");
    }
    strcat(line, "-o ");
    strcat(line, make[0]);
    strcat(line, "\n\n");
    fwrite(line, strlen(line), 1, mfp);

    // .o 파일 만드는 매크로 만들기 
    for (i = 0; i < makeNum; i++) {
        strcpy(line, make[i]);
        strcat(line, ".o : ");
        strcat(line, make[i]);
        strcat(line, ".c\n");
        fwrite(line, strlen(line), 1, mfp);
        strcpy(line, "\tgcc -c ");
        strcat(line, make[i]);
        strcat(line, ".c\n\n");
        fwrite(line, strlen(line), 1, mfp);
    }

    // clean 하는 매크로 만들기 
    strcpy(line, "clean :\n\trm *.o\n\trm ");
    strcat(line, make[0]);
    strcat(line, "\n");
    fwrite(line, strlen(line), 1, mfp);

    // 파일 닫기 
    fclose(mfp);

    return;
}
