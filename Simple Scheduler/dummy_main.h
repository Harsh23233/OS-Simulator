#ifndef DUMMY_MAIN_H
#define DUMMY_MAIN_H

int dummy_main(int argc, char **argv);  
int main(int argc, char **argv) {
    int ret = dummy_main(argc, argv);  
    return ret;
}
#define main dummy_main 
#endif
