#include "ext.h"

typedef unsigned char u_char;

struct superblock {
    char signature[10];              //login autora FS
    char volume_descriptor[20];     //popis vygenerovaného FS
    int32_t disk_size;              //celkova velikost VFS
    int32_t cluster_size;           //velikost clusteru
    int32_t cluster_count;          //pocet clusteru
    int32_t cluster_free_count;     //pocet volnych clusteru
    int32_t inodes_count;           //pocet inodu
    int32_t inodes_free_count;      //pocet volndych inodu
    int32_t bitmapi_size;           //počet prvku pole bitmapy inodes
    int32_t bitmapi_start_address;  //adresa pocatku bitmapy i-uzlů
    int32_t bitmap_size;            //počet prvku pole bitmapy datových bloků (cluesterů)
    int32_t bitmap_start_address;   //adresa pocatku bitmapy datových bloků
    int32_t inode_start_address;    //adresa pocatku  i-uzlů
    int32_t data_start_address;     //adresa pocatku datovych bloku
};

struct pseudo_inode {
    int32_t nodeid;                 //ID i-uzlu, pokud ID = ID_ITEM_FREE, je polozka volna
    bool isDirectory;               //soubor, nebo adresar
    int8_t references;              //počet odkazů na i-uzel, používá se pro hardlinky
    int32_t file_size;              //velikost souboru v bytech
    int32_t direct1;                // 1. přímý odkaz na datové bloky
    int32_t direct2;                // 2. přímý odkaz na datové bloky
    int32_t direct3;                // 3. přímý odkaz na datové bloky
    int32_t direct4;                // 4. přímý odkaz na datové bloky
    int32_t direct5;                // 5. přímý odkaz na datové bloky
    int32_t indirect1;              // 1. nepřímý odkaz (odkaz - datové bloky)
    int32_t indirect2;              // 2. nepřímý odkaz (odkaz - odkaz - datové bloky)
};

struct directory_item {
    int32_t inode;                   // inode odpovídající souboru
    char item_name[12];              //8+3 + /0 C/C++ ukoncovaci string znak
};

// funkce pro predvyplneni struktury sb s nastavenim velikosti disku
int fill_sb(struct superblock *sb, int disk_size) {
   strcpy(sb->signature, "<=vesely=>");
   strcpy(sb->volume_descriptor, "<==semestral ZOS===>");

    sb->cluster_size = CLUSTER_SIZE;                      // takže max. počet "souborových" položek na cluster jsou 2, protože sizeof(directory_item) = 16B
    sb->inodes_count = INODES_COUNT;                      //max pocet souboru v systemu, bylo by dobry aby to blyo delitolny 8, není nutné
    sb->inodes_free_count = sb->inodes_count;             //pocet volnych mist pro soubory
    sb->bitmapi_size = BITMAPI_SIZE;                      //(int)ceil(sb->inodes_count / 8);  chtel sem udelat dynamicky, ale neni to mozne
    sb->bitmap_size =  BITMAP_SIZE;                       //(int)ceil(sb->cluster_count / 8);

    sb->bitmapi_start_address = sizeof(struct superblock);                                  // konec sb
    sb->bitmap_start_address = sb->bitmapi_start_address + sb->bitmapi_size;  // konec bitmapy inodes, zaokrouhluje vždy nahoru
    sb->inode_start_address = sb->bitmap_start_address + sb->bitmap_size;   // konec bit mapy clusteru, viz. inode_bitmap,
                                                                                            // jeden cluster je jeden bit (obsazeno/volne), stejně tak i inodex
    sb->data_start_address = sb->inode_start_address + sb->inodes_count * sizeof(struct pseudo_inode);    //konec inodes, data konci na disk_size

    sb->cluster_count = (int) floor(disk_size - sb->data_start_address)/CLUSTER_SIZE;   //zbyle misto vydelim velikosti clusteru, dostanu pocet clusterů
    sb->cluster_free_count = sb->cluster_count;

    if(disk_size <= (sb->data_start_address + 2 * CLUSTER_SIZE)) {
        printf("Given disk size (%d) is too small. Make it bigger than %d[B]\n", disk_size, (sb->data_start_address + 2 * CLUSTER_SIZE));
        return EXIT_FAILURE;
    }

    sb->disk_size = sb->data_start_address + (sb->cluster_count * sb->cluster_size);   //zadana velikost nemusi byt realna velikost,
                                                                                       // protoze (disk_size - data_start_address) nemusi byt delitelna velikosti clusteru
    printf("Actual size of disk:%dB (%d clusters [%dB])\n",sb->disk_size, sb->cluster_count, sb->cluster_size);
   return 0;
}

