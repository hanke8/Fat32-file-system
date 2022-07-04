#include <fcntl.h>
#include <semaphore.h>

#define USED -1
#define FREE 0

const unsigned int DISK_SIZE  = 104857600;
const unsigned int BLOCK_SIZE = 4096;
const unsigned int BLOCK_NUM  = 25600;          // DISK_SIZE / BLOCK_SIZE
const unsigned int DATA_NUM   = 20970;          // 4096 + 4x + 4096x <= 104857600  -->  x = 20970
const unsigned int FILE_MAX_SIZE = 2097152;     // 2M
const unsigned int FILE_NAME_MAX_LENGTH = 10;

int Shmid;
void* Disk_Addr;
int* Fat;
void* Data_region_addr;
int Cur_Block_Idx;

typedef struct {
    char buf[4096];
} Block;

typedef struct {
    char is_existed;
    char is_dir;
    char name[11];
    int first_block;
    int size;
    int readcnt;
} FCB;

// data structure
void newDisk();
void deleteDisk();
int* getFat();
void* getDataRegionAddr();
FCB* getFirstFcb();
int getFCBSize();
FCB* getFcbByName(char* name);
int getFreeBlock();
Block* getBlockById(int id);

// read-write lock
int readLock(FCB* fcb);
void readUnlock(FCB* fcb);
int writeLock(FCB* fcb);
void writeUnlock(FCB* fcb);
void unlinkAllSem();

// command
void myCreate(char* name, char type);
void myRename(char* name, char* newname);
void myWrite(char* name, char* buf);
void myRead(char* name, char* buf);
void myDelete(char* name);
void myLs();
void myCd(char* dirname);

int* getFat() {
    return (int*)(Disk_Addr + BLOCK_SIZE);
}

void* getDataRegionAddr() {
    return Disk_Addr + BLOCK_SIZE + sizeof(int) * DATA_NUM;
}

FCB* getFirstFcb() {
    return (FCB*)(getDataRegionAddr() + Cur_Block_Idx * BLOCK_SIZE);
}

int getFCBSize() {
    return BLOCK_SIZE / sizeof(FCB);
}

FCB* getFcbByName(char* name) {
    FCB* fcb = getFirstFcb();
    int num = getFCBSize(), idx = -1;
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed == 1 && fcb[i].is_dir != 1 && strcmp(name, fcb[i].name) == 0) {
            idx = i;
            break;
        }
    }
    if(idx == -1) return NULL;
    return &fcb[idx];
}

int getFreeBlock() {
    int res = -1;
    for(int i = 0; i < DATA_NUM; i++) {
        if(Fat[i] == FREE) {
            res = i;
            break;
        }
    }
    return res;
}

Block* getBlockById(int id) {
    return (Block*)(getDataRegionAddr() + id * BLOCK_SIZE);
}

int readLock(FCB* fcb) {
    if(fcb == NULL) return 0;
    // get read and write sem
    char read_sem_name[50];
    char write_sem_name[50];
    sprintf(read_sem_name, "%s-%d-read", fcb->name, Cur_Block_Idx);
    sprintf(write_sem_name, "%s-%d-write", fcb->name, Cur_Block_Idx);
    sem_t* read_sem = sem_open(read_sem_name, O_CREAT, 0664, 1);
    sem_t* write_sem = sem_open(write_sem_name, O_CREAT, 0664, 1);

    sem_wait(read_sem);                      // read lock
    fcb->readcnt += 1;
    if(fcb->readcnt == 1) {
        int val = 0;
        sem_getvalue(write_sem, &val);
        if(val == 0) {
            sem_post(read_sem);             // read unlock
            return -1;
        }
        sem_wait(write_sem);                // write lock
    }
    sem_post(read_sem);                     // read unlock
    return 0;
}

