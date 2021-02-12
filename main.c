#include "ext.h"
#include "commands.h"


int main(int argc, char **argv) {
    if(argc < 1){
        printf("Invalid argument! Usage: semestral.exe <nazev_souboru>\n");
        return EXIT_FAILURE;
    }

    char *name = argv[1];
    bool running = true;
    pseudo_inode act_path_inode = {0, true, 1, CLUSTER_SIZE,0};  //nastaveno na root
    char act_path[60] = "/";

    printf("Program started. Enter commands.\n");
    while (running){

        char line[256];
        scanf("%255[^\n]", line);
        fflush(stdin);
        //printf("line given: |%s|\n", line);

        char command[30];
        sscanf(line, "%s", command);
        //printf("command: |%s|\n", command);

        int result = resolve_command(line, name, &act_path_inode, act_path);
        if(result == -1) {      //error
            continue;
        }else if(result == 2){  //exit
            running = false;
        }                       //1 == OK

    }
    
    return EXIT_SUCCESS;
}