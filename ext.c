#include "ext.h"

// funkce pro predvyplneni struktury sb s nastavenim velikosti disku
int fill_sb(superblock *sb, int disk_size) {
    strcpy(sb->signature, "<=vesely=>");
    strcpy(sb->volume_descriptor, "<==semestral ZOS===>");

    sb->cluster_size = CLUSTER_SIZE;                      // takže max. počet "souborových" položek na cluster jsou 2, protože sizeof(directory_item) = 16B
    sb->inodes_count = INODES_COUNT;                      //max pocet souboru v systemu, bylo by dobry aby to blyo delitolny 8, není nutné
    sb->inodes_free_count = sb->inodes_count;             //pocet volnych mist pro soubory
    sb->bitmapi_size = BITMAPI_SIZE;                      //(int)ceil(sb->inodes_count / 8);  chtel sem udelat dynamicky, ale neni to mozne
    sb->bitmap_size =  BITMAP_SIZE;                       //(int)ceil(sb->cluster_count / 8);

    sb->bitmapi_start_address = sizeof(superblock);                                  // konec sb
    sb->bitmap_start_address = sb->bitmapi_start_address + sb->bitmapi_size;  // konec bitmapy inodes, zaokrouhluje vždy nahoru
    sb->inode_start_address = sb->bitmap_start_address + sb->bitmap_size;   // konec bit mapy clusteru, viz. inode_bitmap,
    // jeden cluster je jeden bit (obsazeno/volne), stejně tak i inodex
    sb->data_start_address = sb->inode_start_address + sb->inodes_count * sizeof(pseudo_inode);    //konec inodes, data konci na disk_size

    sb->cluster_count = (int) floor(disk_size - sb->data_start_address)/CLUSTER_SIZE;   //zbyle misto vydelim velikosti clusteru, dostanu pocet clusterů
    sb->cluster_free_count = sb->cluster_count;

    if(disk_size <= (sb->data_start_address + 2 * CLUSTER_SIZE)) {
        printf("Given disk size (%d) is too small. Make it bigger than %d[B]\n", disk_size, (sb->data_start_address + 2 * CLUSTER_SIZE));
        return EXIT_FAILURE;
    }

    sb->disk_size = sb->data_start_address + (sb->cluster_count * sb->cluster_size);   //zadana velikost nemusi byt realna velikost,
    // protoze (disk_size - data_start_address) nemusi byt delitelna velikosti clusteru
    printf("Actual size of disk:%dB (%d clusters [%dB] + %d)\n",sb->disk_size, sb->cluster_count, sb->cluster_size, sb->data_start_address);
    return 0;
}

void printSb(superblock *sb){
    printf("print superblock\n");
    printf("sizeof(superblock) %d\n", sizeof(superblock));
    printf("signature %s [%d]\n", sb->signature, sizeof(sb->signature));
    printf("volume_descriptor %s [%d]\n", sb->volume_descriptor, sizeof(sb->volume_descriptor));
    printf("disk_size %d\n", sb->disk_size);
    printf("cluster_size %d\n", sb->cluster_size);
    printf("cluster_count %d\n", sb->cluster_count);
    printf("cluster_free_count %d\n", sb->cluster_free_count);
    printf("inodes_count %d\n", sb->inodes_count);
    printf("inodes_free_count %d\n", sb->inodes_free_count);
    printf("bitmapi_start_address %d\n", sb->bitmapi_start_address);
    printf("bitmap_start_address %d\n", sb->bitmap_start_address);
    printf("inode_start_address %d\n", sb->inode_start_address);
    printf("data_start_address %d\n", sb->data_start_address);
}

long int findSize(char *file_name)
{
    // opening the file in read mode
    FILE* fp = fopen(file_name, "r");

    // checking if the file exist or not
    if (fp == NULL) {
        printf("File Not Found!\n");
        return -1;
    }

    fseek(fp, 0L, SEEK_END);

    // calculating the size of the file
    long int res = ftell(fp);

    // closing the file
    fclose(fp);

    return res;
}