void readUnlock(FCB* fcb) {
    if(fcb == NULL) return;
    // get read and write sem
    char read_sem_name[50];
    char write_sem_name[50];
    sprintf(read_sem_name, "%s-%d-read", fcb->name, Cur_Block_Idx);
    sprintf(write_sem_name, "%s-%d-write", fcb->name, Cur_Block_Idx);
    sem_t* read_sem = sem_open(read_sem_name, O_CREAT, 0664, 1);
    sem_t* write_sem = sem_open(write_sem_name, O_CREAT, 0664, 1);

    sem_wait(read_sem);               // read lock
    fcb->readcnt -= 1;
    if(fcb->readcnt == 0) {
        sem_post(write_sem);         // write unlock
    }
    sem_post(read_sem);              // read unlock
}

int writeLock(FCB* fcb) {
    if(fcb == NULL) return 0;
    // get write sem
    char write_sem_name[50];
    sprintf(write_sem_name, "%s-%d-write", fcb->name, Cur_Block_Idx);
    sem_t* write_sem = sem_open(write_sem_name, O_CREAT, 0664, 1);
    int val = 0;
    sem_getvalue(write_sem, &val);
    if(val == 0) return -1; 

    sem_wait(write_sem);         // write lock
    return 0;
}

void writeUnlock(FCB* fcb) {
    if(fcb == NULL) return;
    // get write sem
    char write_sem_name[50];
    sprintf(write_sem_name, "%s-%d-write", fcb->name, Cur_Block_Idx);
    sem_t* write_sem = sem_open(write_sem_name, O_CREAT, 0664, 1);

    sem_post(write_sem);     // write unlock
}

void newDisk() {
    // alloc memeroy
    Shmid = shmget((key_t)1234, DISK_SIZE, 0666 | IPC_CREAT);
    if (Shmid == -1) {
        fprintf(stderr, "shmget failed.\n");
        exit(EXIT_FAILURE);
    }
    Disk_Addr = shmat(Shmid, (void*)0, 0);
    if (Disk_Addr == (void*)-1) {
        fprintf(stderr, "shmat failed.\n");
        exit(EXIT_FAILURE);
    }

    // init
    Fat = getFat();
    Data_region_addr = getDataRegionAddr();
    Fat[0] = Fat[1] = Fat[2] = USED;
    Cur_Block_Idx = 2;
}

void unlinkAllSem() {
    FCB* fcb = getFirstFcb();
    int num = getFCBSize();
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed == 1) {
            if(strcmp(fcb[i].name, ".") == 0 || strcmp(fcb[i].name, "..") == 0) continue;
            if(fcb[i].is_dir == 1) {
                myCd(fcb[i].name);
                unlinkAllSem();
                myCd("..");
            }
            else {
                char sem_read_name[50];
                char sem_write_name[50];
                sprintf(sem_read_name, "%s-%d-read", fcb[i].name, Cur_Block_Idx);
                sprintf(sem_write_name, "%s-%d-write", fcb[i].name, Cur_Block_Idx);
                sem_unlink(sem_read_name);
                sem_unlink(sem_write_name);
            }
        }
    }
}

void deleteDisk() {
    unlinkAllSem();
    if (shmdt(Disk_Addr) == -1) {
        fprintf(stderr, "shmdt failed.\n");
        exit(EXIT_FAILURE);
    }
    if (shmctl(Shmid, IPC_RMID, 0) == -1) {
        fprintf(stderr, "shmctl failed.\n");
        exit(EXIT_FAILURE);
    }
}

