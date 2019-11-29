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
#include <sys/types.h>
#include <dirent.h>
#include <map>
#include <string>
#include <list>
#include <algorithm>

using namespace std;
/*
*
* Блок для define-директив. Определяют идентификаторы и последовательности символов
* которыми будут замещаться данные идентификаторы
*
*/
#define SMALLSH_RL_BUFSIZE 1024
#define SMALLSH_TOK_BUFSIZE 64
#define SMALLSH_TOK_DELIM " \t\r\n\a"
#define MAXDIR 1024
#define NAME variables
/*
*
* Раздел объявления прототипов
*
*/
// command_loop - основной цикл исполнения команд
int    command_loop();
// Функция, считывающая введенную строку
char*  command_read_line();
// Разбиение строки на массив введенных значений
char** command_split_line(char* line);
// Выполнение команды с её параметрами
int    command_execute(char** args);
// Выполнение внешних команд
int    command_launch(char **args);
/*
 *
 * Объявление функций для встроенных команд оболочки:
 *
*/
// Команда change directory - смена каталога
int smallsh_cd(char **args);
// Команда pwd - полный путь к текущему расположению
int smallsh_pwd(char **args);
// Команда kill(PID) - принудительное завершение процесса
int smallsh_kill(char **args);
// Команда declare - список переменных
int smallsh_declare(char **args);
// Список команд
int smallsh_help(char **args);
// Выход из интерпретатора
int smallsh_exit(char **args);
/*
 *
 * Список встроенных команд, за которыми следуют соответствующие функции
 *
 */
char *builtin_str[] = {
        "cd",
        "help",
        "exit",
        "pwd",
        "kill",
        "declare"
};
int (*builtin_func[]) (char **) = {
        &smallsh_cd,
        &smallsh_help,
        &smallsh_exit,
        &smallsh_pwd,
        &smallsh_kill,
        &smallsh_declare,
};
// хранит команды для declare
std::multimap<string, string> Map;
std::list<string> Key;
std::list<string> Val;
// Список встроенных команд
int smallsh_num_builtins() {
        return sizeof(builtin_str) / sizeof(char *);
}
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
    // Если не удалось выделить необходимое количество памяти под буфер
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
/*
 *
 * Выполнение команд, введенных пользователем. Сначала производит поиск встроенных команд,
 * а если команды нет в списке встроенных - выполняет соответствующую программу
 *
 */
int command_execute(char** args) {
    int i;

    if (args[0] == NULL) {
        // Была введена пустая команда.
        return 1;
    }

    for (i = 0; i < smallsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    return command_launch(args);
}

int command_launch(char **args)
{
    pid_t pid, wpid;
    int status;
    // форк текущего процесса
    pid = fork();
    if (pid == 0) {
        // Выполняем дочерний процесс, передавая ему команду с параметрами
        if (execvp(args[0], args) == -1) {
            // Выводим информацию об ошибке
            perror("SmallSH");
        }
        // Завершаем дочерний процесс
        exit(EXIT_FAILURE);
    }
    else if (pid < 0) {
        // Ошибка при форкинге
        perror("SmallSH");
    }
    else {
        // Родительский процесс
        do {
            // ожидаем возврата ответа от дочернего процесса
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 1;
}
/*
 *
 * Реализации встроенных в интепретатор функций
 *
*/
int smallsh_cd(char **args)
{
    if (args[1] == NULL) {
        fprintf(stderr, "SmallSH: ожидается аргумент для \"cd\"\n");
        return 1;
    } else {
        if (chdir(args[1]) != 0) {
            perror("SmallSH");
        }
    }
    return 1;
}

int smallsh_pwd(char **args)
{
    char dir[MAXDIR];
    getcwd(dir, MAXDIR);
    printf("%s%s", dir, "\n");
    return 1;
}

int smallsh_kill(char **args)
{
    pid_t pid = atoi(args[1]);
    if (args[1] == NULL){
        fprintf(stderr, "SmallSH: для завершения работы оболочки используйте exit\n");
        return 1;
    }
    if (pid == 0) {
        fprintf(stderr, "SmallSH: необходимо ввести pid процесса\n");
    } else {
        if (kill(pid, SIGTERM) != 0) {
            perror("SmallSH");
        }
    }
    return 1;
}

int smallsh_declare(char **args)
{
    return 1;
}
int smallsh_help(char **args)
{
    int i;
    printf("SmallSH by Oleg Yariga\n");
    printf("Type command and press return\n");
    printf("Builtin commands list:\n");

    for (i = 0; i < smallsh_num_builtins(); i++) {
        printf("  %s\n", builtin_str[i]);
    }

    printf("Use man to get information about other commands\n");
    return 1;
}

int smallsh_exit(char **args)
{
    return 0;
}