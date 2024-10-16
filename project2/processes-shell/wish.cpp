#include <iostream>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h> 
#include <sstream>
#include <fstream>
#include <vector>
#include <sys/wait.h> 
#include <cstring>
using namespace std;
#define BUFFER_SIZE 4096

void parsingCommand(const string& cmd, vector<string>& prompts) {
    istringstream inputCommand(cmd);
    string command;
    while(inputCommand >> command) {
        prompts.push_back(command);
    }
}

void displayError() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message)); 
}

void cd(vector<string> commands) {
    if(commands.size() != 2) {
        displayError();
    }
    const char* path = const_cast<char*>(commands[1].c_str());
    chdir(path);
}

//Global initial path variable
vector<string> path = {"/bin/"};
void setPath(vector<string> argpath) {
    path.clear();
    for(const string& newpath : argpath) {
        const char* dash = "/";
        const char* first = new char(newpath[0]);
        string parsedPath = newpath;
        if(!parsedPath.empty() && first != dash) {
            parsedPath = "/" + parsedPath;
        }
        delete first;
        path.push_back(parsedPath);
    }
}

bool checkPath(const string& path) {
    bool valid;
    const char* temp = const_cast<char*>(path.c_str());
    if(access(temp, X_OK) != 0) {
        valid = false;
    } else {
        valid = true;
    }
    return valid;
}

int main(int argc, char* argv[]) {
    //Original path

    /*----------Batch Mode----------*/
    if(argc > 1) {
        int fileDescriptor = open(argv[1], O_RDONLY);
        //Check error
        if(fileDescriptor == -1) {
            cout << "Cannot open file" << endl;
        }
        size_t bytesRead;
        char buffer[BUFFER_SIZE];
        string cmd;
        bool exitEncountered = false;
        vector<string> commandline;
        while((bytesRead = read(fileDescriptor, buffer, sizeof(buffer))) > 0) {
            for(size_t i = 0; i < bytesRead; i++) {
                //Add command and arguments to vector if space is found
                if(buffer[i] == ' ') {
                    commandline.push_back(cmd);
                    cmd.clear();
                    continue;
                }
                //If new line is met then check what we have atm
                if(buffer[i] == '\n') {
                    if(!cmd.empty()) {
                        commandline.push_back(cmd);
                    }
                    if(!commandline.empty()) {
                        //Built-in Commands
                        if(commandline[0] == "exit") {
                            //Check for additional stuff
                            if(commandline.size() == 1) {
                                exitEncountered = true;
                                break;
                            } else {
                                displayError();
                            }
                        } else if(commandline[0] == "cd") {
                            cd(commandline);
                        } else if(commandline[0] == "path") {
                            //Set new Path
                            vector<string> dir(commandline.begin() + 1, commandline.end());
                            setPath(dir);
                            //Check new Path -> if all doesn't work then return error
                            bool final = false;
                            for(size_t i = 0; i < path.size(); i++) {
                                final = final || checkPath(path[i]);
                            }
                            if(final == false) {
                                //none worked
                                displayError();
                                exit(1);
                            } 
                        } else {
                        //Normal commands
                            pid_t pid = fork();
                            if(pid < 0) {
                                exit(1);
                            } else if(pid == 0) {
                                vector<char*> command;
                                for(const string& cmdline : commandline) {
                                    command.push_back(const_cast<char*>(cmdline.c_str()));
                                }
                                command.push_back(nullptr);
                                string final = path[0] + command[0];
                                char* finalPath = new char[final.length() + 1];
                                strcpy(finalPath, final.c_str());
                                cout << final << endl;
                                if(execv(finalPath, command.data()) < 0) {
                                    displayError();
                                }
                                delete[] finalPath;
                            } else {
                                waitpid(pid, NULL, 0);
                            }
                        }
                    }
                    //Clear everything for next line
                    cmd.clear();
                    commandline.clear();
                } else {
                    cmd += buffer[i];
                }
            }
            if(exitEncountered) {
                break;
            }
        }
        close(fileDescriptor);
    }
    /*----------Batch Mode----------*/

    /*----------Interactive mode----------*/
    else {
        string cmd;
        while(true) {
            cout << "prompt> ";
            getline(cin, cmd);       
            vector<string> prompts;
            //Parse the inputCommand
            parsingCommand(cmd, prompts);
            //Skip if nothing was added into command
            if(prompts.empty()) {
                continue;
            }
            //Built-in command - exit
            if(prompts[0] == "exit") {
                if(prompts.size() == 1) {
                    exit(0);
                } else {
                    displayError();
                }
            } else if(prompts[0] == "cd") {
                cd(prompts);
            } else if(prompts[0] == "path") {
                vector<string> newPath(prompts.begin() + 1, prompts.end());
                setPath(newPath);
            } else {
            //Normal Command
                //Create child process for execution of non-built-in commands
                pid_t pid = fork();
                if (pid < 0) {
                    exit(1);
                }
                else if(pid == 0) {
                    //child process
                    //Create argument list of char* <- second argument of execv()
                    vector<char*> command;
                    //Parsed command added to argument list
                    for(string cmd : prompts) {
                        //Convert cmd to char* 
                        command.push_back(strdup(cmd.c_str()));
                    }
                    //Last element of argument list must be nullptr
                    command.push_back(nullptr);
                    /*If ls /file/directory is called with execv
                    executable path -> /bin/ls
                    arg -> {"ls", "/file/directory", nullptr} */
                    //Combine current path with command[0] -> executable path
                    string final = path[0] + command[0];
                    char* finalPath = new char[final.length() + 1];
                    strcpy(finalPath, final.c_str());
                    if(execv(finalPath, command.data()) < 0) {
                        displayError();
                    }
                    delete[] finalPath;
                    exit(1);
                } else {
                    //Wait until the command finishes executing
                    waitpid(pid, NULL, 0);
                }
            }
        }
    }
    /*----------Interactive Mode----------*/
    return 0;
}   