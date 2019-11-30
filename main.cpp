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
#define SMALLSH_TOK_PARSE ";"
#define MAXDIR 1024
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
char** command_split_line(char* line, char* TOKEN);
// Выполнение команды с её параметрами
int    command_execute(char** args);
// Выполнение внешних команд
int    command_launch(char **args);
// Экранирование специальных символов
string command_shield(char *line);
// разэкранирование символов перед выполнением команды
char** command_unshield(char**line);
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
        "lpwd",
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
    char **subline;
    int count = 0;
    string parseline, prs;
    int bufsize = SMALLSH_TOK_BUFSIZE;
    char **parseback = (char**)malloc(bufsize * sizeof(char*));

    do {
        // выводим приглашение командной строки
        printf("> ");
        // считываем введенную строку
        line = command_read_line();
        parseline = command_shield(line);
        // разделяем строку на команды между ;
        subline = command_split_line((char*)parseline.c_str(), SMALLSH_TOK_PARSE);
        while (subline[count] != NULL) {
            // разделяем строку на команду и её параметры
            args = command_split_line(subline[count], SMALLSH_TOK_DELIM);
            // производим обратное преобразование спецсимволов
            args = command_unshield(args);
            // выполняем команду с параметрами - ЗДЕСЬ И УЗНАЮ, ЧТО ВСЕ ЭЛЕМЕНТЫ args стали равны последнему значению
            status = command_execute(args);
            count++;
        }
        count = 0;
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
* Разделяет введенную строку на аргументы по символам, определенным в SMALLSH_TOK_DELIM или SMALLSH_TOK_PARSE
*
*/
char** command_split_line(char* line, char* TOKEN){
    // определяем размер буфера для команд
    int bufsize = SMALLSH_TOK_BUFSIZE, position = 0;
    // выделяем память под буфер
    char **tokens = (char**)malloc(bufsize * sizeof(char*));
    char *token;

    if (!tokens) {
        fprintf(stderr, "SmallSH: memory allocation error. Exit...\n");
        exit(EXIT_FAILURE);
    }
    // разделяем строку на параметры, по разделителю token (SMALLSH_TOK_DELIM или SMALLSH_TOK_PARSE)
    token = strtok(line, TOKEN);
    while (token != NULL) {
        tokens[position] = token;
        position++;
        // если выделенного буфера не хватает, то выделяем доп. память равную размеру буфера
        if (position >= bufsize) {
            bufsize += SMALLSH_TOK_BUFSIZE;
            tokens = (char**)realloc(tokens, bufsize * sizeof(char*));
            // выводим ошибку выделения памяти
            if (!tokens) {
                fprintf(stderr, "SmallSH: memory allocation error. Exit...\n");
                exit(EXIT_FAILURE);
            }
        }
        // идем к следующему разделителю
        token = strtok(NULL, TOKEN);
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
    // читаем список встроенных команд и ищем там введенную команду
    for (i = 0; i < smallsh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            // если нашли, выполняем соотв. функцию и возвращаем результат
            return (*builtin_func[i])(args);
        }
    }
    // если введенная команда не входит в список встроенных - выполняем её
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
 * Экранирование специальных символов путем замены их на другие последовательности
 *
 */
string command_shield(char *line)
{
    std::string a = line;
    std::string b = "\\ ";
    std::string c = "@space@";
    std::string::size_type ind;
    while((ind=a.find(b))!=std::string::npos) a.replace(ind, b.size(), c);
    return a;
}
/*
 *
 * Замена экранирующих последовательностей перед выполнением команды
 *
 */
char** command_unshield(char **line)
{
    int it = 0;
    while (line[it] != NULL) {
        std::string a = line[it];
        std::string b = "@space@";
        std::string c = " ";
        std::string::size_type ind;
        while ((ind = a.find(b)) != std::string::npos) a.replace(ind, b.size(), c);
        strcpy( line[it], a.c_str() );
        it++;
    }

    return line;
}
/*
 *
 * Реализации встроенных в интепретатор функций
 *
*/
int smallsh_cd(char **args)
{
    if (args[1] == NULL) {
        // если не введен аргумент для CD - выдаем сообщение об ошибке
        fprintf(stderr, "SmallSH: ожидается аргумент для \"cd\"\n");
        return 1;
    } else {
        // выполняем переход в указанный каталог
        if (chdir(args[1]) != 0) {
            perror("SmallSH");
        }
    }
    return 1;
}

int smallsh_pwd(char **args)
{
    // определяем буфер для результата команды pwd
    char dir[MAXDIR];
    // выполняем pwd
    getcwd(dir, MAXDIR);
    // выводим результат выполнения команды
    printf("%s%s", dir, "\n");
    return 1;
}

int smallsh_kill(char **args)
{
    // преобразование pid процесса в int
    pid_t pid = atoi(args[1]);
    if (args[1] == NULL){
        fprintf(stderr, "SmallSH: для завершения работы оболочки используйте exit\n");
        return 1;
    }
    if (pid == 0) {
        // если pid не введен, то выдаем сообщение об ошибке
        fprintf(stderr, "SmallSH: необходимо ввести pid процесса\n");
    } else {
        // посылаем процессу сигнал "мягкого" завершения
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
    printf("Commands examples:\n");
    printf("> cd ..\n");
    printf("> pwd\n");
    printf("/home/user/Рабочий стол/SmallSH\n");
    printf("> cd cmake-build-debug; pwd; ls -a; exit\n");
    printf("/home/user/Рабочий стол/SmallSH/cmake-build-debug\n");
    printf(".   CMakeCache.txt  cmake_install.cmake  SmallSH      variables\n");
    printf("..  CMakeFiles\t    Makefile\t\t SmallSH.cbp\n");
    printf("Use man to get information about other commands\n");
    return 1;
}

int smallsh_exit(char **args)
{
    return 0;
}