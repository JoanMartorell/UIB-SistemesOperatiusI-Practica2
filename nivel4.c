#define _POSIX_C_SOURCE 200112L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#define COMMAND_LINE_SIZE 1024  //longitud de línea de comando
#define ARGS_SIZE 64            //longitud de argumentos
#define N_JOBS 64               //numero máximo de jobs

//Si no vale 1, el prompt solo será el caracter '$'
#define PERSONAL_PROMPT 1
#define PROMPT '$'

#define nivel1 0 //1 -> información del parse_args()
#define nivel2 0 //1 -> información del internal_cd && internal_export
#define nivel3 1 //1 -> información del execute_line && internal_source
#define nivel4 1 //1 -> información del reaper() && ctrlc()

//Definimos colores
#define RESET "\033[0m"
#define NEGRO_T "\x1b[30m"
#define NEGRO_F "\x1b[40m"
#define GRIS_T "\x1b[94m"
#define ROJO_T "\x1b[31m"
#define VERDE_T "\x1b[32m"
#define AMARILLO_T "\x1b[33m"
#define AZUL_T "\x1b[34m"
#define MAGENTA_T "\x1b[35m"
#define CYAN_T "\x1b[36m"
#define BLANCO_T "\x1b[97m"
#define NEGRITA "\x1b[1m"

//Declaramos funciones
char *read_line(char *line);
void imprimir_prompt();
int execute_line(char *line);
int parse_args(char **args, char *line);
void reaper(int signum);
void ctrlc(int signum);

int check_internal(char **args);
int internal_cd(char **args);
int internal_export(char **args);
int internal_source(char **args);
int internal_jobs(char **args);
int internal_fg(char **args);
int internal_bg(char **args);

//Funciones adicionales
void removerCaracteres(char *cadena, char *caracteres);

//Variable global para guardar el nombre del minishell
static char mi_shell[COMMAND_LINE_SIZE]; 

struct info_job {
    pid_t pid;
    char status; //'N', 'E','D','F'
    char cmd[COMMAND_LINE_SIZE]; //linea de comando asociada
};

static struct info_job jobs_list[N_JOBS];

int main(int argc, char *argv[]) {
    //Declaramos la línea donde se guardará el comando
    char line[COMMAND_LINE_SIZE];
    memset(line, 0, COMMAND_LINE_SIZE);

    //Obtenemos el nombre del minishell
    strcpy(mi_shell, argv[0]);

    //Inicializamos los datos de jobs_list[0]
    jobs_list[0].pid = 0; //pid = 0;
    jobs_list[0].status = 'N'; //status = 'N'
    memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);

    signal(SIGCHLD, reaper);
    signal(SIGINT, ctrlc);

    while (1) {
        if (read_line(line)) {
            execute_line(line);
        }

    }

    return 0;
}

//Función para leer la línea 
char *read_line(char *line) {
    imprimir_prompt();
    //Leemos una linea de consola
    char *p = fgets(line, COMMAND_LINE_SIZE, stdin);
    if (p != NULL) {
        char *letra = strchr(line, 10);
        if (letra != NULL) {
            *letra = '\0';
        }
    } else {
        printf("\r");
        if (feof(stdin)) {
            fprintf(stderr, "Adios\n");
            exit(0);
        }
    }
    return p;
}

//Función que imprime el prompt
void imprimir_prompt() {
    #if PERSONAL_PROMPT == 1 
        char directorio[PATH_MAX];
        printf(NEGRITA ROJO_T "%s" BLANCO_T ":" AZUL_T "%s ", getenv("USERNAME"), getcwd(directorio, sizeof(directorio)));
        printf(BLANCO_T "%c " RESET, PROMPT);
    #else
        printf("%c",PROMPT);
    #endif

    fflush(stdout);
    return;
}

