#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<getopt.h>

int main(int argc, char *argv[]) {
    char *file_name = "traces/csim/long.trace";
    FILE *stream = fopen(file_name,"r");
    char buffer[50];
    int i = 0;
    while (fgets(buffer, 50, stream))
    {
        printf("%s", buffer);
        i++;
    }
    printf("%d lines in total\n", i);
    fclose(stream);
}