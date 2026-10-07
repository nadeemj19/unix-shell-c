/* group number:
             ___  
  _ __ ___  ( _ ) 
 | '_ ` _ \ / _ \ 
 | | | | | | (_) |
 |_| |_| |_|\___/ 
                  
compile and run code (just copy and paste into terminal then hit enter):

        gcc shell.c -pedantic -Wall -o shell && ./shell

*/

#include <unistd.h>    // fork, execvp
#include <sys/types.h> // pid_t datatype
#include <sys/wait.h>  // wait
#include <errno.h>     // errno
#include <stdio.h>     // printf, fgets
#include <string.h>    // strcmp, strtok, strcat, strcpy
#include <stdlib.h>    // getenv, malloc

#define DELIMITERS " \t|><&;\n"  // characters that split input into tokens
#define MAX_LINE_LEN 512         // maximum characters in a command line
#define MAX_TOKEN_NUM 50         // maximum number of tokens per command
#define HISTORY_SIZE 20          // maximum commands kept in history
#define MAX_ALIASES 10           // maximum number of aliases
#define MAX_ALIAS_LEN 512        // maximum alias command length
#define MAX_SUBSTITUTIONS 10     // stage 9: max alias/history chain depth before cycle error


// --- Global variables ---

// history stored as a circular array of strings
char history[HISTORY_SIZE][MAX_LINE_LEN];
int history_count = 0;

// aliases stored as two parallel arrays: names and their commands
char alias_names[MAX_ALIASES][MAX_LINE_LEN];
char alias_commands[MAX_ALIASES][MAX_LINE_LEN];
int alias_count = 0;


// --- Function declarations ---

void tokenize(char *input, char *tokens[]);
void execute(char *tokens[]);

void getPath(void);
void setPath(char *path);
void changeDir(char *path, char *path2);

int  stringToInt(char *str);
void addHistory(char *command);
void printHistory(FILE *output);
int  isHistoryCommand(char *input);
char *getHistoryCommand(char *input);
void loadHistory(void);
void saveHistory(void);
char *getHistoryFilepath(void);

void addAlias(char *name, char *command);
void removeAlias(char *name);
void printAliases(void);

// stage 9: resolve chains of aliases and history invocations with cycle detection
int resolveCommand(char *input);

void saveAliases(void);
void loadAliases(void);
char *getAliasFilepath(void);


// --- Main ---

int main(void) {
        // save original PATH so it can be restored on exit
        char *og_path = getenv("PATH");

        // set working directory to user home directory
        char *home = getenv("HOME");
        chdir(home);

        loadHistory();
        loadAliases();

        char og_input[MAX_LINE_LEN];
        char input[MAX_LINE_LEN];
        char *tokens[MAX_TOKEN_NUM];

        while (1) {
                printf("shellOS> ");

                // read input; NULL means EOF (ctrl-D)
                if (fgets(og_input, MAX_LINE_LEN, stdin) == NULL) {
                        break;
                }

                // exit command
                if (strcmp(og_input, "exit\n") == 0) {
                        break;
                }

                // copy into working buffer so og_input is never modified
                strcpy(input, og_input);

                // stage 9: resolve aliases and history chains (handles !!, !n, !-n, alias chains)
                // resolveCommand modifies input in place; returns 0 on error (prints its own message)
                if (!resolveCommand(input)) {
                        continue;
                }

                // tokenize the fully resolved command
                tokenize(input, tokens);

                // only process non-empty input
                if (tokens[0] != NULL) {

                        // add original input to history (not history invocations)
                        if (!isHistoryCommand(og_input)) {
                                addHistory(og_input);
                        }

                        // alias management
                        if (strcmp(tokens[0], "alias") == 0) {
                                if (tokens[1] == NULL) {
                                        printAliases();
                                } else {
                                        // join all tokens from index 2 onwards as the aliased command
                                        char command[MAX_LINE_LEN] = "";
                                        for (int i = 2; tokens[i] != NULL; i++) {
                                                strcat(command, tokens[i]);
                                                if (tokens[i + 1] != NULL) strcat(command, " ");
                                        }
                                        addAlias(tokens[1], command);
                                }
                        }
                        else if (strcmp(tokens[0], "unalias") == 0) {
                                if (tokens[1] == NULL) {
                                        printf("ERROR - unalias requires a name\n");
                                } else {
                                        removeAlias(tokens[1]);
                                }
                        }
                        else if (strcmp(tokens[0], "getpath") == 0) {
                                getPath();
                        }
                        else if (strcmp(tokens[0], "setpath") == 0) {
                                setPath(tokens[1]);
                        }
                        else if (strcmp(tokens[0], "cd") == 0) {
                                changeDir(tokens[1], tokens[2]);
                        }
                        else if (strcmp(tokens[0], "history") == 0) {
                                printHistory(stdout);
                        }
                        else {
                                execute(tokens);
                        }
                }
        }

        saveHistory();
        saveAliases();

        // restore original PATH
        setenv("PATH", og_path, 1);
        printf("Original path restored: %s\n", getenv("PATH"));

        return 0;
}


