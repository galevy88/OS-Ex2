
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define PATH 200

void readPath(int fd, char dir[PATH], char input[PATH], char expected[PATH]);

int gccForUser(DIR *dir, char *path, int fds[2]);

void closeFds(int fds[2]);

void getUserPath(char dst[PATH], char path[PATH]);

void runUser(char input[PATH], int fds[2], DIR *dir);

int compare(int fds[2], char expected[PATH], DIR *dir);

void timeoutHandler(int signum);

int main(int argc, char *argv[]) {
    //check num of args
    if (argc < 2) {
        if (write(1, "Not valid argv\n", 15) == -1)
            return -1;
        return -1;
    }
    //read the configuration file
    int fd;
    if ((fd = open(argv[1], O_RDONLY)) == -1) {
        if (write(1, "Error in: open\n", 15) == -1)
            return -1;
        return -1;
    }
    char dirPath[PATH], inputPath[PATH], expectedPath[PATH];
    readPath(fd, dirPath, inputPath, expectedPath);
    close(fd);
    //get the relative paths
    char relativePath[PATH];
    strcpy(relativePath, argv[1]);
    getUserPath(relativePath, inputPath);
    strcpy(inputPath, relativePath);
    strcpy(relativePath, argv[1]);
    getUserPath(relativePath, expectedPath);
    strcpy(expectedPath, relativePath);
    char usersDir[PATH];
    strcpy(usersDir, argv[1]);
    getUserPath(usersDir, dirPath);
    // check that the users dir exist
    struct stat statUser;
    if (stat(usersDir, &statUser) == -1) {
        if (write(1, "Error in: stat\n", 15) == -1)
            return -1;
        return -1;
    }
    if (!S_ISDIR(statUser.st_mode)) {
        if (write(1, "Not a valid directory\n", 22) == -1)
            return -1;
        return -1;
    }
    //check that expected output and inout files exist
    if (access(inputPath, F_OK) == -1) {
        if (write(1, "Input file not exist\n", 21) == -1)
            return -1;
        return -1;
    }
    if (access(expectedPath, F_OK) == -1) {
        if (write(1, "Output file not exist\n", 22) == -1)
            return -1;
        return -1;
    }
    //open the users dir
    //[0] is error fd,[1] is result fd
    int fds[2];
    if ((fds[0] = open("errors.txt", O_CREAT | O_WRONLY, 0644)) == -1) {
        if (write(1, "Error in: open\n", 15) == -1)
            return -1;
        return -1;
    }
    if ((fds[1] = open("results.csv", O_CREAT | O_WRONLY, 0644)) == -1) {
        close(fds[0]);
        if (write(1, "Error in: open\n", 15) == -1)
            return -1;
        return -1;
    }
    DIR *dir;
    struct dirent *userDirent;
    if ((dir = opendir(usersDir)) == NULL) {
        closeFds(fds);
        if (write(1, "Error in: opendir\n", 18) == -1)
            return -1;
        return -1;
    }
    //read the users dir
    while ((userDirent = readdir(dir)) != NULL) {
        //pass the current dir cnd parent dir
        if (!strcmp(userDirent->d_name, ".") || !strcmp(userDirent->d_name, ".."))
            continue;
        //check that userDirent is dir
        char innerDir[PATH];
        strcpy(innerDir, usersDir);
        strcat(innerDir, "/");
        strcat(innerDir, userDirent->d_name);
        if (stat(innerDir, &statUser) == -1) {
            closeFds(fds);
            closedir(dir);
            if (write(1, "Error in: stat\n", 15) == -1)
                return -1;
        }
        if (!S_ISDIR(statUser.st_mode))
            continue;
        //open the users dir
        DIR *dir1;
        if ((dir1 = opendir(innerDir)) == NULL) {
            closeFds(fds);
            closedir(dir);
            if (write(1, "Error in: opendir\n", 18) == -1)
                return -1;
        }
        struct dirent *dirent;
        //search c file
        while ((dirent = readdir(dir1)) != NULL) {
            if (strlen(dirent->d_name) == 1)
                continue;
            if (!strcmp(".c", &dirent->d_name[strlen(dirent->d_name) - 2])) {
                //check that not directory
                char path[PATH];
                strcpy(path, innerDir);
                strcat(path, "/");
                strcat(path, dirent->d_name);
                if (stat(path, &statUser) == -1) {
                    closeFds(fds);
                    closedir(dir);
                    closedir(dir1);
                    if (write(1, "Error in: stat\n", 15) == -1)
                        return -1;
                }
                if (!S_ISDIR(statUser.st_mode))
                    break;
            }
        }
        //not exist c file
        if (!dirent) {
            if (write(fds[1], userDirent->d_name, strlen(userDirent->d_name)) == -1) {
                closeFds(fds);
                closedir(dir1);
                closedir(dir);
                return -1;
            }
            if (write(fds[1], ",0,NO_C_FILE\n", 13) == -1) {
                closeFds(fds);
                closedir(dir1);
                closedir(dir);
                return -1;
            }
            continue;
        }
        strcat(innerDir, "/");
        strcat(innerDir, dirent->d_name);
        closedir(dir1);
        //gcc the c file
        int success = gccForUser(dir, innerDir, fds);
        //compilation error
        if (success == 1) {
            if (write(fds[1], userDirent->d_name, strlen(userDirent->d_name)) == -1) {
                closeFds(fds);
                closedir(dir);
                return -1;
            }
            if (write(fds[1], ",10,COMPILATION_ERROR\n", 22) == -1) {
                closeFds(fds);
                closedir(dir);
                return -1;
            }
            continue;
        }
        //run users file
        runUser(inputPath, fds, dir);
        //compare the users output and expected output
        success = compare(fds, expectedPath, dir);
        if (write(fds[1], userDirent->d_name, strlen(userDirent->d_name)) == -1) {
            closeFds(fds);
            closedir(dir);
            return -1;
        }
        printf("The number is: %d\n", success);
        //print the result of compare to the file
        switch (success) {
            case 1:
                if (write(fds[1], ",100,EXCELLENT\n", 15) == -1) {
                    closeFds(fds);
                    closedir(dir);
                    return -1;
                }
                break;
            case 2:
                if (write(fds[1], ",50,WRONG\n", 10) == -1) {
                    closeFds(fds);
                    closedir(dir);
                    return -1;
                }
                break;
            case 3:
                if (write(fds[1], ",75,SIMILAR\n", 12) == -1) {
                    closeFds(fds);
                    closedir(dir);
                    return -1;
                }
                break;
            default:
                closeFds(fds);
                closedir(dir);
        }
        //remove the user output file
        if (remove("output.txt") == -1) {
            closeFds(fds);
            closedir(dir);
            if ((write(2, "Error in: remove\n", 17)) == -1)
                return -1;
            return -1;
        }
    }
    //close fd
    closeFds(fds);
    closedir(dir);
    //remove the user file
    if (remove("user.out") == -1) {
        if ((write(2, "Error in: remove\n", 17)) == -1)
            return -1;
        return -1;
    }
    return 0;
}

