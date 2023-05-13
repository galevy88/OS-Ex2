// Gal Levy 208540872
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>

#define PATH 200
void read_the_path(int fd, char dir[PATH], char input[PATH], char expected[PATH]);
int compile_using_gcc(DIR *dir, char *path, int fds[2]);
#define TIMEOUT 5
int compare(int fds[2], char expected[PATH], DIR *dir);
void timeout_handler(int signum);
int timeout_occurred = 0;
void close_folders(int fds[2]);
int run_user_file(char input[PATH], int fds[2], DIR *dir);
void get_the_path_of_user(char dst[PATH], char path[PATH]);


int main(int argc, char *argv[]) {

    // In case there is lt args than expected
    //check num of args
    if (argc < 2) {
        if (write(1, "Not valid argv\n", 15) == -1)
            return -1;
        return -1;
    }


    //read configuration - conf.txt
    int fd;
    if ((fd = open(argv[1], O_RDONLY)) == -1) {
        if (write(1, "Error in: open\n", 15) == -1)
            return -1;
        return -1;
    }

    char directory_path[PATH], input_path[PATH], expected_path_for[PATH];

    read_the_path(fd, directory_path, input_path, expected_path_for);
    close(fd);

    //relative path - generate and get
    char relative_path[PATH];
    strcpy(relative_path, argv[1]);

    get_the_path_of_user(relative_path, input_path);
    strcpy(input_path, relative_path);
    strcpy(relative_path, argv[1]);

    get_the_path_of_user(relative_path, expected_path_for);
    strcpy(expected_path_for, relative_path);
    char usersDir[PATH];
    strcpy(usersDir, argv[1]);

    get_the_path_of_user(usersDir, directory_path);
    // Search fro usr dir - if exist
    struct stat stat_user_for;
    if (stat(usersDir, &stat_user_for) == -1) {
        if (write(1, "Error in: stat\n", 15) == -1)
            return -1;
        return -1;
    }


    if (!S_ISDIR(stat_user_for.st_mode)) {
        if (write(1, "Not a valid directory\n", 22) == -1)
            return -1;
        return -1;
    }
    if (access(input_path, F_OK) == -1) {
        if (write(1, "Input file not exist\n", 21) == -1)
            return -1;
        return -1;
    }
    if (access(expected_path_for, F_OK) == -1) {
        if (write(1, "Output file not exist\n", 22) == -1)
            return -1;
        return -1;
    }
    //open the dirs for the user
    int fldrs[2];
    if ((fldrs[0] = open("errors.txt", O_CREAT | O_WRONLY, 0644)) == -1) {
        if (write(1, "Error in: open\n", 15) == -1)
            return -1;
        return -1;
    }
    if ((fldrs[1] = open("results.csv", O_CREAT | O_WRONLY, 0644)) == -1) {
        close(fldrs[0]);
        if (write(1, "Error in: open\n", 15) == -1)
            return -1;
        return -1;
    }
    DIR *dir;
    struct dirent *dirnet_usr;
    if ((dir = opendir(usersDir)) == NULL) {

        close_folders(fldrs);
        if (write(1, "Error in: opendir\n", 18) == -1)
            return -1;
        return -1;
    }
    while ((dirnet_usr = readdir(dir)) != NULL) {
        if (!strcmp(dirnet_usr->d_name, ".") || !strcmp(dirnet_usr->d_name, ".."))
            continue;
        //check if it is indeed a dir
        char dir_inside[PATH];

        strcpy(dir_inside, usersDir);
        strcat(dir_inside, "/");

        strcat(dir_inside, dirnet_usr->d_name);
        if (stat(dir_inside, &stat_user_for) == -1) {
            close_folders(fldrs);
            closedir(dir);
            if (write(1, "Error in: stat\n", 15) == -1)
                return -1;
        }
        if (!S_ISDIR(stat_user_for.st_mode))
            continue;
        //open the users dir
        DIR *dir1;

        if ((dir1 = opendir(dir_inside)) == NULL) {
            close_folders(fldrs);
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
                strcpy(path, dir_inside);
                strcat(path, "/");
                strcat(path, dirent->d_name);
                if (stat(path, &stat_user_for) == -1) {
                    close_folders(fldrs);
                    closedir(dir);


                    closedir(dir1);
                    if (write(1, "Error in: stat\n", 15) == -1)
                        return -1;
                }
                if (!S_ISDIR(stat_user_for.st_mode))
                    break;
            }
        }
        //c not exist in the current direcoty at all
        if (!dirent) {
            if (write(fldrs[1], dirnet_usr->d_name, strlen(dirnet_usr->d_name)) == -1) {
                close_folders(fldrs);
                closedir(dir1);
                closedir(dir);
                return -1;
            }


            if (write(fldrs[1], ",0,NO_C_FILE\n", 13) == -1) {
                close_folders(fldrs);
                closedir(dir1);
                closedir(dir);
                return -1;
            }
            continue;
        }
        strcat(dir_inside, "/");
        strcat(dir_inside, dirent->d_name);
        closedir(dir1);
        //compile c file
        int res_success = compile_using_gcc(dir, dir_inside, fldrs);
        //a compilation error has been raised via gcc
        if (res_success == 1) {
            if (write(fldrs[1], dirnet_usr->d_name, strlen(dirnet_usr->d_name)) == -1) {
                close_folders(fldrs);
                closedir(dir);
                return -1;
            }

            
            if (write(fldrs[1], ",10,COMPILATION_ERROR\n", 22) == -1) {
                close_folders(fldrs);
                closedir(dir);
                return -1;
            }
            continue;
        }
        //run users file - get output of run_user_file in case of TIMEOUT
        res_success = run_user_file(input_path, fldrs, dir);
        if (res_success == -1) {
            if (write(fldrs[1], dirnet_usr->d_name, strlen(dirnet_usr->d_name)) == -1) {
                close_folders(fldrs);
                closedir(dir);
                return -1;
            }

            if (write(fldrs[1], ",20,TIMEOUT\n", 12) == -1) {
                close_folders(fldrs);
                closedir(dir);
                return -1;
            }
            continue;
        }
        //compare the users output to origin main
        res_success = compare(fldrs, expected_path_for, dir);
        if (write(fldrs[1], dirnet_usr->d_name, strlen(dirnet_usr->d_name)) == -1) {
            close_folders(fldrs);
            closedir(dir);
            return -1;
        }

        //switch case for output
        switch (res_success) {
            case 1:


                if (write(fldrs[1], ",100,EXCELLENT\n", 15) == -1) {
                    close_folders(fldrs);
                    closedir(dir);
                    return -1;
                }
                break;
            case 2:

        
                if (write(fldrs[1], ",50,WRONG\n", 10) == -1) {
                    close_folders(fldrs);
                    closedir(dir);
                    return -1;
                }
                break;
            case 3:


                if (write(fldrs[1], ",75,SIMILAR\n", 12) == -1) {
                    close_folders(fldrs);
                    closedir(dir);
                    return -1;
                }
                break;
            default:

                close_folders(fldrs);
                closedir(dir);
        }
        //remove usrr output file

        if (remove("output.txt") == -1) {
            close_folders(fldrs);
            closedir(dir);
            if ((write(2, "Error in: remove\n", 17)) == -1)
                return -1;
            return -1;
        }
    }
    close_folders(fldrs);
    closedir(dir);
    if (remove("user.out") == -1) {
        if ((write(2, "Error in: remove\n", 17)) == -1)
            return -1;
        return -1;
    }
    return 0;
}