int find_empty_data_node_in_bitmap(u_char data_bitmap){
    for (int j=7; j>=0; j--) {
        if(((data_bitmap >> j) & 0b1) == 0)
            return 7-j;
    }
    return -1;
}

int set_bit_on_position_in_bitmap(u_char data_bitmap, int position, int bit){
    int i;
    u_char temp = 0b10000000;     //chci nastavit pozici na 1 (0->1), pokud chci nastavit pozici na 0 (1->0), tak pouziju jeji negaci
    for(i = 0; i <= position; i++){
        if(position == i) {
            if (bit == 1) {
                data_bitmap = data_bitmap | temp;
            }else{
                data_bitmap = data_bitmap & (~temp);
            }
        }
        temp = temp >> 1;
    }
    return data_bitmap;
}

int get_needed_clusters(int file_size){
    return ((int) (file_size>0 ? ceil((double)file_size / (double)CLUSTER_SIZE) : 0));
}

void printBits(size_t const size, void const * const ptr)
{
    unsigned char *b = (unsigned char*) ptr;
    unsigned char byte;
    int i, j;

    for (i = size-1; i >= 0; i--) {
        for (j = 7; j >= 0; j--) {
            byte = (b[i] >> j) & 1;
            printf("%u", byte);
        }
    }
    puts("");
}

int get_num_of_char_in_string(char *string, char find){
    int count = 0;
    char * tmp;
    tmp=strchr(string,find);
    while (tmp!=NULL){
        tmp=strchr(tmp+1,'s');
        count++;
    }
    return count;
}

void set_inode_by_nodeid(FILE *fptr,superblock *sb, int nodeid, pseudo_inode *inode){
    fseek(fptr, sb->inode_start_address                            //zacatek dat inodu
                + (nodeid * sizeof(pseudo_inode) ) , SEEK_SET); //zacatek dat slozky
    fread(inode, sizeof(pseudo_inode), 1, fptr);
}

bool set_dir_by_name(FILE *fptr,superblock *sb, pseudo_inode *act_dir_inode, pseudo_inode *dir_inode_to_set, char *name){
    bool found = false;
    for (int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
        directory_item act_dir_content_file;
        fseek(fptr, sb->data_start_address                          //zacatek dat
                    + (act_dir_inode->direct1 * CLUSTER_SIZE ) //zacatek dat slozky
                    + (i * sizeof(directory_item) ), SEEK_SET); //zacatek dat dir itemu
        fread(&act_dir_content_file, sizeof(directory_item), 1, fptr);
        if(strcmp(act_dir_content_file.item_name, name) == 0){
            pseudo_inode tmp;
            set_inode_by_nodeid(fptr, sb, act_dir_content_file.inode, &tmp);
            if(tmp.isDirectory) {
                *dir_inode_to_set = tmp;
                return true;
            }
        }
    }

    return found;
}

bool set_file_by_name(FILE *fptr,superblock *sb, pseudo_inode *act_dir_inode, pseudo_inode *dir_inode_to_set, char *name){
    bool found = false;
    for (int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
        directory_item act_dir_content_file;
        fseek(fptr, sb->data_start_address                          //zacatek dat
                    + (act_dir_inode->direct1 * CLUSTER_SIZE ) //zacatek dat slozky
                    + (i * sizeof(directory_item) ), SEEK_SET); //zacatek dat dir itemu
        fread(&act_dir_content_file, sizeof(directory_item), 1, fptr);
        //printf("found dir_item-> nodeid:%d | name:%s\n",act_dir_content_file.inode, act_dir_content_file.item_name);
        if(strcmp(act_dir_content_file.item_name, name) == 0){
            pseudo_inode tmp;
            set_inode_by_nodeid(fptr, sb, act_dir_content_file.inode, &tmp);
            if(tmp.isDirectory == TYPE_SLINK || tmp.isDirectory == TYPE_FILE) {
                *dir_inode_to_set = tmp;
                return true;
            }
        }
    }

    return found;
}

