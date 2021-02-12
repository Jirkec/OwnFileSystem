#ifndef MAIN_C_COMMANDS_H
#define MAIN_C_COMMANDS_H

#include "ext.h"

/**
 * naformatuje svazek
 * @param disk_size pozadovana valikost svazku
 * @param name nazev svazku
 * @return 1 -> OK -1-> chyba
 */
int format(int disk_size, char *name);

/**
 * nahraje soubor z disku do svazku
 * @param name jmeno svazku
 * @param act_path_inode aktualni cesta
 * @param s1 cesta k souboru na disku
 * @param s2 cesta ve svazku
 * @param printMsg bool jesli se ma vypsat zprava success
 * @param slink bool jeslti se jedna o slink
 * @return 1 -> OK -1-> chyba
 */
int incp(char *name, pseudo_inode *act_path_inode, char *s1, char *s2, bool printMsg, bool slink);

/**
 * vypise data souboru ve svazku
 * @param name jmeno svazku
 * @param act_path_inode aktualni cesta
 * @param s1 cesta k souboru ve svazku
 * @return 1 -> OK -1-> chyba
 */
int cat(char *name, pseudo_inode *act_path_inode, char *s1);

/**
 * vypise informace o souboru/adresari
 * @param name jmneo svazku
 * @param act_path_inode aktulani cesta
 * @param s1 cesta k souboru
 * @return 1 -> OK -1-> chyba
 */
int info(char *name, pseudo_inode *act_path_inode, char *s1);

/**
 * vyexportuje soubor
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param s1 cesta k souboru ve svazku
 * @param s2 cesta kam se ma vyexportovat
 * @param printMsg vypis success
 * @return 1 -> OK -1-> chyba
 */
int outcp(char *name, pseudo_inode *act_path_inode, char *s1, char *s2, bool printMsg);

/**
 * odstrani soubor
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param s1 cesta k souboru
 * @param printMsg vypis success
 * @return 1 -> OK -1-> chyba
 */
int rm(char *name, pseudo_inode *act_path_inode, char *s1, bool printMsg);

/**
 * zkopiruje soubor
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param s1 cesta kopirovaneho souboru
 * @param s2 cesta kam se ma kopirovat
 * @param printMsg vypis success
 * @return 1 -> OK -1-> chyba
 */
int cp(char *name, pseudo_inode *act_path_inode, char *s1, char *s2, bool printMsg);

/**
 * presune soubor
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param s1 cesta co se ma presunou
 * @param s2 cesta kam se ma presunout
 * @return 1 -> OK -1-> chyba
 */
int mv(char *name, pseudo_inode *act_path_inode, char *s1, char *s2);

/**
 * vytvoří adresr
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param s1 jmeno slozky
 * @return 1 -> OK -1-> chyba
 */
int mkdir(char *name, pseudo_inode *act_path_inode, char *s1);

/**
 * vymaze adresat
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param s1 jmeno slozky
 * @return 1 -> OK -1-> chyba
 */
int rmdir(char *name, pseudo_inode *act_path_inode, char *s1);

/**
 * zmeni aktualni cestu
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param act_path cesta text
 * @param s1 nova cesta
 * @return 1 -> OK -1-> chyba
 */
int cd(char *name, pseudo_inode *act_path_inode, char *act_path, char *s1);

/**
 * vypise obsah adresare
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param s1
 * @return 1 -> OK -1-> chyba
 */
int ls(char *name, pseudo_inode *act_path_inode, char *s1);

/**
 * vytvori slink
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param s1 cesta slinku
 * @param s2 cesta souboru
 * @return 1 -> OK -1-> chyba
 */
int slink(char *name, pseudo_inode *act_path_inode, char *s1, char *s2);

/**
 * nacte soubor s prikazy a provede je
 * @param name jmeno svazku
 * @param s1
 * @param act_path_inode aktualni cesta
 * @param act_path text cesta
 * @return 1 -> OK -1-> chyba
 */
int load(char *name, char *s1, pseudo_inode *act_path_inode, char *act_path);

/**
 * vybere spravny prikaz na zaklhade commandu v line
 * @param line radka prikazu
 * @param name jmeno svazku
 * @param act_path_inode cesta
 * @param act_path text cesta
 * @return 1 -> OK -1-> chyba
 */
int resolve_command(char *line, char *name, pseudo_inode *act_path_inode, char *act_path);

#endif //MAIN_C_COMMANDS_H
