#include "ext.h"
#include "commands.h"
#include <ctype.h>


int main(int argc, char **argv) {
    if(argc < 1){
        printf("Invalid argument! Usage: semestral.exe <nazev_souboru>\n");
        return EXIT_FAILURE;
    }

    char *name = argv[1];
    bool running = true;
    pseudo_inode act_path_inode = {0, true, 1, CLUSTER_SIZE,0};  //nastaveno na root

    printf("Program started. Enter commands.\n");
    while (running){
        bool command_recognized = 0;

        char line[256];
        scanf("%255[^\n]", line);
        fflush(stdin);
        //printf("line given: |%s|\n", line);

        char command[30];
        sscanf(line, "%s", command);
        //printf("command: |%s|\n", command);

        //format
        if (strstr(command, "format") != NULL) {
            int size_number = 0;
            char size_char = 0;
            int size_multiplic;
            int disk_size;
            command_recognized = 1;

            sscanf(line, "%s %d %c", command, &size_number, &size_char);
            //printf("print input parsed: %s %d %c\n", command, size_number, size_char);

            size_char = (char) toupper((int)size_char);
            switch (size_char) {
                case 'K':
                    size_multiplic = 10;
                    break;
                case 'M':
                    size_multiplic = 20;
                    break;
                case 'G':
                    size_multiplic = 30;
                    break;
                default:
                    size_multiplic = 0;
            }

            disk_size = size_number * ((int)pow(2, size_multiplic));
            if(format(disk_size, name) == -1)
                continue;
        }


        //incp
        if (strstr(command, "incp") != NULL) {
            char s1[60];    //source na disku
            char s2[60];    //dest v fs.ext
            command_recognized = 1;

            sscanf(line, "%29s %59s %59s", command, s1, s2);
            if(incp(name, &act_path_inode, s1, s2) == -1)
                continue;
        }

        //cat
        if (strstr(command, "cat") != NULL) {
            char s1[60];    //source v fs.ext
            command_recognized = 1;

            sscanf(line, "%29s %59s", command, s1);
            if(cat(name, &act_path_inode, s1) == -1)
                continue;
        }

        //info
        if (strstr(command, "info") != NULL) {
            char s1[60];    //source v fs.ext
            command_recognized = 1;

            sscanf(line, "%29s %59s", command, s1);
            if(info(name, &act_path_inode, s1) == -1)
                continue;
        }

        //outcp
        if (strstr(command, "outcp") != NULL) {
            char s1[60];    //source v fs.ext
            char s2[60];    //dest na disku
            command_recognized = 1;

            sscanf(line, "%29s %59s %59s", command, s1, s2);
            if(outcp(name, &act_path_inode, s1, s2) == -1)
                continue;
        }

        //exit
        if (strstr(command, "exit") != NULL) {
            command_recognized = 1;
            running = false;
        }

        //help
        if (strstr(command, "help") != NULL) {
            command_recognized = 1;
            printf("Commands are:\n");
            printf(" 1  format <size>\n");
            printf(" 2  exit\n");
            printf(" 3  incp <source on disk> <dest in FS>\n");

        }

        if(!command_recognized){
            printf("Command not recognized. Use 'help' command.\n");
        }
    }
    
    return EXIT_SUCCESS;
}