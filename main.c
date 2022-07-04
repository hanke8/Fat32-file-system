#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <time.h>
#include "fat_filesystem.h"

typedef enum {
    MKDIR = 1,
    RMDIR = 2,
    OPEN_FILE = 3,
    WRITE_FILE = 4,
    READ_FILE = 5,
    DELETE_FILE = 6,
    RENAME = 7,
    LS = 8,
    CD = 9,
    CLEAR = 10,
    HELP = 11,
    EXIT = 12,
    EXIT_AND_DELETEDISK = 13,
    UNDEFINED = 14
} CMD;

int run();
void usageInfo();
CMD getCmdType(char* cmd);

void main() {
    printf("Welcome to use hanke's fat32 file system ~\n");
    newDisk();
    int isDeleteDisk = run();
    if(isDeleteDisk == 1) {
        deleteDisk();
    }
    return;
}

int run() {
    int isDeleteDisk = 0, isExit = 0;
    char cmd[10], arg1[10], arg2[10], buf[2097152];
    while(isExit == 0) {
        memset(buf, 0, sizeof(buf));
        memset(cmd, 0, sizeof(cmd));
        memset(arg1, 0, sizeof(arg1));
        memset(arg2, 0, sizeof(arg2));
        printf("> ");
        scanf("%s", cmd);
        switch (getCmdType(cmd)) {
        case MKDIR:
            scanf("%s", arg1);
            myCreate(arg1, 1);
            break;
        case RMDIR:
            scanf("%s", arg1);
            myDelete(arg1);
            break;
        case OPEN_FILE:
            scanf("%s", arg1);
            myCreate(arg1, 0);
            break;
        case WRITE_FILE:
            scanf("%s", arg1);
            getchar();
            myCreate(arg1, 0);
            FCB* fcb_write = getFcbByName(arg1);
            int is_writable = writeLock(fcb_write);
            if(is_writable == -1) {
                printf("Failed: someone is reading or writing this file.\n");
                break;
            }
            fgets(buf, sizeof(buf), stdin);
            buf[strlen(buf) - 1] = '\0';
            myWrite(arg1, buf);
            writeUnlock(fcb_write);
            break;
        case READ_FILE:
            scanf("%s", arg1);
            FCB* fcb_read = getFcbByName(arg1);
            int is_readable = readLock(fcb_read);
            if(is_readable == -1) {
                printf("Failed: someone is writing this file.\n");
                break;
            }
            myRead(arg1, buf);
            readUnlock(fcb_read);
            printf("%s\n", buf);
            break;
        case DELETE_FILE:
            scanf("%s", arg1);
            FCB* fcb_delete = getFcbByName(arg1);
            int is_deletable = writeLock(fcb_delete);
            if(is_deletable == -1) {
                printf("Failed: someone is reading or writing this file\n");
                break;
            }
            myDelete(arg1);
            writeUnlock(fcb_delete);

            if(fcb_delete != NULL && fcb_delete->is_dir != 1) {
                char sem_read_name[50];
                char sem_write_name[50];
                sprintf(sem_read_name, "%s-%d-read", fcb_delete->name, Cur_Block_Idx);
                sprintf(sem_write_name, "%s-%d-write", fcb_delete->name, Cur_Block_Idx);
                sem_unlink(sem_read_name);
                sem_unlink(sem_write_name);
            }
            break;
        case RENAME:
            scanf("%s", arg1);
            scanf("%s", arg2);
            myRename(arg1, arg2);
            break;
        case LS:
            myLs();
            break;
        case CD:
            scanf("%s", arg1);
            myCd(arg1);
            break;
        case CLEAR:
            system("clear");
            break;
        case HELP:
            usageInfo();
            break;
        case EXIT:
            isExit = 1;
            break;
        case EXIT_AND_DELETEDISK:
            isExit = 1;
            isDeleteDisk = 1;
            break;
        default:
            printf("command '%s' not found\n", cmd);
            break;
        }
        fflush(stdin);
    }
    return isDeleteDisk;
}

CMD getCmdType(char* cmd) {
    if(strcmp(cmd, "mkdir") == 0) return MKDIR;
    if(strcmp(cmd, "rmdir") == 0) return RMDIR;
    if(strcmp(cmd, "touch") == 0) return OPEN_FILE;
    if(strcmp(cmd, "vim") == 0) return WRITE_FILE;
    if(strcmp(cmd, "cat") == 0) return READ_FILE;
    if(strcmp(cmd, "rm") == 0) return DELETE_FILE;
    if(strcmp(cmd, "mv") == 0) return RENAME;
    if(strcmp(cmd, "ls") == 0) return LS;
    if(strcmp(cmd, "cd") == 0) return CD;
    if(strcmp(cmd, "clear") == 0) return CLEAR;
    if(strcmp(cmd, "help") == 0) return HELP;
    if(strcmp(cmd, "exit") == 0) return EXIT;
    if(strcmp(cmd, "dismiss") == 0) return EXIT_AND_DELETEDISK;
    return UNDEFINED;
}

void usageInfo() {
    printf("Usage: command [argument]...\n");
    printf("  directory-commands: mkdir, rmdir\n");
    printf("  file-commands: touch, vim, rm, cat\n");
    printf("  common-commands: mv, ls, cd, clear, exit, dismiss\n");
}
