
#ifndef SEMESTRALKA_EXT_H
#define SEMESTRALKA_EXT_H

// priklad - verze 2020-01
// jedná se o SIMULACI souborového systému
// první i-uzel bude odkaz na hlavní adresář (1. datový blok)
// počet přímých i nepřímých odkazů je v reálném systému větší
// adresář vždy obsahuje dvojici číslo i-uzlu - název souboru
// jde jen o příklad, vlastní datové struktury si můžete upravit

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <libgen.h>

#define CLUSTER_SIZE 64             //bylo by dobre, aby byl dělitelný 16 - tj. sizeof(struct directory_item)
#define FILES_IN_FOLDER_COUNT 4     //CLUSTER_SIZE / sizeof(struct directory_item)
#define INODES_COUNT 10
#define BITMAPI_SIZE 10             // schopno zpravocat 80 souborů celkem
#define BITMAP_SIZE 10              // schopno zpravocat 80 clusterů celkem
#define DIRECT_LINK_COUNT 5
#define ID_ITEM_FREE -1
#define ID_CLUESTER_FREE -1

typedef unsigned char u_char;

typedef struct thesuperblock {
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
}superblock;

typedef struct thepseudo_inode {
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
}pseudo_inode;

typedef struct thedirectory_item {
    int32_t inode;                   // inode odpovídající souboru
    char item_name[12];              //8+3 + /0 C/C++ ukoncovaci string znak
}directory_item;

// funkce pro predvyplneni struktury sb s nastavenim velikosti disku
int fill_sb(superblock *sb, int disk_size);

void printSb(superblock *sb);

long int findSize(char *file_name);

int find_empty_data_node_in_bitmap(u_char data_bitmap);

int set_bit_on_position_in_bitmap(u_char data_bitmap, int position, int bit);

int get_needed_clusters(int file_size);

void printBits(size_t const size, void const * const ptr);

int get_num_of_char_in_string(char *string, char find);

void set_inode_by_nodeid(FILE *fptr,superblock *sb, int nodeid, pseudo_inode *inode);

/**
 * najde slozku v adresari act_dir_inode podle nazvu a nastavi ji parametru act_dir_inode
 * @param fptr
 * @param sb
 * @param act_dir_inode
 * @param name
 * @return
 */
bool set_dir_by_name(FILE *fptr,superblock *sb, pseudo_inode *act_dir_inode, pseudo_inode *dir_inode_to_set, char *name);

/**
 * najde soubor v adresari act_dir_inode podle nazvu a nastavi ho parametru act_dir_inode
 * @param fptr
 * @param sb
 * @param act_dir_inode
 * @param name
 * @return
 */
bool set_file_by_name(FILE *fptr,superblock *sb, pseudo_inode *act_dir_inode, pseudo_inode *dir_inode_to_set, char *name);

void parse_first_dir_from_path(char *path, char **target);

/**
 * nastavi inode atributu act_dir_inode na slozku podle zadane cesty path
 * @param act_dir_inode
 * @param path
 * @param sb
 * @param fptr
 * @param act_path_inode
 * @return
 */
bool set_inode_by_path(pseudo_inode *act_dir_inode, char *path, superblock *sb, FILE *fptr, pseudo_inode *act_path_inode);

void print_file_content(pseudo_inode *file, FILE *fptr, superblock *sb);

void print_file_info(pseudo_inode *file, pseudo_inode *file_parent, FILE *fptr, superblock *sb);

bool set_diritem_by_inode(FILE *fptr, superblock *sb, int file_nodeid, pseudo_inode *file_parent, directory_item *file_dir_item);

bool set_inode_by_name(FILE *fptr,superblock *sb, pseudo_inode *act_dir_inode, pseudo_inode *dir_inode_to_set, char *name);

bool export_file(pseudo_inode *file, char *target_name, FILE *fptr, superblock *sb);

bool delete_file(pseudo_inode *file, pseudo_inode *file_parent, FILE *fptr, superblock *sb);

#endif