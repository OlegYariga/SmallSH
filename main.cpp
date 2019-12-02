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
#include <string>
#include <sys/stat.h>
#include <fcntl.h>
#include <map>


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
#define NUM_SHIELD_SYM 10
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
int    command_execute(bool background, int redirection_type, char* redirection_filename, char** args);
// Выполнение внешних команд
int    command_launch(bool background, int redirection_type, char* redirection_filename, char **args);
// Экранирование специальных символов
string command_shield(char *line);
// разэкранирование символов перед выполнением команды
char** command_unshield(char**line);
// удаление коментариев из строки, если # не была экранирована
char*  command_strip_comments(char* line);
// проверяем, есть ли символ, обозначающий background процесс
bool   command_find_background(char* line);
char*  command_strip_background(char* line);
// ищем в строке символы перенаправления
int    command_find_redirection(char* line);
// считываем имя файла для перенаправления
char*  command_get_redirection_filename(int redirection_type, char* line);
// удаляем > или < из строки, чтобы не передавать его исполняемой программе
char*  command_strip_redirection(int redirection_type, char* line);
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
 * Обработчики сигналов
 *
 */
void sigINThandler(int signum);
void sigTSTPhandler(int signum);
/*
 *
 * pid процесса, который выполняется в данный момент в foreground режиме
 *
 */
int CHILD_PID = 0;
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
std::map <char*, char*> Map;

// Количество встроенных команд
int smallsh_num_builtins() {
        return sizeof(builtin_str) / sizeof(char *);
}
/*
 *
 * Массив экранируемых символов и символы для их замены
 *
 */
