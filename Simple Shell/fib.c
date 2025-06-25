#include<stdio.h>
#include<stdlib.h>

int fib(int n){
    if(n==0 || n==1){
        return n;
    }

    return(fib(n-1)+fib(n-2));
}

int main(int argc,char** argv){

    if(argc!=2){
        printf("Enter 2 arguments\n");
        exit(0);
    }

    long long ans = fib(atoi(argv[1]));
    printf("%lld\n",ans);

    return 0;

}