void printSb(struct superblock *sb){
    printf("print superblock\n");
    printf("sizeof(superblock) %d\n", sizeof(struct superblock));
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

void set_inode_by_nodeid(FILE *fptr,struct superblock *sb, int nodeid, struct pseudo_inode *inode){
    fseek(fptr, sb->inode_start_address                            //zacatek dat inodu
                + (nodeid * sizeof(struct pseudo_inode) ) , SEEK_SET); //zacatek dat slozky
    fread(inode, sizeof(struct pseudo_inode), 1, fptr);
}

bool set_dir_by_name(FILE *fptr,struct superblock *sb, struct pseudo_inode *act_dir_inode, char *name){
    bool found = false;
    for (int i = 0; i < FILES_IN_FOLDER_COUNT; i++) {
        struct directory_item act_dir_content_file;
        fseek(fptr, sb->data_start_address                          //zacatek dat
                        + (act_dir_inode->direct1 * sizeof(CLUSTER_SIZE) ) //zacatek dat slozky
                        + (i * sizeof(struct directory_item) ), SEEK_SET); //zacatek dat dir itemu
        fread(&act_dir_content_file, sizeof(struct directory_item), 1, fptr);
        if(strcmp(act_dir_content_file.item_name, name) == 0){
            found = true;
            struct pseudo_inode tmp;
            set_inode_by_nodeid(fptr, sb, act_dir_content_file.inode, &tmp);
        }
    }

    return found;
}

void parse_first_dir_from_path(char *path, char **target){
    *target = strtok(path, "/");
}

bool set_inode_by_path(struct pseudo_inode *act_dir_inode, char *path, struct superblock *sb, FILE *fptr, struct pseudo_inode *act_path_inode){
    bool result = false;
    if(strcmp(path, "/") == 0){ //nastavim na root
        act_dir_inode->nodeid=0;
        act_dir_inode->isDirectory=true;
        act_dir_inode->references=1;
        act_dir_inode->file_size=CLUSTER_SIZE;
        act_dir_inode->direct1=0;
        result = true;
    }

    int num_of_dirs_in_path = get_num_of_char_in_string(path, '/');
    if(num_of_dirs_in_path>0) {
        for (int i = 0; i < num_of_dirs_in_path; i++) {
            parse_first_dir_from_path(dirname(path), &path);
            set_inode_by_path(act_dir_inode, path, sb, fptr, act_path_inode);
        }
    }else{
        if(strcmp(path, ".") == 0){     //aktualni adresar
            *act_dir_inode = *act_path_inode;
            result = true;
        }else if(strcmp(path, "..") == 0){  //rodic
            struct directory_item tmp;
            struct pseudo_inode parrent;
            fseek(fptr, sb->data_start_address                                  //zacatek dat
                               + (act_path_inode->direct1 * CLUSTER_SIZE), SEEK_SET);  //zacatek dat slozky (nazacaku je dir item parent)
            fread(&tmp, sizeof(struct directory_item), 1, fptr);
            set_inode_by_nodeid(fptr, sb, tmp.inode, act_dir_inode);
            result = true;
        }else{  //slozka v act adresari
            result = set_dir_by_name(fptr,sb, act_dir_inode, path);
        }
    }

    return result;
}


#endif