//compare user output with expected output
int compare(int fds[2], char expected[PATH], DIR *dir) {
    pid_t pid = fork();
    //fork failed
    if (pid == -1) {
        closeFds(fds);
        closedir(dir);
        if (write(1, "Error in: fork\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
        //child process
    else if (pid == 0) {
        //call comp.out
        char *argv[] = {"./comp.out", "output.txt", expected, NULL};
        execvp(argv[0], argv);
        closeFds(fds);
        closedir(dir);
        if (write(1, "Error in: execvp\n", 17) == -1)
            exit(-1);
        exit(-1);
    }
        //parent process
    else {
        int status;
        //get child status
        if ((wait(&status)) == -1) {
            closeFds(fds);
            closedir(dir);
            if (write(1, "Error in: wait\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        return WEXITSTATUS(status);
    }
}

//run the user file
void runUser(char input[PATH], int fds[2], DIR *dir) {
    int in, output;
    //open the input and output files
    if ((in = open(input, O_RDONLY)) == -1) {
        closeFds(fds);
        closedir(dir);
        if (write(1, "Error in: open\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    if ((output = open("output.txt", O_CREAT | O_WRONLY, 0644)) == -1) {
        closeFds(fds);
        close(in);
        closedir(dir);
        if (write(1, "Error in: open\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    pid_t pid;
    //fork failed
    if ((pid = fork()) == -1) {
        closeFds(fds);
        close(in);
        close(output);
        closedir(dir);
        if (write(1, "Error in: fork\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
        //child process
    else if (pid == 0) {
        //change standard input
        if (dup2(in, 0) == -1) {
            closeFds(fds);
            close(in);
            close(output);
            closedir(dir);
            if (write(1, "Error in: dup2\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        //change standard output
        if (dup2(output, 1) == -1) {
            closeFds(fds);
            close(in);
            close(output);
            closedir(dir);
            if (write(1, "Error in: dup2\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        //change standard error
        if (dup2(fds[0], 2) == -1) {
            closeFds(fds);
            close(in);
            close(output);
            closedir(dir);
            if (write(1, "Error in: dup2\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        //call user file
        char *argv[] = {"./user.out", NULL};
        execvp(argv[0], argv);
        closeFds(fds);
        close(in);
        close(output);
        closedir(dir);
        if (write(1, "Error in: execvp\n", 17) == -1)
            exit(-1);
        exit(-1);
    }
        //parent process
    else {
        int status;
        if ((wait(&status)) == -1) {
            closeFds(fds);
            close(in);
            close(output);
            closedir(dir);
            if (write(1, "Error in: wait\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        close(in);
        close(output);
        if (WEXITSTATUS(status) == -1)
            exit(-1);
        return;
    }

}

//get the relative path to the path dir
void getUserPath(char dst[PATH], char path[PATH]) {
    //get the dst dir path
    int len = strlen(dst), lastPlace = len, i;
    for (i = 0; i < len; i++) {
        if (dst[i] == '/')
            lastPlace = i;
    }
    dst[lastPlace] = '\0';
    //add the relative path
    strcat(dst, "/");
    strcat(dst, path);
}

//close the error and result files
void closeFds(int fds[2]) {
    close(fds[0]);
    close(fds[1]);
}

//read the conf text
void readPath(int fd, char *dir, char *input, char *expected) {
    char *position = dir;
    //read the first file
    if (read(fd, position, 1) == -1) {
        close(fd);
        if (write(1, "Error in: read\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    while (*position != '\n') {
        position++;
        if (read(fd, position, 1) == -1) {
            close(fd);
            if (write(1, "Error in: read\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
    }
    *position = '\0';
    position = input;
    //read the second line
    if (read(fd, position, 1) == -1) {
        close(fd);
        if (write(1, "Error in: read\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    while (*position != '\n') {
        position++;
        if (read(fd, position, 1) == -1) {
            close(fd);
            if (write(1, "Error in: read\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
    }
    *position = '\0';
    position = expected;
    //read the last file
    if (read(fd, position, 1) == -1) {
        close(fd);
        if (write(1, "Error in: read\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    while (*position != '\n') {
        position++;
        if (read(fd, position, 1) == -1) {
            close(fd);
            if (write(1, "Error in: read\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
    }
    *position = '\0';
}

//get path to c file ant compile him with child process
int gccForUser(DIR *dir, char *path, int fds[2]) {
    pid_t pid;
    //fork failed
    if ((pid = fork()) == -1) {
        closedir(dir);
        closeFds(fds);
        if (write(1, "Error in: fork\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
        //child process
    else if (!pid) {
        //change standard error
        if (dup2(fds[0], 2) == -1) {
            closeFds(fds);
            closedir(dir);
            if (write(1, "Error in: dup2\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        //call gcc
        char *argv[] = {"gcc", path, "-o", "user.out", NULL};
        execvp(argv[0], argv);
        //execvp failed
        closeFds(fds);
        closedir(dir);
        if (write(1, "Error in: execvp\n", 17) == -1)
            exit(-1);
        exit(-1);
    }
        //parent process
    else {
        int status;
        //get child status
        if (wait(&status) == -1) {
            closeFds(fds);
            closedir(dir);
            if (write(1, "Error in: wait\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        return WEXITSTATUS(status);
    }
}