// --- Stage 1: Tokenize ---

/* splits input string into an array of tokens using defined delimiters */
void tokenize(char *input, char *tokens[]) {
        int i = 0;
        char *token = strtok(input, DELIMITERS);
        while (token != NULL && i < MAX_TOKEN_NUM) {
                tokens[i] = token;
                i++;
                token = strtok(NULL, DELIMITERS);
        }
        tokens[i] = NULL;
}


// --- Stage 2: Execute external commands ---

/* forks a child process and runs the given command via execvp */
void execute(char *tokens[]) {
        pid_t p = fork();
        if (p < 0) {
                perror("fork failed");
        } else if (p == 0) {
                // child: replace process image with the command
                execvp(tokens[0], tokens);
                perror(tokens[0]); // only reached if execvp fails
                _exit(1);
        } else {
                // parent: wait for child to finish
                wait(NULL);
        }
}


// --- Stage 3: Path and home directory ---

/* prints the current PATH environment variable */
void getPath(void) {
        char *path = getenv("PATH");
        printf("%s\n", path);
}

/* sets the PATH environment variable to the given path string */
void setPath(char *path) {
        if (path == NULL) {
                printf("ERROR - setpath requires a parameter\n");
        } else {
                setenv("PATH", path, 1);
        }
}


// --- Stage 4: Change directory ---

/* changes the working directory; no arg goes HOME, one arg goes to that path */
void changeDir(char *path, char *path2) {
        if (path != NULL && path2 != NULL) {
                printf("ERROR - cd only takes one argument\n");
        } else if (path == NULL) {
                char *home = getenv("HOME");
                if (chdir(home) == -1) {
                        perror("cd");
                }
        } else {
                if (chdir(path) == -1) {
                        perror("cd");
                }
        }
}


// --- Stage 5: History ---

/* converts a numeric string to an integer (digits only, no stdlib atoi) */
int stringToInt(char *str) {
        int result = 0;
        int i = 0;
        while (str[i] >= '0' && str[i] <= '9') {
                result = result * 10 + (str[i] - '0');
                i++;
        }
        return result;
}

/* adds a command line string to the circular history array */
void addHistory(char *command) {
        int index = history_count % HISTORY_SIZE;
        strcpy(history[index], command);
        history_count++;
}

/* prints the history to the given file (stdout or a file handle) */
void printHistory(FILE *output) {
        int total = (history_count > HISTORY_SIZE) ? HISTORY_SIZE : history_count;
        int start_num = history_count - total + 1;

        for (int i = 0; i < total; i++) {
                int index = (start_num + i - 1) % HISTORY_SIZE;
                fprintf(output, "%d %s", start_num + i, history[index]);
        }
}

/* returns 1 if input starts with '!' (i.e. is a history invocation) */
int isHistoryCommand(char *input) {
        return (input[0] == '!') ? 1 : 0;
}

/*
 * resolves a history invocation string to the actual command string.
 * supports !!, !n, !-n forms.
 * returns NULL and prints an error on invalid invocations.
 */