//Función que ejecuta el comando
int execute_line(char *line) {
    char *args[ARGS_SIZE];

    char command_line[COMMAND_LINE_SIZE];
    memset(command_line, '\0', COMMAND_LINE_SIZE);
    strcpy(command_line, line);

    //Dividimos la línea en tokens
    if (parse_args(args, line) > 0) {
        if (check_internal(args)) {
            return 1;
        }
    }

    pid_t pid = fork();
    pid_t status;

    if (pid == 0) { //Proceso hijo

        //Asociamos acción por defecto a SIGCHLD
        signal(SIGCHLD, SIG_DFL); 
        //Ingnoramos la señal SIGINT
        signal(SIGINT, SIG_IGN);

        #if nivel3
            fprintf(stderr, GRIS_T "[execute_line()-> PID hijo: %d (%s)]\n" RESET, getpid(), command_line);
        #endif

        execvp(args[0],args);

        //perror(ROJO_T "%s" RESET, command_line);
        fprintf(stderr, ROJO_T "%s: no se encontró la orden\n" RESET, command_line);
        exit(-1);

    } else if (pid > 0) {
        //Asociamos el manejador ctrlc a SIGINT
        signal(SIGINT, ctrlc);

        #if nivel3
            fprintf(stderr, GRIS_T "[execute_line()-> PID padre: %d (%s)]\n" RESET, getpid(), mi_shell);
        #endif

        //Actualizar los datos de jobs_list[0]
        jobs_list[0].pid = pid;
        jobs_list[0].status = 'E';
        status = jobs_list[0].status;
        strcpy(jobs_list[0].cmd, command_line);

        //#if nivel3
        //    fprintf(stderr, GRIS_T "[execute_line()-> PID hijo: %d (%s) finalizado con exit(), estado: %d]\n\n" RESET, jobs_list[0].pid,jobs_list[0].cmd, WEXITSTATUS(status));
        //#endif

        while (jobs_list[0].pid > 0) {
            pause();
        }

    } else {
        perror(ROJO_T "Error" RESET);
    }

    return 0;
}

//Función que trocea la línea en tokens
int parse_args(char **args, char *line) {
    int i = 0;

    args[i] = strtok(line, " \t\n\r");

    #if nivel1
        fprintf(stderr, GRIS_T "[parse_args()-> token %i: %s]\n" RESET, i, args[i]);
    #endif

    while (args[i] != NULL && args[i][0] != '#') {
        i++;
        args[i] = strtok(NULL, " \t\n\r");
        #if nivel1 
            fprintf(stderr, GRIS_T "[parse_args()-> token %i: %s]\n" RESET, i, args[i]);
        #endif
    }
    if (args[i]) {
        args[i] = NULL;
        #if nivel1
            fprintf(stderr, GRIS_T "[parse_args()-> token %i corregido: %s]\n" RESET, i, args[i]);
        #endif
    }

    return i;
}

//Función que maneja la señal SIGCHLD
void reaper(int signum) {

    signal(SIGCHLD, reaper);

    pid_t status;
    int ended;

    // while ((ended = waitpid(-1, &status, WNOHANG)) > 0) {
    //     if (ended == jobs_list[0].pid) { 

    //         #if nivel4
    //             fprintf(stderr,GRIS_T "[reaper()-> Proceso hijo %d (%s) finalizado con exit code %d]\n" RESET, ended, jobs_list[0].cmd, WEXITSTATUS(status));
    //         #endif

    //         //Reseteamos el foreground
    //         jobs_list[0].pid = 0;
    //         jobs_list[0].status = 'F';
    //         memset(jobs_list[0].cmd, '\0', COMMAND_LINE_SIZE);
    //     }
    // }

    while ((ended = waitpid(-1, &status, WNOHANG)) > 0) {
        if (jobs_list[0].pid == ended) {  //Foreground
            if(status != 0) {
                #if nivel4
                    fprintf(stderr,GRIS_T "[reaper()→ Proceso hijo %d en foreground (%s) finalizado por señal %d]\n" RESET, ended, jobs_list[0].cmd, status);
                #endif
            }else {
                #if nivel4
                    fprintf(stderr,GRIS_T "[reaper()→ Proceso hijo %d (%s) finalizado con exit code %d]\n" RESET, ended, jobs_list[0].cmd, status);
                #endif
            }

            jobs_list[0].pid = 0;
            jobs_list[0].status = 'F';
        }
    }
}