bool set_inode_by_name(FILE *fptr,superblock *sb, pseudo_inode *act_dir_inode, pseudo_inode *dir_inode_to_set, char *name){
    bool found = false;
    for (int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
        directory_item act_dir_content_file;
        fseek(fptr, sb->data_start_address                          //zacatek dat
                    + (act_dir_inode->direct1 * CLUSTER_SIZE ) //zacatek dat slozky
                    + (i * sizeof(directory_item) ), SEEK_SET); //zacatek dat dir itemu
        fread(&act_dir_content_file, sizeof(directory_item), 1, fptr);
        //printf("found dir_item-> nodeid:%d | name:%s\n",act_dir_content_file.inode, act_dir_content_file.item_name);
        if(strcmp(act_dir_content_file.item_name, name) == 0){
            pseudo_inode tmp;
            set_inode_by_nodeid(fptr, sb, act_dir_content_file.inode, &tmp);
            *dir_inode_to_set = tmp;
            return true;
        }
    }

    return found;
}

void parse_first_dir_from_path(char *path, char **target){
    *target = strtok(path, "/");
}

bool set_inode_by_path(pseudo_inode *act_dir_inode, char *path, superblock *sb, FILE *fptr, pseudo_inode *act_path_inode){
    bool result = false;
    if(act_dir_inode == NULL)
        *act_dir_inode = *act_path_inode;

    if(strcmp(path, "/") == 0){ //nastavim na root
        act_dir_inode->nodeid=0;
        act_dir_inode->isDirectory=true;
        act_dir_inode->references=1;
        act_dir_inode->file_size=CLUSTER_SIZE;
        act_dir_inode->direct1=0;
        result = true;
    }else {
        int num_of_dirs_in_path = get_num_of_char_in_string(path, '/');
        if (num_of_dirs_in_path > 0) {
            for (int i = 0; i < num_of_dirs_in_path; i++) {
                parse_first_dir_from_path(dirname(path), &path);
                set_inode_by_path(act_dir_inode, path, sb, fptr, act_dir_inode);
            }
        } else {
            if (strcmp(path, ".") == 0) {     //aktualni adresar
                *act_dir_inode = *act_path_inode;
                result = true;
            } else if (strcmp(path, "..") == 0) {  //rodic
                directory_item tmp;
                fseek(fptr, sb->data_start_address                                  //zacatek dat
                            + (act_path_inode->direct1 * CLUSTER_SIZE),
                      SEEK_SET);  //zacatek dat slozky (nazacaku je dir item parent)
                fread(&tmp, sizeof(directory_item), 1, fptr);
                set_inode_by_nodeid(fptr, sb, tmp.inode, act_dir_inode);
                result = true;
            } else {  //slozka v act adresari
                result = set_dir_by_name(fptr, sb, act_path_inode, act_dir_inode, path); //path = nazev hledane slozky
            }
        }
    }

    return result;
}

void print_file_content(pseudo_inode *file, FILE *fptr, superblock *sb){
    u_char buffer[CLUSTER_SIZE+1];
    memset(buffer, '\0', sizeof(buffer));
    printf("Printing file:\n");
    //printf("1:%d 2:%d 3:%d 4:%d 5:%d\n",file->direct1, file->direct2, file->direct3, file->direct4, file->direct5);
    if(file->direct1 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct1 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        printf("%s",buffer);
    }
    if(file->direct2 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct2 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        printf("%s",buffer);
    }
    if(file->direct3 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct3 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        printf("%s",buffer);
    }
    if(file->direct4 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct4 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        printf("%s",buffer);
    }
    if(file->direct5 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct5 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        printf("%s",buffer);
    }

    printf("\n");
}

bool set_diritem_by_inode(FILE *fptr, superblock *sb, int file_nodeid, pseudo_inode *file_parent, directory_item *file_dir_item){
    directory_item tmp;
    for(int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
        fseek(fptr, sb->data_start_address + (file_parent->direct1 * CLUSTER_SIZE) + (i * sizeof(directory_item)), SEEK_SET);  //nastevni fseek na zavatek dat rodicovskeho adresare
        fread(&tmp, sizeof(directory_item) , 1, fptr);
        if(tmp.inode == file_nodeid){
            *file_dir_item = tmp;
            return true;
        }
    }

    return false;
}

