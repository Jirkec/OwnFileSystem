#include "commands.h"

int format(int disk_size, char *name){
    remove(name);
    FILE *fptr = fopen(name, "wb");
    if (fptr == NULL) {
        printf("Something went wrong with format read()! %s |%s|\n", strerror(errno), name);
        exit(2);
    }

    // zapis superblocku
    superblock sb = {};
    if (fill_sb(&sb, disk_size) == EXIT_FAILURE){
        fclose(fptr);
        return -1;
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
    pseudo_inode root = {0, true, 1, sb.cluster_size,0, ID_CLUESTER_FREE, ID_CLUESTER_FREE, ID_CLUESTER_FREE, ID_CLUESTER_FREE}; // velikost nastavená na maximum jednoho clusteru
    fwrite(&root, sizeof(root), 1, fptr);

    char inode_placeholder[sizeof( pseudo_inode)]; //40 = sizeof( pseudo_inode))
    strcpy(inode_placeholder, "<===========prazdny_inode==============>");
    // na konci řetězce je '\0', proto v xxd výpisu je mezera mezi placeholdery "=>.<=" - pridal sem jedno = aby tam . nebyla

    //vyplnim zbyle placeholdery
    for (int i = 0; i < sb.inodes_count-1; i++) {
        fwrite(&inode_placeholder, sizeof(inode_placeholder), 1, fptr);
    }

    // data slozky /, prvni je odkaz sam na sebe
    directory_item di_root = {0, "."};
    fwrite(&di_root, sizeof(di_root), 1, fptr);
    directory_item empty = {ID_ITEM_FREE, ""};
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
    return 1;
}

int incp(char *name, pseudo_inode *act_path_inode, char *s1, char *s2){
    char file_name[12];
    char *path;

    //parsovani nazvu souboru - cesty
    char *path_parse_tmp1 = strdup(s2);
    char *path_parse_tmp2 = strdup(s2);
    path = dirname(path_parse_tmp1);
    char * tmp_file_name = basename(path_parse_tmp2);
    if(strlen(tmp_file_name)>12){
        printf("File name is too long. Maximum is 12, %d given.\n",strlen(tmp_file_name));
        return -1;
    }
    strcpy(file_name, tmp_file_name);
    //printf("path: |%s| filename:|%s|\n", path, file_name);


    FILE *fptr = fopen(name, "rb+");
    if (fptr == NULL) {
        printf("%s |%s|\n", strerror(errno), name);
        return -1;
    }
    superblock sb = {};
    fread(&sb, sizeof(sb), 1, fptr);

    u_char inode_bitmap [sb.bitmapi_size];
    u_char data_bitmap [sb.bitmap_size];
    fread(&inode_bitmap, sizeof(inode_bitmap), 1, fptr);
    fread(&data_bitmap, sizeof(data_bitmap), 1, fptr);

    //nastaveni cesty
    pseudo_inode act_dir_inode;
    if(!set_inode_by_path(&act_dir_inode, path, &sb, fptr, act_path_inode)) {
        printf("Path does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("nodeid:%d | isDirectory:%d\n",act_dir_inode.nodeid, act_dir_inode.isDirectory);

    //overeni, ze lze pridat soubor do cilove slozky
    int act_dir_free_file_id = 0;
    int act_dir_free_file_nodeid;
    fseek(fptr, sb.data_start_address + (act_dir_inode.direct1 * sizeof(CLUSTER_SIZE) ), SEEK_SET); //nastaveni fseek na data aktualni slozky
    for (int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
        directory_item act_dir_content_file;
        fread(&act_dir_content_file, sizeof(directory_item), 1, fptr);
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
            return -1;
        }
        //incp soubor.txt soubor.txt
        if (sb.inodes_free_count > 0 && sb.cluster_free_count > 0) {
            long int file_size = findSize(s1);
            int needed_cluster_count = get_needed_clusters(file_size);
            //printf("needed_cluster_count:%d\n",needed_cluster_count);
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

                directory_item new_file = {new_nodeid, "<=filename=>"}; //nejdriv placehordel a potom nakopiruju nazev
                strcpy(new_file.item_name, file_name);
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
                pseudo_inode new_file_inode = {new_nodeid, false, 1, file_size, new_file_clusters[0],
                                               new_file_clusters[1], new_file_clusters[2],
                                               new_file_clusters[3], new_file_clusters[4]};

                sb.inodes_free_count --;
                sb.cluster_free_count = sb.cluster_free_count - needed_cluster_count;
                //zapis dat - superblock, bitmapy, uprava dir_item ve slozce
                fseek(fptr, 0, SEEK_SET);   //nastaveni fseek na zacatek
                fwrite(&sb, sizeof(sb), 1, fptr);
                fwrite(&inode_bitmap, sizeof(u_char), sb.bitmapi_size, fptr);
                fwrite(&data_bitmap, sizeof(u_char), sb.bitmap_size, fptr);
                fseek(fptr, sb.inode_start_address + (new_nodeid * sizeof(pseudo_inode)),SEEK_SET);   //nastaveni fseeku na pozici noveho inodu
                fwrite(&new_file_inode, sizeof(new_file_inode), 1, fptr);
                fseek(fptr, sb.data_start_address                                                   //adresa na zacatek dat
                            + (act_dir_inode.direct1 * CLUSTER_SIZE)                               //na data aktualni slozky
                            + (act_dir_free_file_id * sizeof(directory_item)), SEEK_SET);   //na data konkretniho souboru
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
                return -1;
            }
        } else {
            printf("File couldnt be upload. Filesystem is full.\n");
            fclose(fptr);
            fclose(fptr1);
            return -1;
        }
    }else{
        printf("File couldnt be upload. Selected folder is full.\n");
        fclose(fptr);
        return -1;
    }
}

int cat(char *name, pseudo_inode *act_path_inode, char *s1){
    char file_name[12];
    char *path;

    //parsovani nazvu souboru - cesty
    char *path_parse_tmp1 = strdup(s1);
    char *path_parse_tmp2 = strdup(s1);
    path = dirname(path_parse_tmp1);
    char * tmp_file_name = basename(path_parse_tmp2);
    if(strlen(tmp_file_name)>12){
        printf("File name is too long. Maximum is 12, %d given.\n",strlen(tmp_file_name));
        return -1;
    }
    strcpy(file_name, tmp_file_name);
    //printf("path: |%s| filename:|%s|\n", path, file_name);


    FILE *fptr = fopen(name, "rb+");
    if (fptr == NULL) {
        printf("%s |%s|\n", strerror(errno), name);
        return -1;
    }
    superblock sb = {};
    fread(&sb, sizeof(sb), 1, fptr);

    //nastaveni cesty
    pseudo_inode act_dir_inode;
    if(!set_inode_by_path(&act_dir_inode, path, &sb, fptr, act_path_inode)) {
        printf("Path to dir does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("DIR: nodeid:%d | file_size:%d\n", act_dir_inode.nodeid, act_dir_inode.file_size);

    pseudo_inode file;
    if(!set_file_by_name(fptr,&sb, &act_dir_inode, &file, file_name)) {
        printf("Path to file does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("nodeid:%d | file_size:%d\n", file.nodeid, file.file_size);
    print_file_content(&file, fptr, &sb);

    fclose(fptr);
}

int info(char *name, pseudo_inode *act_path_inode, char *s1){
    char file_name[12];
    char *path;

    //parsovani nazvu souboru - cesty
    char *path_parse_tmp1 = strdup(s1);
    char *path_parse_tmp2 = strdup(s1);
    path = dirname(path_parse_tmp1);
    char * tmp_file_name = basename(path_parse_tmp2);
    if(strlen(tmp_file_name)>12){
        printf("File name is too long. Maximum is 12, %d given.\n",strlen(tmp_file_name));
        return -1;
    }
    strcpy(file_name, tmp_file_name);
    //printf("path: |%s| filename:|%s|\n", path, file_name);


    FILE *fptr = fopen(name, "rb+");
    if (fptr == NULL) {
        printf("%s |%s|\n", strerror(errno), name);
        return -1;
    }
    superblock sb = {};
    fread(&sb, sizeof(sb), 1, fptr);

    //nastaveni cesty
    pseudo_inode act_dir_inode;
    if(!set_inode_by_path(&act_dir_inode, path, &sb, fptr, act_path_inode)) {
        printf("Path to dir does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("DIR: nodeid:%d | file_size:%d\n", act_dir_inode.nodeid, act_dir_inode.file_size);

    pseudo_inode file;
    if(!set_inode_by_name(fptr,&sb, &act_dir_inode, &file, file_name)) {
        printf("Path to file does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("nodeid:%d | file_size:%d\n", file.nodeid, file.file_size);
    print_file_info(&file, &act_dir_inode, fptr, &sb);

    fclose(fptr);
}

int outcp(char *name, pseudo_inode *act_path_inode, char *s1, char *s2){
    char file_name[12];
    char *path;

    //parsovani nazvu souboru - cesty
    char *path_parse_tmp1 = strdup(s1);
    char *path_parse_tmp2 = strdup(s1);
    path = dirname(path_parse_tmp1);
    char * tmp_file_name = basename(path_parse_tmp2);
    if(strlen(tmp_file_name)>12){
        printf("File name is too long. Maximum is 12, %d given.\n",strlen(tmp_file_name));
        return -1;
    }
    strcpy(file_name, tmp_file_name);
    //printf("path: |%s| filename:|%s|\n", path, file_name);


    FILE *fptr = fopen(name, "rb+");
    if (fptr == NULL) {
        printf("%s |%s|\n", strerror(errno), name);
        return -1;
    }
    superblock sb = {};
    fread(&sb, sizeof(sb), 1, fptr);

    //nastaveni cesty
    pseudo_inode act_dir_inode;
    if(!set_inode_by_path(&act_dir_inode, path, &sb, fptr, act_path_inode)) {
        printf("Path to dir does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("DIR: nodeid:%d | file_size:%d\n", act_dir_inode.nodeid, act_dir_inode.file_size);

    pseudo_inode file;
    if(!set_inode_by_name(fptr,&sb, &act_dir_inode, &file, file_name)) {
        printf("Path to file does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("nodeid:%d | file_size:%d\n", file.nodeid, file.file_size);
    if(export_file(&file, s2, fptr, &sb)){
        printf("Succesfully exported.\n");
        fclose(fptr);
    }else {
        fclose(fptr);
        return -1;
    }
}