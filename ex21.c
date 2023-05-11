#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_FILE_SIZE 1000000

char* read_file(char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }
    char* content = (char*) malloc(MAX_FILE_SIZE * sizeof(char));
    if (content == NULL) {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }
    size_t file_size = fread(content, sizeof(char), MAX_FILE_SIZE, fp);
    if (file_size == 0) {
        perror("Error reading file");
        exit(EXIT_FAILURE);
    }
    content[file_size] = '\0';
    fclose(fp);
    return content;
}

int compare_files(char* file1, char* file2) {
    char* content1 = read_file(file1);
    char* content2 = read_file(file2);
    int result = 0; 
    if (strcmp(content1, content2) == 0) {
        result = 1;
    } else {
        int i = 0;
        int j = 0;
        int len1 = strlen(content1);
        int len2 = strlen(content2);
        while (i < len1 && j < len2) {
            char c1 = content1[i];
            char c2 = content2[j];
            if (c1 == c2) {
                i++;
                j++;
            } else if (isspace(c1) || isspace(c2)) {
                while (isspace(content1[i])) {
                    i++;
                }
                while (isspace(content2[j])) {
                    j++;
                }
            } else if (toupper(c1) == toupper(c2)) {
                i++;
                j++;
            } else {
                result = 2;
                break;
            }
        }
        if (result == 0 && (i < len1 || j < len2)) {
            // Check if the remaining characters are only spaces, tabs, or line breaks
            int only_spaces1 = 1;
            int only_spaces2 = 1;
            for (int k = i; k < len1; k++) {
                if (!isspace(content1[k])) {
                    only_spaces1 = 0;
                    break;
                }
            }
            for (int k = j; k < len2; k++) {
                if (!isspace(content2[k])) {
                    only_spaces2 = 0;
                    break;
                }
            }
            if (only_spaces1 && only_spaces2) {
                result = 3;
            } else {
                result = 2;
            }
        } else if (result == 0) {
            result = 3;
        }
    }
    free(content1);
    free(content2);
    return result;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s file1 file2\n", argv[0]);
        return EXIT_FAILURE;
    }
    int result = compare_files(argv[1], argv[2]);
    return result;
}