//Función que maneja la señal SIGINT
void ctrlc(int signum) {

    signal(SIGINT, ctrlc);

    if (jobs_list[0].pid > 0) {
        if (strcmp(jobs_list[0].cmd, mi_shell) != 0) {
            if (kill(jobs_list[0].pid, SIGTERM) == 0) {

                #if nivel4
                    fprintf(stderr, GRIS_T "\n[ctrlc()→ Soy el proceso con PID %d (%s), el proceso en foreground es %d (%s)]\n" RESET, getpid(), mi_shell, jobs_list[0].pid, jobs_list[0].cmd);
                    fprintf(stderr, GRIS_T "[ctrlc()→ Señal 15 enviada a %d (%s) por %d (%s)]\n" RESET, jobs_list[0].pid, jobs_list[0].cmd, getpid(), mi_shell);
                #endif

            } else {
                perror("Error kill");
                exit(-1);
            }
        } else {

            #if nivel4 
                fprintf(stderr, GRIS_T "\n[ctrlc()→ Señal 15 no enviada debido a que el proceso en foreground es el shell]\n" RESET);
            #endif
        }
        
    } else {

        #if nivel4
            fprintf(stderr, GRIS_T "\n[ctrlc()→ Soy el proceso con PID %d (%s), el proceso en foreground es %d (%s)]\n" RESET, getpid(), mi_shell, jobs_list[0].pid, jobs_list[0].cmd);
            fprintf(stderr, GRIS_T "[ctrlc()→ Señal 15 no enviada por %d (%s) debido a que no hay proceso en foreground]\n" RESET, getpid(), mi_shell);
        #endif
    }
}

//Función que detecta si es un comando interno
int check_internal(char **args) {
    if (!strcmp(args[0], "cd")) {
        return internal_cd(args);
    }
    if (!strcmp(args[0], "export")) {
        return internal_export(args);
    }
    if (!strcmp(args[0], "source")) {
        return internal_source(args);
    }
    if (!strcmp(args[0], "jobs")) {
        return internal_jobs(args);
    }
    if (!strcmp(args[0], "fg")) {
        return internal_fg(args);
    }
    if (!strcmp(args[0], "cd")) {
        return internal_bg(args);
    }
    if (!strcmp(args[0], "exit")) {
        exit(0);
    }
    return 0;
}

//Función que cambia el directorio
int internal_cd(char **args) {
    #if nivel1 
        fprintf(stderr, GRIS_T "[internal_cd()-> Esta función cambiará el directorio]\n");
    #endif

    char cwd[PATH_MAX];
    strcpy(cwd, "");

    if (args[1] == NULL) {
        chdir(getenv("HOME"));
    } else {

        //cd avanzado
        char *caracteres = {"\'\"\\"};  //Carcateres a eliminar

        char aux[PATH_MAX];
        int i = 1;
        while(args[i] != NULL) {  //Recorremos cada argumento
            strcpy(aux, args[i]);
            removerCaracteres(aux, caracteres);   //Quitamos los caracteres especiales

            if(args[i+1] != NULL) {  //En caso de que haya un elemento a continuación,
                strcat(aux, " ");    //añadimos un espacio
            }
            
            strcat(cwd, aux);  //Unimos cada argumento
            i++;
        }

        if (chdir(cwd) == -1) {
            fprintf(stderr, NEGRO_T "chdir: %s\n", strerror(errno));
        }        
    }
    
    getcwd(cwd, PATH_MAX);

    #if nivel2
        fprintf(stderr, GRIS_T "[internal_cd()-> PWD: %s]\n" RESET, cwd);
    #endif

    return 1;
}

