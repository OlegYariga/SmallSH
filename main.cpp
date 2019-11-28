// SmallSH.cpp: определяет точку входа для консольного приложения.
// ЗДЕСЬ НАПИСАТЬ ОПИСАНИЕ ПРОЕКТА
// описание проекта
// project description
//
/*
*
* Подключаем необходимые для работы заголовочные файлы
*
*/
#include <stdio.h>
#include <stdlib.h>
#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

using namespace std;
/*
*
* Блок для define-директив. Определяют идентификаторы и последовательности символов
* которыми будут замещаться данные идентификаторы
*
*/
#define SMALLSH_RL_BUFSIZE 1024
#define EXIT_FAILURE 1
#define SMALLSH_TOK_BUFSIZE 64
#define SMALLSH_TOK_DELIM " \t\r\n\a"
/*
*
* Раздел объявления прототипов
*
*/
// command_loop - основной цикл исполнения команд. Запускатеся после
// загрузки конфигурационных файлов при старте SmallSH
int command_loop();
// Функция, считывающая введенную строку
char* command_read_line();
// Разбиение строки на массив введенных значений
char** command_split_line(char* line);
// Выполнение команды с её параметрами
int command_execute(char** args);
/*
*
* Точка входа для консольного приложения
*
*/
int main(int argc, char **argv)
{
    // Загрузка файлов конфигурации при их наличии.

    // Запуск цикла команд.
    command_loop();

    // Выключение / очистка памяти.

    return 0;
}

/*
*
*Функция, производящая обработку команд
*
*/
int command_loop() {
    char *line;
    char **args;
    int status;

    do {
        // выводим приглашение командной строки
        printf("> ");
        // считываем введенную строку
        line = command_read_line();
        // разделяем строку на команду и её параметры
        args = command_split_line(line);
        // выполняем команду с параметрами
        status = command_execute(args);
        printf("%d\n", status);
        free(line);
        free(args);
    } while (status);
    return 0;
}
/*
*
* Считываем символы, пока не встретим EOF или символ переноса строки \n
*
*/
char* command_read_line(){
    char* status = const_cast<char *>("ok");
    int bufsize = SMALLSH_RL_BUFSIZE;
    int position = 0;
    char *buffer = (char*)malloc(sizeof(char) * bufsize);
    int c;
    // Если не удалось выделить необхлдимое количество памяти под буфер
    if (!buffer) {
        fprintf(stderr, "SmallSH: memory allocation error. Exit...\n");
        exit(EXIT_FAILURE);
    }

    while (true) {
        // Читаем символ
        c = getchar();

        // При встрече с EOF заменяем его нуль-терминатором и возвращаем буфер
        if (c == EOF || c == '\n') {
            buffer[position] = '\0';
            return buffer;
        }
        else {
            buffer[position] = c;
        }
        position++;

        // Если мы превысили буфер, перераспределяем блок памяти
        if (position >= bufsize) {
            // увеличиваем блок памяти на длинну стандартного блока
            bufsize += SMALLSH_RL_BUFSIZE;
            buffer = (char*)realloc(buffer, bufsize);
            // Если не удалось выделить необхлдимое количество памяти под буфер
            if (!buffer) {
                fprintf(stderr, "SmallSH: memory allocation error. Exit...\n");
                exit(EXIT_FAILURE);
            }
        }
    }
    return status;
}
/*
*
* Разделяет введенную строку на аргументы по символам, определенным в SMALLSH_TOK_DELIM
*
*/
char** command_split_line(char* line){
    int bufsize = SMALLSH_TOK_BUFSIZE, position = 0;
    char **tokens = (char**)malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "SmallSH: memory allocation error. Exit...\n");
        exit(EXIT_FAILURE);
    }

    token = strtok(line, SMALLSH_TOK_DELIM);
    while (token != NULL) {
        tokens[position] = token;
        position++;

        if (position >= bufsize) {
            bufsize += SMALLSH_TOK_BUFSIZE;
            tokens = (char**)realloc(tokens, bufsize * sizeof(char*));
            if (!tokens) {
                fprintf(stderr, "SmallSH: memory allocation error. Exit...\n");
                exit(EXIT_FAILURE);
            }
        }

        token = strtok(NULL, SMALLSH_TOK_DELIM);
    }
    tokens[position] = NULL;
    return tokens;
}

int command_execute(char** args) {
    return true;
}

int lsh_launch(char **args)
{
    pid_t pid, wpid;
    int status;

    pid = fork();
    if (pid == 0) {
        // Дочерний процесс
        if (execvp(args[0], args) == -1) {
            perror("lsh");
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0) {
        // Ошибка при форкинге
        perror("lsh");
    }
    else {
        // Родительский процесс
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    return 1;
}