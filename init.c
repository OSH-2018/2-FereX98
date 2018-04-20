#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>

int main() {
    /* 输入的命令行 */
    char cmd[256];
    /* 命令行拆解成的各部分，以空指针结尾 */
    char *args[128]; /*指针数组，指向指令每个空格后的第一个字符*/
    int save_pipefd[2];
    while (1) {
        /* 提示符 */
        printf("# ");
        fflush(stdin);
        fgets(cmd, 256, stdin);
        /* 清理结尾的换行符 + 去除命令行中多余的空格*/
        int i, j;
        for (i = 0, j = 0; cmd[i] != '\n'; i++){
            cmd[i - j] = cmd[i];
            if(cmd[i] == ' ' && cmd[i + 1] == ' ') j++;
        }
        cmd[i - j] = '\0';

        /* 拆解命令行 */
        args[0] = cmd;
        for (i = 0; *args[i]; i++)
            for (args[i+1] = args[i] + 1; *args[i+1]; args[i+1]++)
                if (*args[i+1] == ' ') {
                    *args[i+1] = '\0';
                    args[i+1]++;
                    break;
                }
        args[i] = NULL;

        /* 没有输入命令 */
        if (!args[0])
            continue;
        
        /* 管道处理操作需要在这里完成 */
        pid_t pipe_pid;
        int loop_cnt = 0;
        int pipefd[2];
        int goback = 0;
        save_pipefd[0] = dup(0);
        save_pipefd[1] = dup(1);
        if(save_pipefd[0] == -1 || save_pipefd[1] == -1){
            perror("save");
            exit(0);
        }
        while(1){
            for(i = 0;args[i] && strcmp(args[i], "|");i ++);
            /* 找到第一个‘|’ */
            if(!args[i]){
                break;
            }
            if(pipe(pipefd) == -1){
                perror("pipe");
                exit(0);
            }
            pipe_pid = fork();
            if(pipe_pid == -1){
                perror("fork");
                exit(0);
            }
            if(pipe_pid == 0){
                /* 管道前进程 */
                args[i] = NULL;
                close(pipefd[0]);
                dup2(pipefd[1], 1); /* short for stdout == 1 */
                close(pipefd[1]);
                goback = 1;
                break;
            } 
            /* 管道后进程 */
            /* 清除前进程部分的指令指针 */
            /* 笨重的实现…… 待优化 */
            for(j = i + 1;args[j];j ++){
                args[j - i - 1] = args[j];
            }
            args[j - i - 1] = NULL;
            close(pipefd[1]);
            dup2(pipefd[0], 0); /* short for stdin == 0 */
            close(pipefd[0]);
            wait(NULL);
            loop_cnt ++;
        }

        /* 内建命令 */
        if (strcmp(args[0], "cd") == 0) {
            if (args[1])
                chdir(args[1]);
            if(goback) exit(0);
            continue;
        }
        if (strcmp(args[0], "pwd") == 0) {
            char wd[4096];
            puts(getcwd(wd, 4096));
            if(goback) exit(0);
            continue;
        }
        if (strcmp(args[0], "export") == 0) {
            char *pos = strchr(args[1], '=');
            if(pos == NULL){
                printf("invalid input!\n");
                if(goback) exit(0);
                continue;
            }
            *pos = '\0';
            setenv(args[1], pos + 1, 0);
            if(goback) exit(0);
            continue;
        }
        if (strcmp(args[0], "exit") == 0)
            return 0;

        /* 外部命令 */
        pid_t pid = fork();
        if (pid == 0) {
            /* 子进程 */
            execvp(args[0], args);
            /* execvp失败 */
            printf("no command found!\n");
            return 255;
        }
        /* 父进程 */
        wait(NULL);
        if(goback) {
            exit(0);
        }
        dup2(save_pipefd[0],0);
        dup2(save_pipefd[1],1);
    }
}