void print_file_info(pseudo_inode *file, pseudo_inode *file_parent, FILE *fptr, superblock *sb){
    directory_item file_dir_item;
    if(set_diritem_by_inode(fptr, sb, file->nodeid, file_parent, &file_dir_item)) {
        printf("name:%s - size:%d[B] - i-node:%d", file_dir_item.item_name, file->file_size, file->nodeid);
        if (file->direct1 != ID_CLUESTER_FREE) {
            printf(" - direct1:%d", file->direct1);
        }
        if (file->direct2 != ID_CLUESTER_FREE) {
            printf(" - direct2:%d", file->direct2);
        }
        if (file->direct3 != ID_CLUESTER_FREE) {
            printf(" - direct3:%d", file->direct3);
        }
        if (file->direct4 != ID_CLUESTER_FREE) {
            printf(" - direct4:%d", file->direct4);
        }
        if (file->direct5 != ID_CLUESTER_FREE) {
            printf(" - direct5:%d", file->direct5);
        }
        printf("\n");
    }else{
        printf("Info cant be printed. Cannot find directory item.\n");
    }

}

bool export_file(pseudo_inode *file, char *target_name, FILE *fptr, superblock *sb){
    remove(target_name);
    FILE *target = fopen(target_name, "wb");
    if (target == NULL) {
        printf("%s |%s|\n", strerror(errno), target_name);
        return false;
    }

    u_char buffer[CLUSTER_SIZE+1];
    memset(buffer, '\0', sizeof(buffer));
    if(file->direct1 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct1 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        fwrite(&buffer, strlen(buffer), 1, target);
    }
    if(file->direct2 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct2 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        fwrite(&buffer, strlen(buffer), 1, target);
    }
    if(file->direct3 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct3 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        fwrite(&buffer, strlen(buffer), 1, target);
    }
    if(file->direct4 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct4 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        fwrite(&buffer, strlen(buffer), 1, target);
    }
    if(file->direct5 != ID_CLUESTER_FREE) {
        fseek(fptr, sb->data_start_address + (file->direct5 * CLUSTER_SIZE), SEEK_SET);
        fread(&buffer, CLUSTER_SIZE, 1, fptr);
        fwrite(&buffer, strlen(buffer), 1, target);
    }
    fclose(target);
    return true;
}

