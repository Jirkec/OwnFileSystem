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
    for(int i = 0; i < FILES_IN_FOLDER_COUNT -1; i++) {
        directory_item empty = {ID_ITEM_FREE, "<=filename=>"};
        fwrite(&empty, sizeof(empty), 1, fptr);
    }
    char data_placeholder[sb.cluster_size];  //32 = sizeof cluster
    strcpy(data_placeholder, "<============volna_data======================volna_data========>");

    //zbytek dat
    //printf("cluster_count:%d\n",sb.cluster_count);
    for (int i = 0; i < sb.cluster_count - 1; i++) {
        fwrite(&data_placeholder, sizeof(data_placeholder), 1, fptr);
    }

    fclose(fptr);
    printf("Succesfully formated\n");
    return 1;
}

int incp(char *name, pseudo_inode *act_path_inode, char *s1, char *s2, bool printMsg, bool slink){
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
        printf("%s zde|%s|\n", strerror(errno), name);
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
    for (int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
        directory_item act_dir_content_file;
        fseek(fptr, sb.data_start_address
                    + (act_dir_inode.direct1 * CLUSTER_SIZE)
                    + (i * sizeof(directory_item) ), SEEK_SET); //nastaveni fseek na data aktualni slozky
        fread(&act_dir_content_file, sizeof(directory_item), 1, fptr);
        if(act_dir_content_file.inode == ID_ITEM_FREE){
            act_dir_free_file_nodeid = act_dir_content_file.inode;
            //printf("act_dir_free_file_nodeid:%d\n",act_dir_free_file_nodeid);
            break;
        }
        act_dir_free_file_id++;
    }
    if(act_dir_free_file_nodeid == ID_ITEM_FREE) {

        //overeni, ze soubor jeste neexistuje
        pseudo_inode file_exists;
        if(!set_inode_by_name(fptr, &sb, &act_dir_inode, &file_exists, file_name)) {
            FILE *fptr1 = fopen(s1, "rb");
            if (fptr1 == NULL) {
                printf("Something went wrong with read()! %s |%s|\n", strerror(errno), s1);
                fclose(fptr);
                return -1;
            }

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
                            inode_bitmap[i] = set_bit_on_position_in_bitmap(inode_bitmap[i], (new_nodeid - (i * 8)), 1);
                            break;
                        }
                    }

                    directory_item new_file = {new_nodeid,
                                               "<=filename=>"}; //nejdriv placehordel a potom nakopiruju nazev
                    strcpy(new_file.item_name, file_name);
                    //nastevni cluster bitmapy - zatim pocitam s tim, že data se vejdou do 5 clusteru TODO - overit, ze neni potreba undirected
                    int new_file_clusters[DIRECT_LINK_COUNT];    // id clusteru noveho souboru - pujde pak do vytvareni pseudo_inode
                    for (int i = 0; i < DIRECT_LINK_COUNT; i++) {
                        new_file_clusters[i] = ID_CLUESTER_FREE;
                    }
                    //nastavim na -1, pokud je -1 znamena, že není provázán
                    if (needed_cluster_count > 0) {
                        for (int j = 0; j < needed_cluster_count; j++) {
                            for (int i = 0; i < sb.bitmap_size; i++) {
                                if (data_bitmap[i] < 0b11111111) {
                                    int bitmap_position;
                                    bitmap_position = find_empty_data_node_in_bitmap(data_bitmap[i]);
                                    new_file_clusters[j] = bitmap_position + (i * 8);    //pridam do pole novych direct clusteru
                                    data_bitmap[i] = set_bit_on_position_in_bitmap(data_bitmap[i], bitmap_position, 1);
                                    break;
                                }
                            }
                        }
                    }
                    int8_t file_type = slink ? TYPE_SLINK : TYPE_FILE;
                    pseudo_inode new_file_inode = {new_nodeid, file_type, 1, file_size, new_file_clusters[0],
                                                   new_file_clusters[1], new_file_clusters[2],
                                                   new_file_clusters[3], new_file_clusters[4]};

                    sb.inodes_free_count--;
                    sb.cluster_free_count = sb.cluster_free_count - needed_cluster_count;

                    //zapis dat - superblock, bitmapy, uprava dir_item ve slozce
                    fseek(fptr, 0, SEEK_SET);   //nastaveni fseek na zacatek
                    fwrite(&sb, sizeof(sb), 1, fptr);
                    fwrite(&inode_bitmap, sizeof(u_char), sb.bitmapi_size, fptr);
                    fwrite(&data_bitmap, sizeof(u_char), sb.bitmap_size, fptr);
                    fseek(fptr, sb.inode_start_address + (new_nodeid * sizeof(pseudo_inode)),
                          SEEK_SET);   //nastaveni fseeku na pozici noveho inodu
                    fwrite(&new_file_inode, sizeof(new_file_inode), 1, fptr);
                    fseek(fptr,
                          sb.data_start_address                                                   //adresa na zacatek dat
                          +
                          (act_dir_inode.direct1 * CLUSTER_SIZE)                               //na data aktualni slozky
                          + (act_dir_free_file_id * sizeof(directory_item)), SEEK_SET);   //na data konkretniho souboru
                    fwrite(&new_file, sizeof(new_file), 1, fptr);

                    //zapis dat do clusteru
                    unsigned char buffer[CLUSTER_SIZE] = {0};
                    int new_file_clusters_increment = 0;
                    while ((fread(buffer, 1, sizeof(buffer), fptr1)) > 0) {
                        int cluster_ofset = new_file_clusters[new_file_clusters_increment];
                        if (cluster_ofset > ID_CLUESTER_FREE) {   //overeni, ze vybrany direct neni -1
                            fseek(fptr, sb.data_start_address + (cluster_ofset * CLUSTER_SIZE),
                                  SEEK_SET); //nastaveni fseek na spravnou pozici idclusteru * velikost cluster od zacatku dat
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
                    if (printMsg)
                        printf("File succesfully uploaded.\n");
                    return 1;
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
            printf("File couldnt be upload. File already exists.\n");
            fclose(fptr);
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

    //printf("act_dir_inode.inodeid:%d | file_name:%s\n", act_dir_inode.nodeid, file_name);
    pseudo_inode file;
    if(!set_file_by_name(fptr,&sb, &act_dir_inode, &file, file_name)) {
        printf("Path to file does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("nodeid:%d | file_size:%d\n", file.nodeid, file.file_size);
    if(file.isDirectory != TYPE_SLINK)
        print_file_content(&file, fptr, &sb);
    else{
        int32_t real_inode_id;
        fseek(fptr, sb.data_start_address + (file.direct1 * CLUSTER_SIZE), SEEK_SET);
        fread(&real_inode_id, sizeof(real_inode_id),1, fptr);
        if(real_inode_id > ID_ITEM_FREE){
            pseudo_inode real_inode;
            set_inode_by_nodeid(fptr, &sb, real_inode_id, &real_inode);
            print_file_content(&real_inode, fptr, &sb);
        }
    }

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

int outcp(char *name, pseudo_inode *act_path_inode, char *s1, char *s2, bool printMsg){
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
    if(file.isDirectory == TYPE_SLINK){
        printf("Opration failed. Selected file is slink.\n");
        fclose(fptr);
        return -1;
    }

    //printf("nodeid:%d | file_size:%d\n", file.nodeid, file.file_size);
    if(export_file(&file, s2, fptr, &sb)){
        if(printMsg)
            printf("Succesfully exported.\n");
        fclose(fptr);
    }else {
        fclose(fptr);
        return -1;
    }
}

int rm(char *name, pseudo_inode *act_path_inode, char *s1, bool printMsg){
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
    if(delete_file(&file, &act_dir_inode, fptr, &sb)){
        if(printMsg)
            printf("Succesfully deleted.\n");
    }else {
        fclose(fptr);
        return -1;
    }

    fclose(fptr);
    return 1;
}

int cp(char *name, pseudo_inode *act_path_inode, char *s1, char *s2, bool printMsg){
    char *export_path = "tmp/temp_copy_file.tmp";

    if(outcp(name, act_path_inode, s1, export_path, false) == -1)
        return -1;
    if(incp(name, act_path_inode, export_path, s2, false, false) == -1)
        return -1;

    remove(export_path);
    if(printMsg)
        printf("Succesfully copied.\n");

    return 1;
}

int mv(char *name, pseudo_inode *act_path_inode, char *s1, char *s2){
    if(cp(name, act_path_inode, s1, s2, false) == -1) {
        //printf("cp returned -1");
        return -1;
    }
    if(rm(name, act_path_inode, s1, false) == -1) {
        //printf("rm returned -1");
        return -1;
    }

    printf("Succesfully moved.\n");
    return 1;
}

int mkdir(char *name, pseudo_inode *act_path_inode, char *s1){
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
    printf("nodeid:%d | isDirectory:%d | act_dir_inode.direct1:%d\n",act_dir_inode.nodeid, act_dir_inode.isDirectory, act_dir_inode.direct1);

    //overeni, ze lze pridat soubor do cilove slozky
    int act_dir_free_file_id = 0;
    int act_dir_free_file_nodeid;
    for (int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
        directory_item act_dir_content_file;
        fseek(fptr, sb.data_start_address
                        + (act_dir_inode.direct1 * CLUSTER_SIZE)
                        + (i * sizeof(directory_item) ), SEEK_SET); //nastaveni fseek na data aktualni slozky
        fread(&act_dir_content_file, sizeof(directory_item), 1, fptr);
        if(act_dir_content_file.inode == ID_ITEM_FREE){
            act_dir_free_file_nodeid = act_dir_content_file.inode;
            //printf("act_dir_free_file_nodeid:%d\n",act_dir_free_file_nodeid);
            break;
        }
        act_dir_free_file_id++;
    }
    if(act_dir_free_file_nodeid == ID_ITEM_FREE) {

        //overeni, ze soubor jeste neexistuje
        pseudo_inode file_exists;
        if(!set_inode_by_name(fptr, &sb, &act_dir_inode, &file_exists, file_name)) {

            if (sb.inodes_free_count > 0 && sb.cluster_free_count > 0) {
                long int file_size = sb.cluster_size;
                //printf("needed_cluster_count:%d\n",needed_cluster_count);
                if (sb.cluster_free_count >= 1) {
                    //nasteni nove bitmapy inodes
                    int new_nodeid;
                    for (int i = 0; i < sb.bitmapi_size; i++) {
                        if (inode_bitmap[i] < 0b11111111) {
                            new_nodeid = find_empty_data_node_in_bitmap(inode_bitmap[i]) + (i * 8);
                            inode_bitmap[i] = set_bit_on_position_in_bitmap(inode_bitmap[i], (new_nodeid - (i * 8)), 1);
                            break;
                        }
                    }

                    directory_item new_file = {new_nodeid,"<=filename=>"}; //nejdriv placehordel a potom nakopiruju nazev
                    strcpy(new_file.item_name, file_name);
                    //nastevni cluster bitmapy
                    int new_clusterid;
                    for (int i = 0; i < sb.bitmap_size; i++) {
                        if (data_bitmap[i] < 0b11111111) {
                            new_clusterid = find_empty_data_node_in_bitmap(data_bitmap[i]) + (i * 8);
                            data_bitmap[i] = set_bit_on_position_in_bitmap(data_bitmap[i], (new_clusterid - (i * 8)), 1);
                            break;
                        }
                    }
                    pseudo_inode new_file_inode = {new_nodeid, true, 1, file_size, new_clusterid,
                                                   ID_CLUESTER_FREE, ID_CLUESTER_FREE,
                                                   ID_CLUESTER_FREE, ID_CLUESTER_FREE};

                    sb.inodes_free_count--;
                    sb.cluster_free_count--;

                    //zapis dat - superblock, bitmapy, uprava dir_item ve slozce rodice
                    fseek(fptr, 0, SEEK_SET);   //nastaveni fseek na zacatek
                    fwrite(&sb, sizeof(sb), 1, fptr);
                    fwrite(&inode_bitmap, sizeof(u_char), sb.bitmapi_size, fptr);
                    fwrite(&data_bitmap, sizeof(u_char), sb.bitmap_size, fptr);
                    fseek(fptr, sb.inode_start_address + (new_nodeid * sizeof(pseudo_inode)),SEEK_SET);   //nastaveni fseeku na pozici noveho inodu
                    fwrite(&new_file_inode, sizeof(new_file_inode), 1, fptr);
                    fseek(fptr,sb.data_start_address                                                   //adresa na zacatek dat
                          +(act_dir_inode.direct1 * CLUSTER_SIZE)                               //na data aktualni slozky
                          +(act_dir_free_file_id * sizeof(directory_item)), SEEK_SET);   //na data konkretniho souboru
                    fwrite(&new_file, sizeof(new_file), 1, fptr);


                    // zapis dat data slozky, prvni je odkaz sam na sebe
                    fseek(fptr, sb.data_start_address +(new_file_inode.direct1 * CLUSTER_SIZE), SEEK_SET);  //nastaveni na zacatek dat nove slozky
                    directory_item parent_dir = {act_dir_inode.nodeid, ".."};
                    fwrite(&parent_dir, sizeof(parent_dir), 1, fptr);
                    for(int i = 0; i < FILES_IN_FOLDER_COUNT -1; i++) {         //vyplneni slozky volnymi soubory
                        directory_item empty = {ID_ITEM_FREE, "<=filename=>"};
                        fwrite(&empty, sizeof(empty), 1, fptr);
                    }

                    //konec - zavreni souboru
                    fclose(fptr);
                    printf("Directory succesfully created.\n");
                    return 1;

                } else {
                    printf("Directory couldnt be created. Not enough free space.\n");
                    fclose(fptr);
                    return -1;
                }
            } else {
                printf("Directory couldnt be created. Filesystem is full.\n");
                fclose(fptr);
                return -1;
            }
        }else{
            printf("Directory couldnt be created. Directory already exists.\n");
            fclose(fptr);
            return -1;
        }
    }else{
        printf("Directory couldnt be created. Selected folder is full.\n");
        fclose(fptr);
        return -1;
    }
}

int rmdir(char *name, pseudo_inode *act_path_inode, char *s1){
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
        printf("Path to parent dir does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("DIR: nodeid:%d | file_size:%d\n", act_dir_inode.nodeid, act_dir_inode.file_size);

    pseudo_inode file;
    if(!set_inode_by_name(fptr,&sb, &act_dir_inode, &file, file_name)) {
        printf("Path to dir does not exist.\n");
        fclose(fptr);
        return -1;
    }

    for(int i = 1; i < FILES_IN_FOLDER_COUNT -1; i++) {
        directory_item tmp;
        fseek(fptr, sb.data_start_address + file.direct1 * CLUSTER_SIZE + (i * sizeof(directory_item)), SEEK_SET);
        fread(&tmp, sizeof(directory_item), 1, fptr);
        if (tmp.inode != ID_ITEM_FREE) {
            printf("Directory cannot be deleted. Directory is not empty.\n");
            return -1;
        }
    }
    //printf("nodeid:%d | file_size:%d\n", file.nodeid, file.file_size);
    if(delete_file(&file, &act_dir_inode, fptr, &sb)){
        printf("Succesfully deleted.\n");
    }else {
        fclose(fptr);
        return -1;
    }

    fclose(fptr);
    return 1;
}

int cd(char *name, pseudo_inode *act_path_inode, char *act_path, char *s1){
    FILE *fptr = fopen(name, "rb+");
    if (fptr == NULL) {
        printf("%s |%s|\n", strerror(errno), name);
        return -1;
    }
    superblock sb = {};
    fread(&sb, sizeof(sb), 1, fptr);

    pseudo_inode new_path_inode;
    if(!set_inode_by_path(&new_path_inode, s1, &sb, fptr, act_path_inode)){
        printf("Path does not exist.\n");
        fclose(fptr);
        return -1;
    }

    *act_path_inode = new_path_inode;
    fclose(fptr);
}

int ls(char *name, pseudo_inode *act_path_inode, char *s1){
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
        printf("Path to parent dir does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("DIR: nodeid:%d | file_size:%d\n", act_dir_inode.nodeid, act_dir_inode.file_size);

    int file_count = 0;
    for(int i = 1; i < FILES_IN_FOLDER_COUNT-1; i++){
        directory_item tmp;
        fseek(fptr, sb.data_start_address + (act_dir_inode.direct1 * CLUSTER_SIZE) + (i * sizeof(directory_item) ), SEEK_SET);
        fread(&tmp, sizeof(directory_item), 1, fptr);
        //printf("name:%s | node_id:%d \n", tmp.item_name, tmp.inode);
        if(tmp.inode != ID_ITEM_FREE && tmp.inode != act_dir_inode.nodeid) {
            pseudo_inode tmp_inode;
            set_inode_by_nodeid(fptr, &sb, tmp.inode, &tmp_inode);
            printf("name:%s | node_id:%d | isDirectory:%d | file size:%d\n", tmp.item_name, tmp.inode,tmp_inode.isDirectory, tmp_inode.file_size);
            file_count++;
        }
    }

    if(file_count == 0){
        printf("Directory is empty.\n");
    }
    return 1;
}

int slink(char *name, pseudo_inode *act_path_inode, char *s1, char *s2){
    char file_name_1[12];
    char *path_1;

    //parsovani nazvu souboru - cesty
    char *path_parse_tmp11 = strdup(s2);
    char *path_parse_tmp12 = strdup(s2);
    path_1 = dirname(path_parse_tmp11);
    char * tmp_file_name1 = basename(path_parse_tmp12);
    if(strlen(tmp_file_name1)>12){
        printf("File name is too long. Maximum is 12, %d given.\n",strlen(tmp_file_name1));
        return -1;
    }
    strcpy(file_name_1, tmp_file_name1);
    //printf("path: |%s| filename:|%s|\n", path, file_name);


    FILE *fptr = fopen(name, "rb+");
    if (fptr == NULL) {
        printf("%s :) |%s|\n", strerror(errno), name);
        return -1;
    }
    superblock sb = {};
    fread(&sb, sizeof(sb), 1, fptr);

    //nastaveni cesty
    pseudo_inode act_dir_inode_1;   //dir inodu na ktery bude ukazovano
    if(!set_inode_by_path(&act_dir_inode_1, path_1, &sb, fptr, act_path_inode)) {
        printf("Path dir to linked file does not exist.\n");
        fclose(fptr);
        return -1;
    }
    pseudo_inode file;  //inode na ktery bude ukazovano
    if(!set_file_by_name(fptr,&sb, &act_dir_inode_1, &file, file_name_1)) {
        printf("Path to file does not exist.\n");
        fclose(fptr);
        return -1;
    }
    //printf("nodeid:%d | isDirectory:%d\n",act_dir_inode.nodeid, act_dir_inode.isDirectory);

    char *tmp_file_slink_data = "tmp/temp_slink_file.tmp";
    remove(tmp_file_slink_data);
    FILE *fptr_tmp = fopen(tmp_file_slink_data, "wb");
    if (fptr_tmp == NULL) {
        printf("%s |%s|\n", strerror(errno), tmp_file_slink_data);
        return -1;
    }
    fwrite(&file.nodeid, sizeof(file.nodeid), 1, fptr_tmp);
    fclose(fptr_tmp);
    fclose(fptr);

    if(incp(name, act_path_inode, tmp_file_slink_data, s1, false, true) == -1){
        printf("slink couldnt be created.\n");
        remove(tmp_file_slink_data);
        return -1;
    }

    remove(tmp_file_slink_data);
    printf("slink created.\n");
    return 1;
}

int load(char *name, char *s1, pseudo_inode *act_path_inode, char *act_path){
    char line[256];

    FILE *fptr = fopen(s1, "rb+");
    if (fptr == NULL) {
        printf("%s :) |%s|\n", strerror(errno), name);
        return -1;
    }
    while( fgets (line, 256, fptr)!=NULL ) {
        line[strcspn(line, "\n")] = '\0';
        printf("%s\n",line);
        resolve_command(line, name, act_path_inode, act_path);
    }
    fclose(fptr);
}

int resolve_command(char *line, char *name, pseudo_inode *act_path_inode, char *act_path){
    bool command_recognized = 0;
    char command[30];
    sscanf(line, "%s", command);
    //format
    if (strcmp(command, "format") == 0) {
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
            return -1;
    }


    //incp
    if (strcmp(command, "incp") == 0) {
        char s1[60];    //source na disku
        char s2[60];    //dest v fs.ext
        command_recognized = 1;
        sscanf(line, "%29s %59s %59s", command, s1, s2);

        pseudo_inode tmp_path_inode = *act_path_inode;
        if(incp(name, &tmp_path_inode, s1, s2, true, false) == -1)
            return -1;
    }

    //cat
    if (strcmp(command, "cat") == 0) {
        char s1[60];    //source v fs.ext
        command_recognized = 1;

        sscanf(line, "%29s %59s", command, s1);
        pseudo_inode tmp_path_inode = *act_path_inode;
        if(cat(name, &tmp_path_inode, s1) == -1)
            return -1;
    }

    //info
    if (strcmp(command, "info") == 0) {
        char s1[60];    //source v fs.ext
        command_recognized = 1;

        sscanf(line, "%29s %59s", command, s1);
        pseudo_inode tmp_path_inode = *act_path_inode;
        if(info(name, &tmp_path_inode, s1) == -1)
            return -1;
    }

    //rm
    if (strcmp(command, "rm") == 0) {
        char s1[60];    //source v fs.ext
        command_recognized = 1;

        sscanf(line, "%29s %59s", command, s1);
        pseudo_inode tmp_path_inode = *act_path_inode;
        if(rm(name, &tmp_path_inode, s1, true) == -1)
            return -1;
    }

    //outcp
    if (strcmp(command, "outcp") == 0) {
        char s1[60];    //source v fs.ext
        char s2[60];    //dest na disku
        command_recognized = 1;

        sscanf(line, "%29s %59s %59s", command, s1, s2);
        pseudo_inode tmp_path_inode = *act_path_inode;
        if(outcp(name, &tmp_path_inode, s1, s2, true) == -1)
            return -1;
    }

    //cp
    if (strcmp(command, "cp") == 0) {
        char s1[60];    //source v fs.ext
        char s2[60];    //dest v fs.ext
        command_recognized = 1;

        sscanf(line, "%29s %59s %59s", command, s1, s2);
        pseudo_inode tmp_path_inode = *act_path_inode;
        if(cp(name, &tmp_path_inode, s1, s2, true) == -1) {
            printf("Coping failed.\n");
            return -1;
        }
    }

    //mv
    if (strcmp(command, "mv") == 0) {
        char s1[60];    //source v fs.ext
        char s2[60];    //dest v fs.ext
        command_recognized = 1;

        sscanf(line, "%29s %59s %59s", command, s1, s2);
        pseudo_inode tmp_path_inode = *act_path_inode;
        if(mv(name, &tmp_path_inode, s1, s2) == -1) {
            printf("Move failed.\n");
            return -1;
        }
    }

    //mkdir
    if (strcmp(command, "mkdir") == 0) {
        char s1[60];    //source v fs.ext
        command_recognized = 1;

        sscanf(line, "%29s %59s", command, s1);
        pseudo_inode tmp_path_inode = *act_path_inode;
        if(mkdir(name, &tmp_path_inode, s1) == -1) {
            return -1;
        }
    }

    //rmdir
    if (strcmp(command, "rmdir") == 0) {
        char s1[60];    //source v fs.ext
        command_recognized = 1;

        sscanf(line, "%29s %59s", command, s1);
        pseudo_inode tmp_path_inode = *act_path_inode;
        if(rmdir(name, &tmp_path_inode, s1) == -1) {
            printf("Directory delete failed.\n");
            return -1;
        }
    }

    //cd
    if (strcmp(command, "cd") == 0) {
        char s1[60];    //pozadovana
        command_recognized = 1;

        sscanf(line, "%29s %59s", command, s1);
        if(cd(name, act_path_inode, act_path, s1) == -1) {
            return -1;
        }
    }

    //ls
    if (strcmp(command, "ls") == 0) {
        char s1[60];    //adresa slozky ktera se ma vypsat
        command_recognized = 1;

        sscanf(line, "%29s %59s", command, s1);
        if(ls(name, act_path_inode, s1) == -1) {
            return -1;
        }
    }

    //load
    if (strcmp(command, "load") == 0) {
        char s1[60];    //adresa slozky ktera se ma vypsat
        command_recognized = 1;

        sscanf(line, "%29s %59s", command, s1);
        if(load(name, s1, act_path_inode, act_path) == -1) {
            return -1;
        }
    }

    //slink
    if (strcmp(command, "slink") == 0) {
        char s1[60];    //source v fs.ext
        char s2[60];    //dest v fs.ext
        command_recognized = 1;

        sscanf(line, "%29s %59s %59s", command, s1, s2);
        pseudo_inode tmp_path_inode = *act_path_inode;
        if(slink(name, &tmp_path_inode, s1, s2) == -1) {
            return -1;
        }
    }

    //pwd
    if (strcmp(command, "pwd") == 0) {
        command_recognized = 1;
        printf("%s\n",act_path);
    }

    //exit
    if (strcmp(command, "exit") == 0) {
        command_recognized = 1;
        return 2;
    }

    //help
    if (strcmp(command, "help") == 0) {
        command_recognized = 1;
        printf("Commands are:\n");
        printf(" 1  format <size>\n");
        printf(" 2  exit\n");
        printf(" 3  incp <source on disk> <dest in FS>\n");
    }

    if(!command_recognized){
        printf("Command not recognized. Use 'help' command.\n");
    }
    return 1;
}