// create file when type = 0, create dir when type = 1
void myCreate(char* name, char type) {
    // check if it already exists
    FCB* fcb = getFirstFcb();
    int num = getFCBSize();
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed == 1 && strcmp(name, fcb[i].name) == 0) {
            if(type == 1) printf("%s already exists.\n", name);
            return;
        }
    }

    // get free block
    int free_block = -1;
    if(type == 1) {
        free_block = getFreeBlock();
        if(free_block == -1) {
            printf("No free space.\n");
            return;
        }
    }
    
    // find free fcb to write
    int isok = 0;
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed != 1) {
            isok = 1;
            fcb[i].is_existed = 1;
            fcb[i].is_dir = type;
            strcpy(fcb[i].name, name);
            fcb[i].first_block = free_block;
            if(type == 1) {
                fcb[i].size = 2 * sizeof(FCB);
            }
            if(type == 0) {
                fcb[i].size = 0;
                fcb[i].readcnt = 0;
            }
            break;
        }
    }
    if(isok == 0) {
        printf("There is no space left in the current directory.\n");
        return;
    }

    // if create type is dir
    if(type == 1) {
        // write fat
        Fat[free_block] = USED;
        // write FCB "." and ".."
        FCB* cur_dir_fcb = (FCB*)(Data_region_addr + free_block * BLOCK_SIZE);
        FCB* parent_dir_fcb = cur_dir_fcb + 1;
        cur_dir_fcb->is_existed = 1;
        cur_dir_fcb->is_dir = 1;
        strcpy(cur_dir_fcb->name, ".");
        cur_dir_fcb->first_block = free_block;
        parent_dir_fcb->is_existed = 1;
        parent_dir_fcb->is_dir = 1;
        strcpy(parent_dir_fcb->name, "..");
        parent_dir_fcb->first_block = Cur_Block_Idx;
    }

    // update cur dir size
    if(strcmp(fcb[0].name, ".") == 0 && strcmp(fcb[1].name, "..") == 0) {
        int cur_block = fcb[0].first_block;
        int parent_block = fcb[1].first_block;
        FCB* pfcb = (FCB*)(Data_region_addr + parent_block * BLOCK_SIZE);
        for(int i = 0; i < num; i++) {
            if(pfcb[i].is_existed == 1 && pfcb[i].is_dir == 1 && pfcb[i].first_block == cur_block) {
                pfcb[i].size += sizeof(FCB);
                break;
            }
        }
    }
}

void myRename(char* name, char* newname) {
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0
    || strcmp(newname, ".") == 0 || strcmp(newname, "..") == 0) {
        printf("cannot rename '.' or '..'\n");
        return;
    }
    FCB* fcb = getFirstFcb();
    int num = getFCBSize(), idx = -1;
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed == 1 && strcmp(name, fcb[i].name) == 0) {
            idx = i;
            break;
        }
    }
    if(idx == -1) {
        printf("No such file or directory.\n");
        return;
    }
    strcpy(fcb[idx].name, newname);
}

void myWrite(char* name, char* buf) {
    // find it's fcb
    FCB* fcb = getFirstFcb();
    int num = getFCBSize(), idx = -1;
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed == 1 && fcb[i].is_dir != 1 && strcmp(name, fcb[i].name) == 0) {
            idx = i;
            break;
        }
    }
    if(idx == -1) {
        printf("No such file.\n");
        return;
    }

    // get free blocks to write
    size_t size = strlen(buf);
    if(size == 0) return;
    int pre_block_cnt = fcb[idx].size / BLOCK_SIZE;
    if(fcb[idx].size % BLOCK_SIZE > 0) pre_block_cnt += 1;
    int free_block_cnt = size / BLOCK_SIZE;
    int leftover = size % BLOCK_SIZE;
    if(leftover > 0) free_block_cnt += 1;
    int free_blocks[free_block_cnt];

    for(int i = pre_block_cnt; i < free_block_cnt; i++) {
        int tmp = getFreeBlock();
        if(tmp == -1) {
            for(int j = pre_block_cnt; j < i; j++) {
                Fat[free_blocks[j]] = FREE;
            }
            printf("No enough free space.\n");
            return;
        }
        Fat[tmp] = USED;
        free_blocks[i] = tmp;
    }
    for(int i = 0, cur = fcb[idx].first_block; i < pre_block_cnt; i++) {
        free_blocks[i] = cur;
        cur = Fat[cur];
    }

    // write fcb
    fcb[idx].first_block = free_blocks[0];
    fcb[idx].size = size;

    // write data and update fat
    for(int i = 0; i < free_block_cnt; i++) {
        int cur = free_blocks[i], next = USED;
        if(i + 1 < free_block_cnt) next = free_blocks[i + 1];
        Fat[cur] = next;

        Block* write_block = getBlockById(cur);
        strncpy(write_block->buf, buf + i * BLOCK_SIZE, BLOCK_SIZE);
    }
}