bool delete_file(pseudo_inode *file, pseudo_inode *file_parent, FILE *fptr, superblock *sb) {
        //printf("size:%d[B] - i-node:%d", file->file_size, file->nodeid);

        //mazání dat + uprava datove bitmapy
        char data_placeholder[sb->cluster_size];  //32 = sizeof cluster
        strcpy(data_placeholder, "<============volna_data======================volna_data========>");

        u_char data_bitmap [sb->bitmap_size];
        fseek(fptr, sb->bitmap_start_address, SEEK_SET);
        fread(&data_bitmap, sizeof(data_bitmap), 1, fptr);
        int data_bitmap_id;

        if (file->direct1 != ID_CLUESTER_FREE) {
            fseek(fptr, sb->data_start_address + (file->direct1 * CLUSTER_SIZE), SEEK_SET);
            fwrite(&data_placeholder, sizeof(data_placeholder), 1, fptr);

            data_bitmap_id = (int) floor((double) file->nodeid / 8.0);
            data_bitmap[data_bitmap_id] = set_bit_on_position_in_bitmap(data_bitmap[data_bitmap_id], file->direct1, 0);
            sb->cluster_free_count++;
        }
        if (file->direct2 != ID_CLUESTER_FREE) {
            fseek(fptr, sb->data_start_address + (file->direct2 * CLUSTER_SIZE), SEEK_SET);
            fwrite(&data_placeholder, sizeof(data_placeholder), 1, fptr);

            data_bitmap_id = (int) floor((double) file->nodeid / 8.0);
            data_bitmap[data_bitmap_id] = set_bit_on_position_in_bitmap(data_bitmap[data_bitmap_id], file->direct2, 0);
            sb->cluster_free_count++;
        }
        if (file->direct3 != ID_CLUESTER_FREE) {
            fseek(fptr, sb->data_start_address + (file->direct3 * CLUSTER_SIZE), SEEK_SET);
            fwrite(&data_placeholder, sizeof(data_placeholder), 1, fptr);

            data_bitmap_id = (int) floor((double) file->nodeid / 8.0);
            data_bitmap[data_bitmap_id] = set_bit_on_position_in_bitmap(data_bitmap[data_bitmap_id], file->direct3, 0);
            sb->cluster_free_count++;
        }
        if (file->direct4 != ID_CLUESTER_FREE) {
            fseek(fptr, sb->data_start_address + (file->direct4 * CLUSTER_SIZE), SEEK_SET);
            fwrite(&data_placeholder, sizeof(data_placeholder), 1, fptr);

            data_bitmap_id = (int) floor((double) file->nodeid / 8.0);
            data_bitmap[data_bitmap_id] = set_bit_on_position_in_bitmap(data_bitmap[data_bitmap_id], file->direct4, 0);
            sb->cluster_free_count++;
        }
        if (file->direct5 != ID_CLUESTER_FREE) {
            fseek(fptr, sb->data_start_address + (file->direct5 * CLUSTER_SIZE), SEEK_SET);
            fwrite(&data_placeholder, sizeof(data_placeholder), 1, fptr);

            data_bitmap_id = (int) floor((double) file->nodeid / 8.0);
            data_bitmap[data_bitmap_id] = set_bit_on_position_in_bitmap(data_bitmap[data_bitmap_id], file->direct5, 0);
            sb->cluster_free_count++;
        }
        fseek(fptr, sb->bitmap_start_address, SEEK_SET);
        fwrite(&data_bitmap, sizeof(u_char), sb->bitmapi_size, fptr);


        //mazání inode
        char inode_placeholder[sizeof( pseudo_inode)]; //40 = sizeof( pseudo_inode))
        strcpy(inode_placeholder, "<===========prazdny_inode==============>");
        fseek(fptr, sb->inode_start_address + (file->nodeid * sizeof(pseudo_inode)), SEEK_SET);
        fwrite(&inode_placeholder, sizeof(inode_placeholder), 1, fptr);
        sb->inodes_free_count++;

        //uprava bitmapy inodes
        int inode_bitmap_id = (int) floor((double) file->nodeid / 8.0);
        u_char inode_bitmap [sb->bitmapi_size];
        fseek(fptr, sb->bitmapi_start_address, SEEK_SET);
        fread(&inode_bitmap, sizeof(inode_bitmap), 1, fptr);
        inode_bitmap[inode_bitmap_id] = set_bit_on_position_in_bitmap(inode_bitmap[inode_bitmap_id], file->nodeid, 0);
        fseek(fptr, sb->bitmapi_start_address, SEEK_SET);
        fwrite(&inode_bitmap, sizeof(u_char), sb->bitmapi_size, fptr);


        //mazani dir item
        for(int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
            directory_item tmp;
            fseek(fptr, sb->data_start_address                  //data
                        + (file_parent->direct1 * CLUSTER_SIZE)        //data slozky rodice souboru
                        + (i * sizeof(directory_item) ), SEEK_SET);    //konkretni dir item ve slozce
            fread(&tmp, sizeof(directory_item), 1, fptr);
            //printf("tmp.inode:%d == file->nodeid:%d\n",tmp.inode, file->nodeid);
            if(tmp.inode == file->nodeid){
                tmp.inode = ID_ITEM_FREE;
                strcpy(tmp.item_name, "<=filename=>");
                fseek(fptr, sb->data_start_address                  //data
                            + (file_parent->direct1 * CLUSTER_SIZE)        //data slozky rodice souboru
                            + (i * sizeof(directory_item) ), SEEK_SET);    //konkretni dir item ve slozce
                fwrite(&tmp, sizeof(directory_item), 1, fptr);
                break;
            }
        }

        fseek(fptr, 0, SEEK_SET);
        fwrite(sb, sizeof(*sb), 1, fptr);

    return true;
}