//Función que escompone en tokens el argumento NOMBRE=VALOR
int internal_export(char **args) {
    #if nivel1 
        printf("[internal_export()-> Esta función asignará valores a variables de entorno]\n");
    #endif

    char *nombre;
    char *valor;

    nombre = strtok(args[1], "=");
    if(nombre == NULL) {
        fprintf(stderr, ROJO_T "Error de sintaxis. Uso: export Nombre=Valor\n" RESET);
    } else {
        valor = strtok(NULL, "");
        if (valor == NULL) {
            #if nivel2
                fprintf(stderr, GRIS_T "[internal_export()-> nombre: %s]\n" RESET, nombre);
                fprintf(stderr, GRIS_T "[internal_export()-> valor: %s]\n" RESET, valor);
            #endif
            fprintf(stderr, ROJO_T "Error de sintaxis. Uso: export Nombre=Valor\n" RESET);
        } else {
            #if nivel2
                fprintf(stderr, GRIS_T "[internal_export()-> nombre: %s]\n" RESET, nombre);
                fprintf(stderr, GRIS_T "[internal_export()-> valor: %s]\n" RESET, valor);
                fprintf(stderr, GRIS_T "[internal_export()-> antiguo valor para %s: %s]\n" RESET, nombre, getenv(nombre));
            #endif

            setenv(nombre, valor, 1);

            #if nivel2
            fprintf(stderr, GRIS_T "[internal_export()-> nuevo valor para %s: %s]\n" RESET, nombre, getenv(nombre));
            #endif
        }
    }
    
    return 1;
}

//Función que comprueba los argumentos
int internal_source(char **args) {
    #if nivel1 
        printf("[internal_source()-> Esta función ejecutará un fichero de líneas de comandos]\n");
    #endif
    
    char linea[COMMAND_LINE_SIZE];
    FILE *f;
    if (args[1] == NULL) {
        fprintf(stderr, ROJO_T "Error de sintaxis. Uso: source <nombre_fichero>\n" RESET);
    } else {
        f = fopen(args[1], "r");
        if(f == NULL) {
            //perror(ROJO_T "fopen: " RESET);
            fprintf(stderr, ROJO_T "fopen: No such file or directory\n" RESET);
            return -1;
        } else {
            while(fgets(linea, COMMAND_LINE_SIZE, f) != NULL) {
                for (int i = 0; i < COMMAND_LINE_SIZE; i++){
                    if (linea[i] == '\n') {
                        linea[i] = '\0';
                    }
                }
                #if nivel3
                    fprintf(stderr, GRIS_T "[internal_source()-> LINE: %s]\n" RESET, linea);
                #endif
                fflush(f);
                execute_line(linea);
            }
            fclose(f);
        }
    }
    return 1;
}

int internal_jobs(char **args) {
    #if nivel1 
        printf("[internal_jobs()-> Esta función mostrará el PID de los procesos que no estén en foreground]\n");
    #endif
    return 1;
}

int internal_fg(char **args) {
    #if nivel1 
        printf("[internal_fg()-> Esta función enviará un trabajo detenido al foreground reactivando su ejecución, o uno del background al foreground]\n");
    #endif
    return 1;
}

int internal_bg(char **args) {
    #if nivel1 
        printf("[internal_bg()-> Esta función reactivará un proceso detenido para que siga ejecutándose pero en segundo plano]\n");
    #endif
    return 1;
}


//Función adicional para remover caracteres en una cadena
void removerCaracteres(char *cadena, char *caracteres) {
    int indiceCadena = 0, indiceCadenaLimpia = 0;
    int deberiaAgregarCaracter = 1;

    // Recorrer cadena carácter por carácter
    while (cadena[indiceCadena]) {
        // Primero suponemos que la letra sí debe permanecer
        deberiaAgregarCaracter = 1;
        int indiceCaracteres = 0;

        // Recorrer los caracteres prohibidos
        while (caracteres[indiceCaracteres]) {
            // Y si la letra actual es uno de los caracteres, ya no se agrega
            if (cadena[indiceCadena] == caracteres[indiceCaracteres]) {
                deberiaAgregarCaracter = 0;
            }
            indiceCaracteres++;
        }

        // Dependiendo de la variable de arriba, la letra se agrega a la "nueva cadena"
        if (deberiaAgregarCaracter) {
            cadena[indiceCadenaLimpia] = cadena[indiceCadena];
            indiceCadenaLimpia++;
        }

        indiceCadena++;
    }

    // Al final se agrega el carácter NULL para terminar la cadena
    cadena[indiceCadenaLimpia] = 0;
}
