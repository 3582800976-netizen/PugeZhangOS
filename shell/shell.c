#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <limits.h>

#define MAX_CMD 1024
#define MAX_ARG 64

int main()
{
    char command[MAX_CMD];

    while (1)
    {
        // 显示当前路径
        char cwd[PATH_MAX];

        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("Shell:%s$ ", cwd);
        }
        else
        {
            printf("Shell$ ");
        }

        fflush(stdout);

        // 读取用户输入
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            break;
        }

        // 去掉最后的换行
        command[strcspn(command, "\n")] = '\0';

        // 如果什么都没有输入，就重新显示提示符
        if (strlen(command) == 0)
        {
            continue;
        }

        // 把输入的命令拆成多个参数
        char *argv[MAX_ARG];
        int argc = 0;

        char *token = strtok(command, " \t");

        while (token != NULL && argc < MAX_ARG - 1)
        {
            argv[argc] = token;
            argc++;

            token = strtok(NULL, " \t");
        }

        argv[argc] = NULL;

        // exit：退出 Shell
        if (strcmp(argv[0], "exit") == 0)
        {
            printf("Bye!\n");
            break;
        }

        // cd：改变 Shell 自己的当前目录
        if (strcmp(argv[0], "cd") == 0)
        {
            if (argc < 2)
            {
                printf("cd: missing argument\n");
            }
            else if (chdir(argv[1]) != 0)
            {
                perror("cd");
            }

            continue;
        }

        // 记录程序开始运行的时间
        struct timeval start, end;
        gettimeofday(&start, NULL);

        // 创建子进程
        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");
            continue;
        }

        if (pid == 0)
        {
            // 子进程：执行用户输入的命令
            execvp(argv[0], argv);

            // exec 执行失败才会运行到这里
            perror("exec");
            exit(127);
        }
        else
        {
            // 父进程：等待子进程结束
            int status;

            waitpid(pid, &status, 0);

            // 记录程序结束的时间
            gettimeofday(&end, NULL);

            long time =
                (end.tv_sec - start.tv_sec) * 1000 +
                (end.tv_usec - start.tv_usec) / 1000;

            // 输出程序运行结果
            if (WIFEXITED(status))
            {
                printf("[pid=%d, exit=%d, time=%ldms]\n",
                       pid,
                       WEXITSTATUS(status),
                       time);
            }
            else
            {
                printf("[pid=%d, terminated, time=%ldms]\n",
                       pid,
                       time);
            }
        }
    }

    return 0;
}
