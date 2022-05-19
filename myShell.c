#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<string.h>
#include<dirent.h>
#include<pwd.h>
#include<wait.h>
#include<sys/types.h>
#include <readline/readline.h>
#include <readline/history.h>


//define the printf color
#define L_GREEN "\e[1;32m"
#define L_BLUE  "\e[1;34m"
#define L_RED   "\e[1;31m"
#define WHITE   "\e[0m"

#define TRUE 1
#define FALSE 0
#define DEBUG

char lastdir[100];
char command[BUFSIZ];
char argv[100][100];
char** argvtmp1;
char** argvtmp2;
char argv_redirect[100];
int  argc;
int BUILTIN_COMMAND = 0;
int PIPE_COMMAND = 0;
int REDIRECT_COMMAND = 0;
//set the prompt
void set_prompt(char* prompt);
//analysis the command that user input
int analysis_command();
void builtin_command();
void do_command();
//print help information
void help();
void initial();
void init_lastdir();
void history_setup();
void history_finish();
void display_history_list();

int main() {
	char prompt[BUFSIZ];
	char* line;

	init_lastdir();
	history_setup();
	while(1) {
		// show prompt
		set_prompt(prompt);
		if(!(line = readline(prompt)))
			break;
		if(*line)
			add_history(line);
		strcpy(command, line);
		if(!(analysis_command())) {
			if(BUILTIN_COMMAND) {
				builtin_command();
			} else {
				do_command();
			}
		}
		initial();
	}
	history_finish();
	return 0;
}

//set the prompt
void set_prompt(char* prompt) {
	char hostname[100], cwd[100], cwdcopy[100];
	char super = '#';
	char delims[] = "/";
	if(gethostname(hostname, sizeof(hostname)) == -1) strcpy(hostname, "unknown");
	if(!(getcwd(cwd, sizeof(cwd)))) strcpy(cwd, "unknown");
	struct passwd* pwp = getpwuid(getuid());
	// copy一下cwd 用以进行一个strtok分割
	strcpy(cwdcopy, cwd);
	// 用'/'分割得到的第一个元素
	char* firstSlice = strtok(cwdcopy, delims);
	// 用'/'分割得到的第二个元素 其实就是用户名 这里用分割直接获取
	char* secondSlice = strtok(NULL, delims);
	// /home/* -> ~
	// 字符串的切割和处理 这里用 /home/* -> ~即可
	if(!(strcmp(firstSlice, "home")) && !(strcmp(secondSlice, pwp->pw_name))) {
		// 当时进入到/home/*的路径下进行一个处理
		int offset = strlen(firstSlice) + strlen(secondSlice) + 2;
		// 给一个buffer 进行存储
		char newcwd[100], tmp[100];
		char* p = cwd + offset, * q = newcwd;
		// 将offset后面的字符串进行一个复制
		while(*(q++) = *(p++));
		// 将复制好的字符串和~进行一个拼接
		// 然后再strcpy给cwd得到最后的一个文件的路径
		strcpy(tmp, "~");
		strcat(tmp, newcwd);
		strcpy(cwd, tmp);
	}
	// 判断当前用户的级别
	if(getuid() == 0) super = '#';
	else super = '$';
	sprintf(prompt, "\001"L_GREEN"\002%s@%s\001"WHITE"\002:\001"L_RED"\002%s\001"WHITE"\002%c", pwp->pw_name, hostname, cwd, super);
}

//analysis command that user input
int analysis_command() {
	int i = 1;
	char* p;
	//to cut the cwd by " "	
	char delims[] = " ";
	argc = 1;
	strcpy(argv[0], strtok(command, delims));
	while(p = strtok(NULL, delims)) {
		strcpy(argv[i++], p);
		argc++;
	}
	if(!(strcmp(argv[0], "exit")) || !(strcmp(argv[0], "help")) || !(strcmp(argv[0], "cd"))) {
		// 打个标记
		BUILTIN_COMMAND = 1;
	}
	int j;
	// 是否为管道指令
	int pipe_location;
	for(j = 0;j < argc;j++) {
		// 通过 | 来判断是否是管道 并且去标记管道的位置
		if(strcmp(argv[j], "|") == 0) {
			PIPE_COMMAND = 1;
			pipe_location = j;
			break;
		}
	}
	// 是否为输出重定位指令
	int redirect_location;
	for(j = 0;j < argc;j++) {
		// 根据 > 来判断是否为输出重定向 然后标记输出重定向的位置
		if(strcmp(argv[j], ">") == 0) {
			REDIRECT_COMMAND = 1;
			redirect_location = j;
			break;
		}
	}
	if(PIPE_COMMAND) {
		// pipe left
		argvtmp1 = malloc(sizeof(char*) * pipe_location + 1);
		int i;
		for(i = 0;i < pipe_location + 1;i++) {
			argvtmp1[i] = malloc(sizeof(char) * 100);
			// 将 argv | left -> argvtmp1
			if(i <= pipe_location) strcpy(argvtmp1[i], argv[i]);
		}
		// end
		argvtmp1[pipe_location] = NULL;
		// pipe right
		argvtmp2 = malloc(sizeof(char*) * (argc - pipe_location));
		int j;
		for(j = 0;j < argc - pipe_location;j++) {
			argvtmp2[j] = malloc(sizeof(char) * 100);
			// 将 argv | right -> argvtmp2
			if(j <= pipe_location) strcpy(argvtmp2[j], argv[pipe_location + 1 + j]);
		}
		// end
		argvtmp2[argc - pipe_location - 1] = NULL;
	} else if(REDIRECT_COMMAND) {
		// if redirecte
		strcpy(argv_redirect, argv[redirect_location + 1]);
		argvtmp1 = malloc(sizeof(char*) * redirect_location + 1);
		for(int i = 0;i < redirect_location + 1;i++) {
			argvtmp1[i] = malloc(sizeof(char) * 100);
			// argv > left -> argvtmp1
			if(i < redirect_location) strcpy(argvtmp1[i], argv[i]);
		}
		// end
		argvtmp1[redirect_location] = NULL;
	} else {
		argvtmp1 = malloc(sizeof(char*) * argc + 1);
		for(int i = 0;i < argc + 1;i++) {
			argvtmp1[i] = malloc(sizeof(char) * 100);
			// argv -> argvtmp1
			if(i < argc) strcpy(argvtmp1[i], argv[i]);
		}
		// end
		argvtmp1[argc] = NULL;
	}
	return 0;
}

