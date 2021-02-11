
// v0.2 - nahrazení zconf.h za stdlib.h
// v0.1

// toto je primitivní example, který využívá struktury ze souboru ext.h
// format - vytvoří layout virtuálního FS s root složkou / a souborem /soubor.txt, který je předán parametrem
// parametry FS jsou schválně nesmyslně malé, aby byl layout jednoduše vidět nástrojem xxd nebo jiným bin. prohlížečem
// read sekce pak přečte binární soubor FS a názorně vypisuje obsah

#include "ext.h"
#include <ctype.h>


int main(int argc, char **argv) {
    if(argc < 1){
        printf("Invalid argument! Usage: semestral.exe <nazev_souboru>\n");
        return EXIT_FAILURE;
    }

    const char *name = argv[1];
    bool running = true;
    char *act_path = "/";
    struct pseudo_inode act_path_inode = {0, true, 1, CLUSTER_SIZE,0};  //nastaveno na root

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

            remove(name);
            FILE *fptr = fopen(name, "wb");
            if (fptr == NULL) {
                printf("Something went wrong with format read()! %s |%s|\n", strerror(errno), name);
                exit(2);
            }

            // zapis superblocku
            struct superblock sb = {};
            if (fill_sb(&sb, disk_size) == EXIT_FAILURE){
                fclose(fptr);
                continue;
            }
            //printSb(&sb);

            sb.inodes_free_count--;     //root slozka
            sb.cluster_free_count--;    //obsah root slozky
            fwrite(&sb, sizeof(sb), 1, fptr); // automaticky posune fseek
            // inode bitmapa /
            u_char inode_bitmap[sb.bitmapi_size];
            for(int i = 0; i < sb.bitmapi_size; i++){
                inode_bitmap[i] = 0b00000000;
            }
            inode_bitmap[0] = 0b10000000;

            // data bitmapa - první jsou data složky /
            u_char data_bitmap[sb.bitmap_size];
            for(int i = 0; i < sb.bitmapi_size; i++){
                data_bitmap[i] = 0b00000000;
            }
            data_bitmap[0] = 0b10000000;

            //printSb(&sb);
            // zapis bitmap
            fwrite(&inode_bitmap, sizeof(u_char), sb.bitmapi_size, fptr);
            fwrite(&data_bitmap, sizeof(u_char), sb.bitmap_size, fptr);

            // zapis inodu pro /, velikost nastavena na 32B tedy celý první cluster (kam se vejdou maximálně 2 položky)
            struct pseudo_inode root = {0, true, 1, sb.cluster_size,0}; // velikost nastavená na maximum jednoho clusteru
            fwrite(&root, sizeof(root), 1, fptr);

            char inode_placeholder[sizeof(struct pseudo_inode)]; //40 = sizeof(struct pseudo_inode))
            strcpy(inode_placeholder, "<===========prazdny_inode==============>");
            // na konci řetězce je '\0', proto v xxd výpisu je mezera mezi placeholdery "=>.<=" - pridal sem jedno = aby tam . nebyla

            //vyplnim zbyle placeholdery
            for (int i = 0; i < sb.inodes_count-1; i++) {
                fwrite(&inode_placeholder, sizeof(inode_placeholder), 1, fptr);
            }

            // data slozky /, prvni je odkaz sam na sebe
            struct directory_item di_root = {0, "."};
            fwrite(&di_root, sizeof(di_root), 1, fptr);
            struct directory_item empty = {ID_ITEM_FREE, ""};
            fwrite(&empty, sizeof(empty), 1, fptr);

            char data_placeholder[sb.cluster_size];  //32 = sizeof cluster
            strcpy(data_placeholder, "<============volna_data========>");

            //zbytek dat
            //printf("cluster_count:%d\n",sb.cluster_count);
            for (int i = 0; i < sb.cluster_count - 1; i++) {
                fwrite(&data_placeholder, sizeof(data_placeholder), 1, fptr);
            }

            fclose(fptr);
            printf("Succesfully formated\n");
        }


        //incp
        if (strstr(command, "incp") != NULL) {
            char s1[60];    //source na disku
            char s2[60];    //dest v fs.ext
            char file_name[12];
            char *path;
            command_recognized = 1;

            sscanf(line, "%29s %59s %59s", command, s1, s2);
            //printf("print input parsed: |%s| |%s| |%s|\n", command, s1, s2);
            //incp soubor.txt soubor.txt

            //parsovani nazvu souboru - cesty
            char *path_parse_tmp1 = strdup(s2);
            char *path_parse_tmp2 = strdup(s2);
            path = dirname(path_parse_tmp1);
            char * tmp_file_name = basename(path_parse_tmp2);
            if(strlen(tmp_file_name)>12){
                printf("File name is too long. Maximum is 12, %d given.\n",strlen(tmp_file_name));
                continue;
            }
            strcpy(file_name, tmp_file_name);
            //printf("path: |%s| filename:|%s|\n", path, file_name);


            FILE *fptr = fopen(name, "rb+");
            if (fptr == NULL) {
                printf("%s |%s|\n", strerror(errno), name);
                exit(2);
            }
            struct superblock sb = {};
            fread(&sb, sizeof(sb), 1, fptr);

            u_char inode_bitmap [sb.bitmapi_size];
            u_char data_bitmap [sb.bitmap_size];
            fread(&inode_bitmap, sizeof(inode_bitmap), 1, fptr);
            fread(&data_bitmap, sizeof(data_bitmap), 1, fptr);

            struct pseudo_inode act_dir_inode;
            if(!set_inode_by_path(&act_dir_inode, path, &sb, fptr, &act_path_inode)) {
                printf("Path does not exist.\n");
                fclose(fptr);
                continue;
            }
            //printf("nodeid:%d | isDirectory:%d\n",act_dir_inode.nodeid, act_dir_inode.isDirectory);


            fseek(fptr, sb.inode_start_address, SEEK_SET);  //nastaveni fseek na inode aktualni slozky TODO - nastaveno na root, dodelat obecne
            fread(&act_dir_inode, sizeof(struct pseudo_inode), 1, fptr);


            //overeni, ze lze pridat soubor do cilove slozky
            int act_dir_free_file_id = 0;
            int act_dir_free_file_nodeid;
            fseek(fptr, sb.data_start_address + (act_dir_inode.direct1 * sizeof(CLUSTER_SIZE) ), SEEK_SET); //nastaveni fseek na data aktualni slozky
            for (int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
                struct directory_item act_dir_content_file;
                fread(&act_dir_content_file, sizeof(struct directory_item), 1, fptr);
                if(act_dir_content_file.inode == ID_ITEM_FREE){
                    act_dir_free_file_nodeid = act_dir_content_file.inode;
                    //printf("act_dir_free_file_nodeid:%d\n",act_dir_free_file_nodeid);
                    break;
                }
                act_dir_free_file_id++;
            }
            if(act_dir_free_file_nodeid == ID_ITEM_FREE) {
                FILE *fptr1 = fopen(s1, "rb");
                if (fptr1 == NULL) {
                    printf("Something went wrong with read()! %s |%s|\n", strerror(errno), s1);
                    fclose(fptr);
                    continue;
                    //exit(2);
                }
                //incp soubor.txt soubor.txt
                if (sb.inodes_free_count > 0 && sb.cluster_free_count > 0) {
                    long int file_size = findSize(s1);
                    int needed_cluster_count = get_needed_clusters(file_size);
                    printf("needed_cluster_count:%d\n",needed_cluster_count);
                    if (sb.cluster_free_count >= needed_cluster_count) {
                        //nasteni nove bitmapy inodes
                        int new_nodeid;
                        for (int i = 0; i < sb.bitmapi_size; i++) {
                            if (inode_bitmap[i] < 0b11111111) {
                                new_nodeid = find_empty_data_node_in_bitmap(inode_bitmap[i]) + (i * 8);
                                inode_bitmap[i] = set_bit_on_position_in_bitmap(inode_bitmap[i], new_nodeid, 1);
                                break;
                            }
                        }

                        struct directory_item new_file = {new_nodeid, file_name[0]}; //file_name "soubor.txt"
                        //nastevni cluster bitmapy - zatim pocitam s tim, že data se vejdou do 5 clusteru TODO - overit, ze neni potreba undirected
                        int new_file_clusters[DIRECT_LINK_COUNT] = {ID_CLUESTER_FREE};    // id clusteru noveho souboru - pujde pak do vytvareni pseudo_inode
                        //nastavim na -1, pokud je -1 znamena, že není provázán
                        if (needed_cluster_count > 0) {
                            for (int j = 0; j < needed_cluster_count; j++) {
                                for (int i = 0; i < sb.bitmap_size; i++) {
                                    if (data_bitmap[i] < 0b11111111) {
                                        int bitmap_position;
                                        bitmap_position = find_empty_data_node_in_bitmap(data_bitmap[i]);
                                        new_file_clusters[j] = bitmap_position + (i * 8);    //pridam do pole novych direct clusteru
                                        data_bitmap[i] = set_bit_on_position_in_bitmap(data_bitmap[i], bitmap_position ,1);
                                        break;
                                    }
                                }
                            }
                        }
                        struct pseudo_inode new_file_inode = {new_nodeid, false, 1, file_size, new_file_clusters[1],
                                                              new_file_clusters[2], new_file_clusters[3],
                                                              new_file_clusters[4], new_file_clusters[5]};

                        sb.inodes_free_count --;
                        sb.cluster_free_count = sb.cluster_free_count - needed_cluster_count;
                        //zapis dat - superblock, bitmapy, uprava dir_item ve slozce
                        fseek(fptr, 0, SEEK_SET);   //nastaveni fseek na zacatek
                        fwrite(&sb, sizeof(sb), 1, fptr);
                        fwrite(&inode_bitmap, sizeof(u_char), sb.bitmapi_size, fptr);
                        fwrite(&data_bitmap, sizeof(u_char), sb.bitmap_size, fptr);
                        fseek(fptr, sb.inode_start_address + (new_nodeid * sizeof(struct pseudo_inode)),SEEK_SET);   //nastaveni fseeku na pozici noveho inodu
                        fwrite(&new_file_inode, sizeof(new_file_inode), 1, fptr);
                        fseek(fptr, sb.data_start_address                                                   //adresa na zacatek dat
                                            + (act_dir_inode.direct1 * CLUSTER_SIZE)                               //na data aktualni slozky
                                            + (act_dir_free_file_id * sizeof(struct directory_item)), SEEK_SET);   //na data konkretniho souboru
                        fwrite(&new_file, sizeof(new_file), 1,fptr);

                        //zapis dat do clusteru
                        unsigned char buffer[CLUSTER_SIZE] = {0};
                        int new_file_clusters_increment = 0;
                        while ((fread(buffer, 1, sizeof(buffer), fptr1)) > 0) {
                            int cluster_ofset = new_file_clusters[new_file_clusters_increment];
                            if (cluster_ofset > ID_CLUESTER_FREE) {   //overeni, ze vybrany direct neni -1
                                fseek(fptr, sb.data_start_address + (cluster_ofset * CLUSTER_SIZE),SEEK_SET); //nastaveni fseek na spravnou pozici idclusteru * velikost cluster od zacatku dat
                                fwrite(&buffer, sizeof(buffer), 1, fptr);
                                memset(buffer, 0x00, CLUSTER_SIZE);     //vynulovani bufferu na cteni souboru
                            } else {
                                printf("Part of file was not uploaded. Invalid cluster id.\n");
                            }
                            new_file_clusters_increment++;
                        }

                        //konec - zavreni souboru
                        fclose(fptr1);
                        fclose(fptr);
                        printf("File succesfully uploaded.\n");

                    } else {
                        printf("File couldnt be upload. Not enough free space.\n");
                        fclose(fptr);
                        fclose(fptr1);
                        continue;
                    }
                } else {
                    printf("File couldnt be upload. Filesystem is full.\n");
                    fclose(fptr);
                    fclose(fptr1);
                    continue;
                }
            }else{
                printf("File couldnt be upload. Selected folder is full.\n");
                fclose(fptr);
                continue;
            }
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