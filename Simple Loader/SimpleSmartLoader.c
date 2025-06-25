#include "loader.h"

typedef Elf32_Ehdr elfHeader;
typedef Elf32_Phdr progHeader;

#define SizeofPage 4096
static int faults = 0;
static int allocations = 0;
static size_t intFragmentation = 0;

// Global variables for ELF information
static elfHeader *elfhdr = NULL;
static progHeader *phdr = NULL;
static int fd = -1;
static void *EntryPoint = NULL;

//free memory and close file
void loader_cleanup() {
    if (elfhdr) {
        free(elfhdr);
        elfhdr = NULL;
    }
    if (phdr) {
        free(phdr);
        phdr = NULL;
    }
    if (fd > 0) {
        close(fd);
        fd = -1;
    }
}

//open elf in read-only mode
int open_elf(const char *file_path) {
    fd = open(file_path, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open ELF file");
        return -1;
    }
    return fd;
}

//load and read elfhdr
int load_and_read_elfhdr() {
    elfhdr = (elfHeader *)malloc(sizeof(elfHeader));
    if (!elfhdr) {
        perror("Memory allocation failed for ELF header");
        return -1;
    }
    
    if (read(fd, elfhdr, sizeof(elfHeader)) != sizeof(elfHeader)) {
        perror("Failed to read ELF header");
        return -1;
    }
    return 0;
}

int load_and_read_phdr() {
    phdr = (progHeader *)malloc(elfhdr->e_phnum * elfhdr->e_phentsize);
    if (!phdr) {
        perror("Memory allocation failed for program headers");
        return -1;
    }
    
    lseek(fd, elfhdr->e_phoff, SEEK_SET);
   
    
    if (read(fd, phdr, elfhdr->e_phnum * elfhdr->e_phentsize) != elfhdr->e_phnum * elfhdr->e_phentsize) {
        perror("Failed to read program headers");
        return -1;
    }
    return 0;
}

void* getEntryPoint() {
    EntryPoint = (void *)elfhdr->e_entry;
    return EntryPoint;
}

//segment corresponding to faulting address
progHeader* find_segment(void *addr){
    Elf32_Addr ad = (Elf32_Addr)addr;
    for (int i = 0; i < elfhdr->e_phnum; i++) {
        if (phdr[i].p_type == 1) {
            Elf32_Addr s = phdr[i].p_vaddr;
            if (ad >= s && ad < s + phdr[i].p_memsz){
                return &phdr[i];
            }
        }
    }
    return NULL;
}

size_t getPages(progHeader *segment) {
    size_t s_page = segment->p_vaddr & ~(SizeofPage - 1);
    size_t e_page = (segment->p_vaddr + segment->p_memsz + SizeofPage - 1) & ~(SizeofPage - 1);
    return (e_page - s_page) / SizeofPage;
}

size_t getFrag(progHeader *segment, void *addr) {
    size_t p_start = (size_t)addr;
    size_t p_end = p_start + SizeofPage;
    size_t s_start = segment->p_vaddr;
    size_t s_end = s_start + segment->p_memsz;
    
    // If this is the last page of the segment
    if (p_end >= s_end) {
        size_t used = (s_end - p_start);
        return (used < SizeofPage) ? (SizeofPage - used) : 0;
    }
    
    // If this is the first page of the segment
    if (p_start <= s_start) {
        size_t unused = s_start - p_start;
        return unused;
    }
    
    return 0;
}

void SIGSEGV_handler( int sig, siginfo_t *info, void *context ) {
    void *fault_addr = info->si_addr;
    faults++;
    
    void *AdjustedAddress = (void *)((uintptr_t)fault_addr & ~(SizeofPage - 1));
    
    progHeader *segment = find_segment(fault_addr);
    if (!segment) {
        printf("No segment found for the given fault address.");
        exit(EXIT_FAILURE);
    }
    size_t seg_offset = (uintptr_t)AdjustedAddress - segment->p_vaddr;
    
    void *Mapping = mmap(AdjustedAddress, SizeofPage, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    
    if (Mapping == MAP_FAILED) {
        perror("Failed to map memory for segment");
        exit(EXIT_FAILURE);
    }
    
    allocations++;

    off_t fOffset = segment->p_offset + seg_offset;
    size_t remBytes = (segment->p_memsz > seg_offset) ? (segment->p_memsz - seg_offset) : 0;
    size_t rSize = (remBytes < SizeofPage) ? remBytes : SizeofPage;
    
    printf("Remaining bytes = %zd\n", remBytes);
    printf("Read Size = %zd\n", rSize);
    
    memset(Mapping,0,SizeofPage);
    if (rSize > 0) {
        if (lseek(fd, fOffset, SEEK_SET) == -1) {
            perror("Failed to locate in file");
            munmap(Mapping, SizeofPage);
            exit(EXIT_FAILURE);
        }

        size_t bytes_read = 0;
        while (bytes_read < rSize) {
            ssize_t ret = read(fd, Mapping + bytes_read, rSize - bytes_read);
            if (ret <= 0) {
                if (ret == 0) break; 
                perror("Read failed");
                munmap(Mapping, SizeofPage);
                exit(EXIT_FAILURE);
            }
            bytes_read += ret;
        }
    }
    size_t page_fragmentation = getFrag(segment, AdjustedAddress);
    intFragmentation += page_fragmentation;
    printf("Page fragmentation = %zd\n", page_fragmentation);

    if (rSize < SizeofPage) {
        memset(Mapping + rSize, 0, SizeofPage - rSize);
    }
    int prot = 0;
    if (segment->p_flags & PF_R) prot |= PROT_READ;
    if (segment->p_flags & PF_W) prot |= PROT_WRITE;
    if (segment->p_flags & PF_X) prot |= PROT_EXEC;

    if (mprotect(Mapping, SizeofPage, prot) == -1) {
        perror("Failed to set segment permissions");
        munmap(Mapping, SizeofPage);
        exit(EXIT_FAILURE);
    }
}

void setup_sigsegv_handler() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = SIGSEGV_handler;
    sigemptyset(&sa.sa_mask);
    
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("Failed to set up SIGSEGV handler");
        exit(EXIT_FAILURE);
    }
}

void load_and_run_elf(const char *filename) {
    setup_sigsegv_handler();
    
    if (open_elf(filename) < 0 || load_and_read_elfhdr() < 0 || load_and_read_phdr() < 0) {
        loader_cleanup();
        exit(EXIT_FAILURE);
    }
    
    if (memcmp(elfhdr->e_ident, ELFMAG, SELFMAG) != 0) {
        printf("Invalid ELF file\n");
        loader_cleanup();
        exit(EXIT_FAILURE);
    }
    
    void (*start_func)(void) = getEntryPoint();
    if (!start_func) {
        fprintf(stderr, "Failed to find entry point\n");
        loader_cleanup();
        exit(EXIT_FAILURE);
    }
    
    int return_value = ((int (*)(void))start_func)();
    
    printf("Execution Statistics:\n");
    printf("Page faults: %d\n", faults);
    printf("Page allocations: %d\n", allocations);
    printf("Internal Fragmentation: %.2f KB\n", intFragmentation / 1024.0);
    printf("Program's Output: %d\n", return_value);
    
    loader_cleanup();
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("Error. Exiting file.");
        return EXIT_FAILURE;
    }
    
    load_and_run_elf(argv[1]);
    return 0;
}