// 简单内部指令
void builtin_command() {
	struct passwd* pwp;
	if(strcmp(argv[0], "exit") == 0) {
		// exit 
		exit(EXIT_SUCCESS);
	} else if(strcmp(argv[0], "help") == 0) {
		// help
		help();
	} else if(strcmp(argv[0], "cd") == 0) {
		// cd func
		char cd_path[100];
		if((strlen(argv[1])) == 0) {
			// 执行cd指令 回到home/username目录
			pwp = getpwuid(getuid());
			sprintf(cd_path, "/home/%s", pwp->pw_name);
			strcpy(argv[1], cd_path);
			argc++;
		} else if((strcmp(argv[1], "~") == 0)) {
			// 执行cd ~ 指令 同样回到home/username
			pwp = getpwuid(getuid());
			sprintf(cd_path, "/home/%s", pwp->pw_name);
			strcpy(argv[1], cd_path);
		}
		if((chdir(argv[1])) < 0) {
			printf("cd failed in builtin_command()\n");
		}
	}
}
void do_command() {
	if(PIPE_COMMAND) {
		int fd[2], res, status;
		res = pipe(fd);
		if(res == -1) printf("pipe failed in do_command()\n");
		pid_t pid1 = fork();
		if(pid1 == -1) {
			printf("fork failed in do_command()\n");
		} else if(pid1 == 0) {
			// child process
			dup2(fd[1], 1); // dup the stdout 将std描述符到管道写端
			close(fd[0]); // close the read edge 关闭fd[0]文件读端
			if(execvp(argvtmp1[0], argvtmp1) < 0) printf("%s:command not found\n", argvtmp1[0]);
		} else {
			// fa process
			// wait for child prcocess to end
			waitpid(pid1, &status, 0);
			// create a new child process to recieve the result from the first constructsion
			pid_t pid2 = fork();
			if(pid2 == -1) {
				printf("fork failed in do_command()\n");
			} else if(pid2 == 0) {
				close(fd[1]); //close write edge
				dup2(fd[0], 0); // 复制stdin描述道管道读端
				if(execvp(argvtmp2[0], argvtmp2) < 0) printf("%s:command not found\n", argvtmp2[0]);
			} else {
				// fa procress close the construction
				close(fd[0]);
				close(fd[1]);
				waitpid(pid2, &status, 0);
			}
		}
	} else if(REDIRECT_COMMAND) {
		pid_t pid = fork();
		if(pid == -1) {
			printf("fork failed in do_command()\n");
		} else if(pid == 0) {
			// child process
			int redirect_flag = 0;
			// fopen打开一个输入流
			FILE* fstream = fopen(argv_redirect, "w+");
			// 将stdout输入到到目标
			freopen(argv_redirect, "w", stdout);
			if(execvp(argvtmp1[0], argvtmp1) < 0) {
				redirect_flag = 1; //execvp this redirect command failed		
			}
			// close stdout
			fclose(stdout);
			// close fstream即可
			fclose(fstream);
			if(redirect_flag) {
				printf("%s:command not found\n", argvtmp1[0]);
			}
		} else {
			// pa process wait the children process to stop
			int pidReturn = wait(NULL);
		}
	} else {
		//else normal command
		pid_t pid = fork();
		if(pid == -1) {
			printf("fork failed in do_command()\n");
		} else if(pid == 0) {
			if(execvp(argvtmp1[0], argvtmp1) < 0) {
				printf("%s:command not found\n", argvtmp1[0]);
			}
		} else {
			int pidReturn = wait(NULL);
		}
	}
	free(argvtmp1);
	free(argvtmp2);
}

void help() {
	char message[50] = "Hi,welcome to myShell!";
	printf("< %s >\n", message);
	printf("This is Made by chenJiongLin, Jonycchen\n");
}

// 初始化
void initial() {
	int i = 0;
	for(i = 0;i < argc;i++) {
		strcpy(argv[i], "\0");
	}
	argc = 0;
	BUILTIN_COMMAND = 0;
	PIPE_COMMAND = 0;
	REDIRECT_COMMAND = 0;
}

void init_lastdir() {
	getcwd(lastdir, sizeof(lastdir));
}

void history_setup() {
	using_history();
	stifle_history(50);
	read_history("/tmp/msh_history");
}

void history_finish() {
	append_history(history_length, "/tmp/msh_history");
	history_truncate_file("/tmp/msh_history", history_max_entries);
}

void display_history_list() {
	HIST_ENTRY** h = history_list();
	if(h) {
		int i = 0;
		while(h[i]) {
			printf("%d: %s\n", i, h[i]->line);
			i++;
		}
	}
}