// compile the c file using gcc
int compile_using_gcc(DIR *dir, char *path, int fds[2]) {
    pid_t pid;
    if ((pid = fork()) == -1) {
        closedir(dir);
        close_folders(fds);

        if (write(1, "Error in: fork\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    else if (!pid) {
        //change standard error
        if (dup2(fds[0], 2) == -1) {
            close_folders(fds);
            closedir(dir);

            if (write(1, "Error in: dup2\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        //call gcc
        // gcc call
        char *argv[] = {"gcc", path, "-o", "user.out", NULL};
        execvp(argv[0], argv);
 

        close_folders(fds);
        closedir(dir);
        if (write(1, "Error in: execvp\n", 17) == -1)
            exit(-1);
        exit(-1);
    }
    else {
        int status;
        //get child status
        if (wait(&status) == -1) {
            close_folders(fds);
            closedir(dir);
            if (write(1, "Error in: wait\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        return WEXITSTATUS(status);
    }
}



void timeout_handler(int signum) {
    timeout_occurred = 1;
}



//run the user file
int run_user_file(char input[PATH], int fldrs[2], DIR *dir) {

    //declare in output
    int in, output;
    //open files for i/o
    if ((in = open(input, O_RDONLY)) == -1) {
        close_folders(fldrs);
        closedir(dir);
        // if the condition met
        if (write(1, "Error in: open\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    if ((output = open("output.txt", O_CREAT | O_WRONLY, 0644)) == -1) {
        close_folders(fldrs);
        close(in);
        closedir(dir);
        // if the condition met
        if (write(1, "Error in: open\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    //procees pid
    pid_t pid;
    //fork failed
    if ((pid = fork()) == -1) {
        close_folders(fldrs);
        close(in);
        close(output);
        closedir(dir);

        // if the condition met
        if (write(1, "Error in: fork\n", 15) == -1)
            exit(-1);
        exit(-1);
    } else if (pid == 0) {
        //change for standartization
        if (dup2(in, 0) == -1) {
            close_folders(fldrs);
            close(in);
            close(output);

            closedir(dir);
            if (write(1, "Error in: dup2\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        //change for standartization
        if (dup2(output, 1) == -1) {
            close_folders(fldrs);
            close(in);
            close(output);
            closedir(dir);
            // if the condition met
            if (write(1, "Error in: dup2\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        //standartization
        if (dup2(fldrs[0], 2) == -1) {
            close_folders(fldrs);
            close(in);
            close(output);
            closedir(dir);
            // if the condition met
            if (write(1, "Error in: dup2\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        //call user file
        char *argv[] = {"./user.out", NULL};
        execvp(argv[0], argv);
        close_folders(fldrs);
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

        // Set up signal handling for a timeout
        signal(SIGALRM, timeout_handler);
        // Set alarm for TIMEOUT seconds
        alarm(TIMEOUT);
        
        if ((wait(&status)) == -1) {
            close_folders(fldrs);
            close(in);
            close(output);
            closedir(dir);
            if (write(1, "Error in: wait\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        // Disarm the alarm
        alarm(0);
        close(in);
        close(output);

        if (timeout_occurred) {
        timeout_occurred = 0; // Reset the flag for future use
        return -1; // Return -1 to indicate a timeout occurred
        }

        if (WEXITSTATUS(status) == -1)
            exit(-1);
    }

}


void get_the_path_of_user(char dst[PATH], char path[PATH]) {
    //get the path of the dir
    int len = strlen(dst), last_place = len, i;
    for (i = 0; i < len; i++) {

        if (dst[i] == '/')
            last_place = i;
    }
    // check correction end
    dst[last_place] = '\0';
    strcat(dst, "/");
    strcat(dst, path);
}
// close folders - done
void close_folders(int fds[2]) {
    close(fds[0]);

    close(fds[1]);
}

//rread the configuration file




int compare(int fldrs[2], char expected[PATH], DIR *dir) {
    pid_t pid = fork();
    //fork fail
    if (pid == -1) {
        close_folders(fldrs);
        closedir(dir);

        if (write(1, "Error in: fork\n", 15) == -1)
            exit(-1);
        exit(-1);
    }

        //child process
    else if (pid == 0) {
        // called file comparision
        char *argv[] = {"./comp.out", "output.txt", expected, NULL};
        execvp(argv[0], argv);
        close_folders(fldrs);
        closedir(dir);
        // if the condition met
        if (write(1, "Error in: execvp\n", 17) == -1)
            exit(-1);
        exit(-1);
    }

        //parent process
    else {
        int status;
        //get child status
        if ((wait(&status)) == -1) {

            close_folders(fldrs);
            closedir(dir);
            // if the condition met
            if (write(1, "Error in: wait\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
        return WEXITSTATUS(status);
    }
}

void read_the_path(int fldr, char *dir, char *input, char *expected) {
    char *pos = dir;
    //read 1st line
    if (read(fldr, pos, 1) == -1) {
        
        close(fldr);
        if (write(1, "Error in: read\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    // while there is no new line
    while (*pos != '\n') {

        pos++;
        if (read(fldr, pos, 1) == -1) {
            close(fldr);
            if (write(1, "Error in: read\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
    }
    *pos = '\0';
    pos = input;
    //read 2nd line
    if (read(fldr, pos, 1) == -1) {
        close(fldr);
        if (write(1, "Error in: read\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    while (*pos != '\n') {
        pos++;
        if (read(fldr, pos, 1) == -1) {
            close(fldr);
            if (write(1, "Error in: read\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
    }
    *pos = '\0';
    pos = expected;
    //read 3rd line
    if (read(fldr, pos, 1) == -1) {
        close(fldr);
        if (write(1, "Error in: read\n", 15) == -1)
            exit(-1);
        exit(-1);
    }
    while (*pos != '\n') {
        pos++;
        if (read(fldr, pos, 1) == -1) {
            close(fldr);
            if (write(1, "Error in: read\n", 15) == -1)
                exit(-1);
            exit(-1);
        }
    }
    *pos = '\0';
}