void myRead(char* name, char* buf) {
    // find it's fcb
    FCB* fcb = getFirstFcb();
    int num = getFCBSize(), idx = -1;
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed == 1 && fcb[i].is_dir != 1 && strcmp(name, fcb[i].name) == 0) {
            idx = i;
            break;
        }
    }
    if(idx == -1) {
        printf("No such file.\n");
        return;
    }

    // read by using fat
    if(fcb[idx].size == 0) return;
    int read_cnt = fcb[idx].size / BLOCK_SIZE;
    int leftover = fcb[idx].size % BLOCK_SIZE;
    if(leftover > 0) read_cnt += 1;
    for(int i = 0, cur = fcb[idx].first_block; i < read_cnt; i++) {
        Block* read_block = getBlockById(cur);
        if(i == read_cnt - 1) {
            strncpy(buf + i * BLOCK_SIZE, read_block->buf, leftover);
        }
        else strcpy(buf + i * BLOCK_SIZE, read_block->buf);
        cur = Fat[cur];
    }
}

void myDelete(char* name) {
    if(strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        printf("cannot delete '.' or '..'\n");
        return;
    }
    // find it's fcb
    FCB* fcb = getFirstFcb();
    int num = getFCBSize(), idx = -1;
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed == 1 && strcmp(name, fcb[i].name) == 0) {
            if(fcb[i].is_dir == 1 && fcb[i].size > 2 * sizeof(FCB)) {
                printf("Directory not empty.\n");
                return;
            }
            idx = i;
            fcb[i].is_existed = 0;
            break;
        }
    }
    if(idx == -1) {
        printf("No such file.\n");
        return;
    }
    
    // delete fat
    if(fcb[idx].size == 0) return;
    int delete_cnt = fcb[idx].size / BLOCK_SIZE;
    if(fcb[idx].size % BLOCK_SIZE > 0) delete_cnt += 1;
    int pre = -1, cur = fcb[idx].first_block;
    for(int i = 0; i < delete_cnt; i++) {
        pre = cur;
        cur = Fat[pre];
        Fat[pre] = FREE;
    }

    // update cur dir size
    if(strcmp(fcb[0].name, ".") == 0 && strcmp(fcb[1].name, "..") == 0) {
        int cur_block = fcb[0].first_block;
        int parent_block = fcb[1].first_block;
        FCB* pfcb = (FCB*)(Data_region_addr + parent_block * BLOCK_SIZE);
        for(int i = 0; i < num; i++) {
            if(pfcb[i].is_existed == 1 && pfcb[i].is_dir == 1 && pfcb[i].first_block == cur_block) {
                pfcb[i].size -= sizeof(FCB);
                break;
            }
        }
    }
}

void myLs() {
    FCB* fcb = getFirstFcb();
    int num = getFCBSize();
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed == 1) {
            printf("%s  ", fcb[i].name);
        }
    }
    printf("\n");
}

void myCd(char* dirname) {
    FCB* fcb = getFirstFcb();
    int num = getFCBSize(), isok = 0;
    for(int i = 0; i < num; i++) {
        if(fcb[i].is_existed == 1 && fcb[i].is_dir == 1 && strcmp(dirname, fcb[i].name) == 0) {
            isok = 1;
            Cur_Block_Idx = fcb[i].first_block;
            break;
        }
    }
    if(isok == 0) {
        printf("No such directory.\n");
    }
}