std::string shield_symbol[NUM_SHIELD_SYM] = {
        "\\ ",
        "\\n",
        "\\$",
        "\\&",
        "\\|",
        "\\#",
        "\\;",
        "\\>",
        "\\<",
        "\\\\"
};
std::string replace_symbol[NUM_SHIELD_SYM] = {
        "%space%",
        "%return%",
        "%bucks%",
        "%and%",
        "%or%",
        "%jail%",
        "%semicolon%",
        "%more%",
        "%less%",
        "%slash%"

};
std::string unshield_symbol[NUM_SHIELD_SYM] = {
        " ",
        "\n",
        "$",
        "&",
        "|",
        "#",
        ";",
        ">",
        "<",
        "\\"
};
/*
*
* Точка входа для консольного приложения
*
*/
int main(int argc, char **argv)
{
    // регистрируем обработчики сигналов
    signal(SIGINT, sigINThandler);
    signal(SIGTSTP, sigTSTPhandler);

    // Запуск цикла команд.
    command_loop();

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
    char* line_no_comments;

    do {
        // выводим приглашение командной строки
        printf("> ");
        // считываем введенную строку
        line = command_read_line();
        // экранируем символы в строке
        parseline = command_shield(line);
        // удаляем все неэкранированные коментарии
        line_no_comments = command_strip_comments((char*)parseline.c_str());
        // разделяем строку на команды между ;
        subline = command_split_line(line_no_comments, SMALLSH_TOK_PARSE);
        while (subline[count] != NULL) {
            // ищем в строке символ & для запуска приложения в фоновом режиме
            bool exec_params = command_find_background(subline[count]);
            // удаляем & из строки, чтобы не передавать его исполняемой программе
            subline[count] = command_strip_background(subline[count]);
            // ищем в строке символы перенаправления
            int redirection_type = command_find_redirection(subline[count]);
            char* redirection_filename = "";
            if (redirection_type != 0){
                // считываем имя файла для перенаправления
                redirection_filename = command_get_redirection_filename(redirection_type, subline[count]);
                // удаляем > или < из строки, чтобы не передавать его исполняемой программе
                subline[count] = command_strip_redirection(redirection_type, subline[count]);
            }
            // разделяем строку на команду и её параметры
            args = command_split_line(subline[count], SMALLSH_TOK_DELIM);
            // производим обратное преобразование спецсимволов
            args = command_unshield(args);
            // выполняем команду с параметрами
            status = command_execute(exec_params,redirection_type,redirection_filename,args);
            count++;
        }
        count = 0;
    } while (status);
    // освобождаем занимаемую память
    free(line);
    free(args);
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
            // проверяем, не экранировал ли пользователь символ переноса строки
            if (c == '\n' && buffer[position-1] == '\\') {
                buffer[position-1] = ' ';
                buffer[position] = ' ';
            }else {
                // ставим терминатор и возвращаем строку
                buffer[position] = '\0';
                return buffer;
            }
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
int command_execute(bool background, int redirection_type, char* redirection_filename, char** args) {
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
    return command_launch(background,redirection_type,redirection_filename,  args);
}
/*
 *
 * Выполнение внешней программы
 *
 */
int command_launch(bool background, int redirection_type, char* redirection_filename, char **args)
{
    pid_t pid, wpid;
    int status;
    // форк текущего процесса
    pid = fork();
    if (pid == 0) {
        // дочерний процесс будет игнорировать сигналы CTRL+C и CTRL+Z поступающие из оболочки
        signal(SIGINT, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        // если перенаправлений ввода-вывода нет
        if (redirection_type == 0) {
            // Выполняем дочерний процесс, передавая ему команду с параметрами
            if (execvp(args[0], args) == -1) {
                // Выводим информацию об ошибке
                perror("SmallSH");
            }
            // Завершаем дочерний процесс
            exit(EXIT_FAILURE);
        }
        // если было обнаружено перенаправление вывода
        if (redirection_type == 1) {
            // создаем новый файл
            int fd = open(redirection_filename, O_WRONLY|O_TRUNC|O_CREAT, 0644);
            if (fd < 0) { perror("open"); abort(); }
            // переназначаем дескриптор файла на stdout
            if (dup2(fd, 1) < 0) { perror("dup2"); abort(); }
            // закрываем файл
            close(fd);
            // Выполняем дочерний процесс, передавая ему команду с параметрами
            if (execvp(args[0], args) == -1) {
                // Выводим информацию об ошибке
                perror("SmallSH");
            }
            // Завершаем дочерний процесс
            exit(EXIT_FAILURE);
        }
        // если было обнаружено перенаправление ввода
        if (redirection_type == -1) {
            // открываем файл на чтение, если такого файла нет - создаем новый
            int fd = open(redirection_filename, O_RDONLY);
            if (fd < 0) { perror("open"); abort(); }
            // переназначаем дескриптор файла на stdin
            if (dup2(fd, 0) < 0) { perror("dup2"); abort(); }
            // Выполняем дочерний процесс, передавая ему команду с параметрами
            if (execvp(args[0], args) == -1) {
                // Выводим информацию об ошибке
                perror("SmallSH");
            }
            // закрываем файла
            close(fd);
            // Завершаем дочерний процесс
            exit(EXIT_FAILURE);
        }
        // для всех background процессов перенаправляем ввод-вывод в /dev/null
        if (background){
            int targetFD = open("/dev/null", O_WRONLY);
            if (dup2(targetFD, STDIN_FILENO) == -1) { fprintf(stderr, "Error redirecting"); exit(1); };
            if (dup2(targetFD, STDOUT_FILENO) == -1) { fprintf(stderr, "Error redirecting"); exit(1); };
        }
    }
    else if (pid < 0) {
        // Ошибка при форкинге
        perror("SmallSH");
    }
    else {
        if (!background) {
            // считываем pid дочернего процесса
            CHILD_PID = pid;
            // Родительский процесс
            do {
                // проверяем, сработал ли обработчик сигнала
                if (CHILD_PID == 0)
                    return 1;
                // ожидаем возврата ответа от дочернего процесса
                wpid = waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }else{
            printf("[PID] %d\n", pid);
        }
    }
    // обнуляем pid дочернего процесса
    CHILD_PID = 0;
    return 1;
}
/*
 *
 * Экранирование специальных символов путем замены их на другие последовательности
 *
 */
string command_shield(char *line)
{
    // считываем строку
    std::string a = line;
    // поизводим поиск символов для замены
    for (int i=0; i<NUM_SHIELD_SYM; i++) {
        std::string b = shield_symbol[i];
        std::string c = replace_symbol[i];
        std::string::size_type ind;
        while ((ind = a.find(b)) != std::string::npos) a.replace(ind, b.size(), c);
    }
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
    // проходим в цикле по всем строкам в массиве
    while (line[it] != NULL) {
        // считываем строку
        std::string a = line[it];
        // производим поиск в массиве замененных символов
        for (int i=0; i<NUM_SHIELD_SYM; i++) {
            std::string b = replace_symbol[i];
            std::string c = unshield_symbol[i];
            std::string::size_type ind;
            while ((ind = a.find(b)) != std::string::npos) a.replace(ind, b.size(), c);
        }
        // заменяем исходную строку на обработанную
        strcpy(line[it], a.c_str());
        it++;
    }
    return line;
}
/*
 *
 * Удаляем всю часть строки после #, если она не была экранирована
 *
 */
char*  command_strip_comments(char* line){
    // если первый символ - решетка, возвращаем пустую строку
    if (line[0]=='#') return "";
    // обрезаем стрку до решетки
    return strtok(line, "#");
}
/*
 *
 * Ищем в строке с командой символ & и удаляем его
 *
 */
bool  command_find_background(char* line){
    std::string str = line;
    // ищем символ & в строке
    int position = str.find('&');
    if (position == -1) return false;
    return true;
}
char*  command_strip_background(char* line){
    // если первый символ - &, возвращаем пустую строку
    if (line[0]=='&') return "";
    // обрезаем стрку до &
    return strtok(line, "&");
}
/*
 *
 * ищем в строке символы перенаправления
 *
 */
int command_find_redirection(char* line){
    std::string str = line;
    // ищем символ > в строке
    if ((int)str.find('>') >= 0) return 1;
    // ищем символ < в строке
    else if ((int)str.find('<') >= 0) return -1;
    // если ничего не нашли - возвращам 0
    else return 0;
}
/*
 *
 * считываем имя файла для перенаправления
 *
 */
char*  command_get_redirection_filename(int redirection_type, char* line){
    char* delim = ">";
    if (redirection_type == 1) delim = ">";
    else delim = "<";
    char *strs = line;
    char *primer = strtok(strs, delim);
    char *other = strtok(0, "");
    return other;
}
/*
 *
 * удаляем > или < из строки, чтобы не передавать его исполняемой программе
 *
 */
char*  command_strip_redirection(int redirection_type, char* line){
    char *delim = ">";
    if (redirection_type == 1) delim = ">";
    else delim = "<";
    // если первый символ - < >, возвращаем пустую строку
    if (line[0]=='<' or line[0]=='>') return "";
    // обрезаем стрку до < или до >
    return strtok(line, delim);
}
/*
 *
 * Обработчики сигналов
 *
 */
void sigINThandler(int signum){
    // если дочерний процесс в данный момент работает
    if (CHILD_PID != 0){
        // посылаем процессу сигнал sigterm
        kill(CHILD_PID, SIGTERM);
        // выводим и обнуляем pid процесса
        printf("Program [%d] terminated\n", CHILD_PID);
        CHILD_PID = 0;
    }
}
void sigTSTPhandler(int signum){
    // если дочерний процесс в данный момент работает
    if (CHILD_PID != 0){
        // посылаем процессу сигнал SIGSTOP
        kill(CHILD_PID, SIGSTOP);
        // выводим и обнуляем pid процесса
        printf("Program [%d] stopped\n", CHILD_PID);
        CHILD_PID = 0;
    }
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
    // итератор по элементам словаря
    std::map<char*, char*>::iterator i;
    // если не введены ключ и значение, то выводим список всех переменных
    if (args[1] == NULL && args[2] == NULL) {
        for (i = Map.begin(); i != Map.end(); i++) {
            printf("%s=%s\n", (*i).first, (*i).second);
        }
        return 1;
    // если введен и ключ и значение - добавляем пару в словарь
    }else if(args[1] != NULL && args[2] != NULL){
        // выделяем память под длину ключа и значения
        char *s_key = new char [1+strlen(args[1])];
        char *s_val = new char [1+strlen(args[2])];
        //копируем элементы из args в выделенную память
        strcpy(s_key, args[1]);
        strcpy(s_val, args[2]);
        //добавляем пару в словарь
        Map.insert(pair<char*, char*>(s_key,s_val));
        return 1;
    // если введе только ключ- выводим сообщение об ошибке
    }else{
        printf("SmallSH: declare usage error. Type 'help' to get more info\n");
        return 1;
    }
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
    printf("> cd .. # You can type comment after #\n");
    printf("> mkdir \\#FOLDER\\#; ls; cd /home/user/Рабочий\\ стол; pwd  # Or you can shield some symbols\n");
    printf("> gnome-calculator # Launch foreign program\n");
    printf("> gnome-calculator & # Launch foreign program in background\n");
    printf("[PID]18536\n");
    printf("> gnome-calculator &; sensible-browser &; ls -la "
           "# Launch foreign programs in background and excec ls -la\n");
    printf("[PID]15322\n");
    printf("[PID]15323\n");
    printf("> echo 'You can redirect command output to file' >message.txt \n");
    printf("> cat<f\n");
    printf("Hub\n");
    printf("Middle\n");
    printf("Angr\n");
    printf("> sort<f\n");
    printf("Angr\n");
    printf("Middle\n");
    printf("Hub\n");
    printf("> declare\n");
    printf("BASHVERSION=1.3.8\n");
    printf("> declare PATH /home/usr/\n");
    printf("BASHVERSION=1.3.8\n");
    printf("PATH=/home/urs/\n");

    printf("> gnome-calculator \n");
    printf("^C # Press CTRL+C to interrupt program\n");
    printf("> gnome-calculator \n");
    printf("^Z # Press CTRL+Z to stop program\n");
    printf("> gnome-calculator & \n");
    printf("^C # CTRL+C and CTRL+Z do not affect to programs in background mode\n");

    printf("\n> clear; \\\n");
    printf("echo 'You can type large commands'; \\\n");
    printf("ls -la >ls-result.txt; \\\n");
    printf("gnome-calculator &; \\\n");
    printf("# use '\\' to shield new-line symbol\n");

    printf("\nUse man to get information about other commands\n");
    return 1;
}

int smallsh_exit(char **args)
{
    return 0;
}