char *getHistoryCommand(char *input) {
        int target_num;

        if (strcmp(input, "!!\n") == 0 || strcmp(input, "!!") == 0) {
                if (history_count == 0) {
                        printf("Error - no commands in history\n");
                        return NULL;
                }
                target_num = history_count;
        } else if (input[1] == '-') {
                int offset = stringToInt(input + 2);
                target_num = history_count - offset;
        } else {
                target_num = stringToInt(input + 1);
        }

        if (target_num < 1 || target_num > history_count) {
                printf("Error - invalid history number\n");
                return NULL;
        }

        int oldest = history_count - HISTORY_SIZE + 1;
        if (oldest < 1) oldest = 1;

        if (target_num < oldest) {
                printf("Error - command no longer in history\n");
                return NULL;
        }

        int index = (target_num - 1) % HISTORY_SIZE;
        return history[index];
}


// --- Stage 6: Persistent history ---

/* loads history from ~/.hist_list on shell startup */
void loadHistory(void) {
        FILE *file = fopen(getHistoryFilepath(), "r");
        if (file == NULL) return;

        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), file) != NULL) {
                char *command = strchr(line, ' '); // format is "N command"
                if (command != NULL) {
                        command++; // skip the space after the number
                        addHistory(command);
                }
        }
        fclose(file);
}

/* saves current history to ~/.hist_list before shell exits */
void saveHistory(void) {
        FILE *file = fopen(getHistoryFilepath(), "w");
        if (file == NULL) return;
        printHistory(file);
        fclose(file);
}

/* returns the full path to the history file in the user's home directory */
char *getHistoryFilepath(void) {
        static char filepath[MAX_LINE_LEN];
        char *home = getenv("HOME");
        snprintf(filepath, sizeof(filepath), "%s/.hist_list", home);
        return filepath;
}


// --- Stage 7: Aliases ---

/* adds or updates an alias; prints a message if overriding an existing one */
void addAlias(char *name, char *command) {
        // check if alias already exists and update it
        for (int i = 0; i < alias_count; i++) {
                if (strcmp(alias_names[i], name) == 0) {
                        strcpy(alias_commands[i], command);
                        printf("Alias '%s' updated\n", name);
                        return;
                }
        }
        if (alias_count >= MAX_ALIASES) {
                printf("ERROR - maximum number of aliases reached\n");
                return;
        }
        strcpy(alias_names[alias_count], name);
        strcpy(alias_commands[alias_count], command);
        alias_count++;
}

/* removes an alias by name, shifting remaining entries to fill the gap */
void removeAlias(char *name) {
        int found = -1;
        for (int i = 0; i < alias_count; i++) {
                if (strcmp(alias_names[i], name) == 0) {
                        found = i;
                        break;
                }
        }
        if (found == -1) {
                printf("ERROR - no such alias\n");
                return;
        }
        // shift remaining aliases down to fill the gap
        for (int i = found; i < alias_count - 1; i++) {
                strcpy(alias_names[i], alias_names[i + 1]);
                strcpy(alias_commands[i], alias_commands[i + 1]);
        }
        alias_count--;
}

/* prints all currently set aliases in alias name='command' format */
void printAliases(void) {
        if (alias_count == 0) {
                printf("No aliases set\n");
                return;
        }
        for (int i = 0; i < alias_count; i++) {
                printf("alias %s='%s'\n", alias_names[i], alias_commands[i]);
        }
}


// --- Stage 8: Persistent aliases ---

/* returns the full path to the aliases file in the user's home directory */
char *getAliasFilepath(void) {
        static char filepath[MAX_LINE_LEN];
        char *home = getenv("HOME");
        snprintf(filepath, sizeof(filepath), "%s/.aliases", home);
        return filepath;
}

/* saves current aliases to ~/.aliases before shell exits */
void saveAliases(void) {
        FILE *file = fopen(getAliasFilepath(), "w");
        if (file == NULL) return;
        for (int i = 0; i < alias_count; i++) {
                fprintf(file, "%s %s\n", alias_names[i], alias_commands[i]);
        }
        fclose(file);
}

/* loads aliases from ~/.aliases on shell startup */
void loadAliases(void) {
        FILE *file = fopen(getAliasFilepath(), "r");
        if (file == NULL) return;

        char line[MAX_LINE_LEN];
        while (fgets(line, sizeof(line), file) != NULL) {
                char *space = strchr(line, ' ');
                if (space == NULL) continue;

                *space = '\0'; // split line into name and command
                char name[MAX_LINE_LEN];
                char command[MAX_LINE_LEN];
                strcpy(name, line);
                strcpy(command, space + 1);
                command[strcspn(command, "\n")] = '\0'; // strip trailing newline
                addAlias(name, command);
        }
        fclose(file);
}


// --- Stage 9: Enhanced alias resolution ---

int resolveCommand(char *input) {
        char work[MAX_LINE_LEN];
        int substitutions = 0;

        strcpy(work, input);

        while (substitutions < MAX_SUBSTITUTIONS) {
                // strip leading whitespace for clean token extraction
                char temp[MAX_LINE_LEN];
                strcpy(temp, work);

                // extract first token (the command word) without destroying work
                char first[MAX_LINE_LEN];
                char rest[MAX_LINE_LEN] = "";

                // find end of first token
                int i = 0;
                while (temp[i] != '\0' && temp[i] != ' ' && temp[i] != '\t' && temp[i] != '\n') {
                        first[i] = temp[i];
                        i++;
                }
                first[i] = '\0';

                // rest is everything after the first token (trimming leading space)
                if (temp[i] != '\0') {
                        // skip whitespace between first token and rest
                        int j = i;
                        while (temp[j] == ' ' || temp[j] == '\t') j++;
                        strcpy(rest, temp + j);
                        // strip trailing newline from rest if present
                        rest[strcspn(rest, "\n")] = '\0';
                }

                // --- Try alias expansion first ---
                int found_alias = 0;
                for (int k = 0; k < alias_count; k++) {
                        if (strcmp(first, alias_names[k]) == 0) {
                                // build: aliased_command [rest_of_original_input]
                                char expanded[MAX_LINE_LEN] = "";
                                strcpy(expanded, alias_commands[k]);
                                if (rest[0] != '\0') {
                                        strcat(expanded, " ");
                                        strcat(expanded, rest);
                                }
                                strcat(expanded, "\n");
                                strcpy(work, expanded);
                                found_alias = 1;
                                substitutions++;
                                break;
                        }
                }
                if (found_alias) continue;

                // --- Try history invocation ---
                if (first[0] == '!') {
                        // build a null-terminated history lookup token (no trailing newline)
                        char hist_token[MAX_LINE_LEN];
                        strcpy(hist_token, first);
                        strcat(hist_token, "\n"); // getHistoryCommand expects newline-terminated for !!

                        char *hist_cmd = getHistoryCommand(hist_token);
                        if (hist_cmd == NULL) {
                                return 0; // error already printed
                        }
                        // build: history_command [rest]
                        char expanded[MAX_LINE_LEN] = "";
                        // hist_cmd includes a trailing newline; strip it
                        strcpy(expanded, hist_cmd);
                        expanded[strcspn(expanded, "\n")] = '\0';
                        if (rest[0] != '\0') {
                                strcat(expanded, " ");
                                strcat(expanded, rest);
                        }
                        strcat(expanded, "\n");
                        strcpy(work, expanded);
                        substitutions++;
                        continue;
                }

                // no alias or history substitution happened — command is resolved
                break;
        }

        // if we hit the substitution limit, there is likely a cycle
        if (substitutions >= MAX_SUBSTITUTIONS) {
                printf("Error - alias/history cycle detected or chain too long\n");
                return 0;
        }

        strcpy(input, work);
        return 1;
}


/*

        _       _ _
     __| |_   _| | |_   _
    / _` | | | | | | | | |
   | (_| | |_| | | | |_| |
    \__,_|\__,_|_|_|\__, |
                    |___/

  _ _     _          
 (_) |__ | |__  _   _
 | | '_ \| '_ \| | | |
 | | |_) | |_) | |_| |
 |_|_.__/|_.__/ \__, |
                |___/

  __  __ _
 |  \/  / |
 | |\/| | |
 | |  | | |
 |_|  |_|_|
           
    _               __ 
   / \   _ __ __ _ / _|
  / _ \ | '__/ _` | |_ 
 / ___ \| | | (_| |  _|
/_/   \_\_|  \__,_|_|  

 */