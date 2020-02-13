//
//  replicate.cpp
//  
//
//  Created by John La Velle on 3/26/18.
//

#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include <iostream>
#include <ctime>
#include <sstream>
#include <vector>
#include <map>
#include <thread>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#include "replicate.hpp"
#include "common.hpp"

#define SERVERPORT 7773
#define LISTEN 10

std::vector<struct client> clients;
int cli_sock, loc_sock, udp_sock;
struct sockaddr_in cli, loc, udp;
int in_election = 0;
std::mutex lock;

void synch() {
    char * buf = (char *)malloc(MAX_MSG);
    int rdsz;
    int foundend = 0;
    std::stringstream stream;
    write(loc_sock, (char *)"SYNCH", strlen((char *)"SYNCH"));
    stream << articles.size();
    write(loc_sock, stream.str().c_str(), strlen(stream.str().c_str()));
    while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
        memset(buf, 0, 1);
    }
    buf[rdsz] = '\0';
    if (strncmp(buf, "GOOD", 4) == 0) {  // Articles match
        stream.str(std::string());
        int sz = 0;
        for (int k = 0; k < replies.size(); k++) {  // Check replies
            sz += replies[k].size();
        }
        write(loc_sock, stream.str().c_str(), strlen(stream.str().c_str()));
        memset(buf, 0, MAX_MSG);
        while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
            memset(buf, 0, 1);
        }
        if (strncmp(buf, "GOOD", 4) == 0) {  // Replies match
            return;
        } else {  // Replies mismatch
            for (std::map<int, std::vector<std::string>>::iterator iter = replies.begin(); iter != replies.end(); ++iter) {
                for (std::vector<std::string>::iterator it = ((*iter).second).begin(); it != ((*iter).second).end(); ++it) {
                    write(loc_sock, (*it).c_str(), strlen((*it).c_str()));  // Check each reply
                    memset(buf, 0, MAX_MSG);
                    while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                        memset(buf, 0, 1);
                    }
                    if (strncmp(buf, "GOOD", 4) == 0) {
                        continue;
                    } else if (strncmp(buf, "END", 3) == 0) {
                        foundend = 1;
                        while (it != ((*iter).second).end()) {
                            it = ((*iter).second).erase(it);
                        }
                    } else {
                        it = ((*iter).second).insert(it, buf);  // Insert missing articles
                    }
                }
                if (!foundend) {
                    memset(buf, 0, MAX_MSG);
                    while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                        memset(buf, 0, 1);
                    }
                    while (strncmp(buf, (char *)"END", 3) != 0) {
                        int l = replies.size();
                        replies[l - 1].push_back(buf);
                        memset(buf, 0, MAX_MSG);
                        while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                            memset(buf, 0, 1);
                        }
                        buf[rdsz] = '\0';
                    }
                }
                foundend = 0;
            }
            if (!foundend) {
                memset(buf, 0, MAX_MSG);
                while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                    memset(buf, 0, 1);
                }
                while (strncmp(buf, (char *)"END", 3) != 0) {
                    int l = replies.size();
                    replies[l].push_back(buf);
                    memset(buf, 0, MAX_MSG);
                    while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                        memset(buf, 0, 1);
                    }
                    buf[rdsz] = '\0';
                }
            }
        }
    } else {  // Mismatched articles
        
        for (std::vector<std::string>::iterator iter = articles.begin(); iter != articles.end(); ++iter) {
            write(loc_sock, (*iter).c_str(), strlen((*iter).c_str()));
            memset(buf, 0, MAX_MSG);
            
            while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {  // Check each article
                memset(buf, 0, 1);
            }
            buf[rdsz] = '\0';
            
            if (strncmp(buf, "GOOD", 4) == 0) {
                continue;
            } else if (strncmp(buf, "END", 3) == 0) {
                foundend = 1;
                while (iter != articles.end()) {
                    iter = articles.erase(iter);
                }
            } else {
                iter = articles.insert(iter, buf);  // Insert missing article
            }
        }
        
        if (!foundend) {  // Check for aricles added at end of coordinator vector
            memset(buf, 0, MAX_MSG);
            
            while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {  // Check each article
                memset(buf, 0, 1);
            }
            buf[rdsz] = '\0';
            
            while(strncmp(buf, (char *)"END", 3) != 0) {
                articles.push_back(std::string(buf));
                memset(buf, 0, MAX_MSG);
                while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                    memset(buf, 0, 1);
                }
                buf[rdsz] = '\0';
            }
        }
        foundend = 0;
        
        stream.str(std::string());
        int sz = 0;
        for (int k = 0; k < replies.size(); k++) {  // Check replies
            sz += replies[k].size();
        }
        
        write(loc_sock, stream.str().c_str(), strlen(stream.str().c_str()));
        
        memset(buf, 0, MAX_MSG);
        while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
            memset(buf, 0, 1);
        }
        buf[rdsz] = '\0';
        
        if (strncmp(buf, "GOOD", 4) == 0) {
            return;
        } else {  // Mismatched replies
            for (std::map<int, std::vector<std::string>>::iterator iter = replies.begin(); iter != replies.end(); ++iter) {
                for (std::vector<std::string>::iterator it = ((*iter).second).begin(); it != ((*iter).second).end(); ++it) {
                    write(loc_sock, (*it).c_str(), strlen((*it).c_str()));
                    memset(buf, 0, MAX_MSG);
                    while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                        memset(buf, 0, 1);
                    }
                    if (strncmp(buf, "GOOD", 4) == 0) {
                        continue;
                    } else if (strncmp(buf, "END", 3) == 0) {  // Remove elements past curr pos
                        foundend = 1;
                        while (it != ((*iter).second).end()) {
                            it = ((*iter).second).erase(it);
                        }
                    } else {
                        it = articles.insert(it, buf);  // Insert missing replies
                    }
                }
                
                if (!foundend) {
                    memset(buf, 0, MAX_MSG);
                    while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                        memset(buf, 0, 1);
                    }
                    while (strncmp(buf, (char *)"END", 3) != 0) {
                        int l = replies.size();
                        replies[l - 1].push_back(buf);
                        memset(buf, 0, MAX_MSG);
                        while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                            memset(buf, 0, 1);
                        }
                        buf[rdsz] = '\0';
                    }
                }
                foundend = 0;
            }
            if (!foundend) {
                memset(buf, 0, MAX_MSG);
                while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                    memset(buf, 0, 1);
                }
                while (strncmp(buf, (char *)"END", 3) != 0) {
                    int l = replies.size();
                    replies[l].push_back(buf);
                    memset(buf, 0, MAX_MSG);
                    while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                        memset(buf, 0, 1);
                    }
                    buf[rdsz] = '\0';
                }
            }
        }
    }
}

void vote_listen() {
    size_t reclen;
    struct sockaddr_in recer;
    socklen_t slen;
    char * buf;
    
    if (bind(udp_sock, (struct sockaddr *)&udp, sizeof(udp)) < 0) {
        std::cout << "Voter unable to bind" << std::endl;
    }
    
    while (1) {
        char * temp;
        std::stringstream stream;
        buf = (char *)malloc(MAX_MSG);
        reclen = recvfrom(udp_sock, buf, MAX_MSG, 0, (struct sockaddr *)&recer, &slen);
        lock.lock();
        if (in_election) {
            sendto(udp_sock, (char *)"ABORT", strlen((char *)"ABORT"), 0, (struct sockaddr *)&recer, slen);
            lock.unlock();
            continue;
        }
        
        if (strncmp(buf, "VOTE:WRITE:", 10) == 0) {
            temp = (char *)malloc(strlen(buf) - 11);
            strcpy(temp, buf + 11);
            stream << "GOODWRITE:" << temp;
            sendto(udp_sock, stream.str().c_str(), strlen(stream.str().c_str()), 0, (struct sockaddr *)&recer, slen);
            in_election = 1;
        } else if (strncmp(buf, "VOTE:READ:", 9) == 0) {
            temp = (char *)malloc(strlen(buf) - 9);
            strcpy(temp, buf + 9);
            stream << "GOODWRITE:" << temp;
            sendto(udp_sock, stream.str().c_str(), strlen(stream.str().c_str()), 0, (struct sockaddr *)&recer, slen);
        }
        lock.unlock();
    }
}

void client_select(int sock) {
    fd_set readset;
    int rdsz;
    char * buf;
    int count;
    
    while (1) {
        count = 0;
        FD_SET(sock, &readset);
        select(sock + 1, &readset, NULL, NULL, NULL);
        if (FD_ISSET(sock, &readset)) {
            buf = (char *)malloc(MAX_MSG);
            rdsz = read(sock, buf, MAX_MSG);
            buf[rdsz] = '\0';
            fprintf(stdout, "Read: %s\n", buf);
            if (strncmp(buf, "REPLY:", 5) == 0) {
                char * buf2 = (char *)malloc(strlen(buf) + 6);
                sprintf(buf2, "WRITE:%s", buf);
                memset(buf, 0, MAX_MSG);
                lock.lock();
attempt_reply:
                if (count >= 3) {
                    write(sock, (char *)"UNABLE TO REPLY: ELECTION RUNNING", strlen((char *)"UNABLE TO REPLY: ELECTION RUNNING"));
                    free(buf);
                    lock.unlock();
                    continue;
                }
                
                write(loc_sock, buf2, strlen(buf2));
                usleep(300);
                while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                    usleep(300);
                }
                buf[rdsz] = '\0';
                if (strncmp(buf, "ABORT", 5) == 0) {
                    count++;
                    memset(buf, 0, rdsz);
                    goto attempt_reply;
                }
                
                memset(buf, 0, rdsz);
                write(sock, (char *)"COMMIT", strlen((char *)"COMMIT"));
                std::cout << "commited to client" << std::endl;
                rdsz = read(sock, buf, MAX_MSG);
                buf[rdsz] = '\0';
                write(loc_sock, buf, rdsz);
                write(sock, "1", 1);
                lock.unlock();
                
                free(buf2);
            } else if (strncmp(buf, "POST", 4) == 0) {
                memset(buf, 0, rdsz);
                rdsz = sprintf(buf, "WRITE:POST");
                buf[rdsz] = '\0';
                lock.lock();
attempt_post:
                if (count >= 3) {
                    write(sock, (char *)"UNABLE TO POST: ELECTION RUNNING", strlen((char *)"UNABLE TO POST: ELECTION RUNNING"));
                    free(buf);
                    lock.unlock();
                    continue;
                }
                
                std::cout << "Asking coordinator" << std::endl;
                write(loc_sock, buf, strlen(buf));
                usleep(300);
                while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                    usleep(300);
                }
                buf[rdsz] = '\0';
                std::cout << "Received: " << buf << std::endl;
                if (strncmp(buf, "ABORT", 5) == 0) {
                    count++;
                    memset(buf, 0, rdsz);
                    lock.unlock();
                    goto attempt_post;
                }
                
                memset(buf, 0, rdsz);
                write(sock, (char *)"COMMIT", strlen((char *)"COMMIT"));
                rdsz = read(sock, buf, MAX_MSG);
                buf[rdsz] = '\0';
                std::cout << "Received: " << buf << std::endl;
                write(loc_sock, buf, rdsz);
                write(sock, "1", 1);
                lock.unlock();
            } else if (strncmp(buf, "SENDPAGE", 8) == 0) {
                buf = buf + 9;
                fprintf(stdout, "Getting page: %s\n", buf);
                int pgnum = atoi(buf);
                buf = buf - 9;
                lock.lock();
attempt_page:
                if (count >= 3) {
                    write(sock, (char *)"UNABLE TO READ PAGE: ELECTION RUNNING", strlen((char *)"UNABLE TO READ PAGE: ELECTION RUNNING"));
                    free(buf);
                    lock.unlock();
                    continue;
                }
                
                write(loc_sock, (char *)"READ", strlen((char *)"READ"));
                std::cout << "Message server for page" << std::endl;
                usleep(300);
                in_election = 1;
                while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                    usleep(300);
                }
                buf[rdsz] = '\0';
                if (strncmp(buf, "ABORT", 5) == 0) {
                    count++;
                    memset(buf, 0, rdsz);
                    goto attempt_page;
                }
                
                memset(buf, 0, rdsz);
                rdsz = sprintf(buf, "PAGE:%d", pgnum);
                buf[rdsz] = '\0';
                write(loc_sock, buf, strlen(buf));
                usleep(30);
                free(buf);
                buf = (char *)malloc((MAX_MSG * 10) + 20);
                while ((rdsz = read(loc_sock, buf,(MAX_MSG * 10) + 20)) <= 1) {
                    usleep(300);
                }
                buf[rdsz] = '\0';
                write(sock, buf, rdsz);
                lock.unlock();
            } else if (strncmp(buf, "READ", 4) == 0){
                int i = 0;
                char * temp = (char *)malloc(MAX_MSG);
                lock.lock();
attempt_read:
                if (count >= 3) {
                    write(sock, (char *)"UNABLE TO READ: ELECTION RUNNING", strlen((char *)"UNABLE TO READ: ELECTION RUNNING"));
                    free(buf - 5);
                    lock.unlock();
                    continue;
                }
                
                write(loc_sock, (char *)"READ", 4);
                usleep(300);
                while ((rdsz = read(loc_sock, temp, MAX_MSG)) <= 1) {
                    usleep(300);
                }
                temp[rdsz] = '\0';
                if (strncmp(temp, "ABORT", 5) == 0) {
                    count++;
                    memset(buf, 0, MAX_MSG);
                    lock.unlock();
                    goto attempt_read;
                }
                
                write(loc_sock, buf + 5, strlen(buf + 5));
                usleep(300);
                memset(buf, 0, rdsz);
                while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
                    usleep(300);
                }
                buf[rdsz] = '\0';
                write(sock, buf, rdsz);
                lock.unlock();
            } else if (strncmp(buf, "CLOSE", 5) == 0) {
                close(sock);
                pthread_exit(NULL);
            } else {
                std::cout << "Unknown message: " << buf << std::endl;
            }
            
            free(buf);
        }
    }
}

void get_clients() {
    int j;
    if (setsockopt(cli_sock, SOL_SOCKET, SO_REUSEADDR, &j, sizeof(int)) < 0) {
        std::cout << "Set socket options failed: client thread" << std::endl;
        return;
    }
    if (bind(cli_sock, (struct sockaddr *)&cli, sizeof(cli)) < 0) {
        std::cout << "Unable to bind: client thread" << std::endl;
        return;
    }
    if (listen(cli_sock, LISTEN) < 0) {
        std::cout << "Client thread: listen failed" << std::endl;
    }
    
    while (1) {
        struct client cl;
        struct sockaddr_in c_sa;
        socklen_t slen;
        
        int c_sock = accept(cli_sock, (struct sockaddr *)&c_sa, &slen);
        cl.sock = c_sock;
        cl.sa = c_sa;
        clients.push_back(cl);
        
        std::thread cli_sel(client_select, c_sock);
        cli_sel.detach();
    }
}

void serve_updates(int port, char * addr) {
    int rdsz;
    int i, j, hascontent;
    char * buf;
    std::stringstream stream;
    std::string connector;
    std::clock_t timer;
    
    stream << port << ":" << addr;
    connector = stream.str();
    std::cout << "Sending: " << connector << std::endl;
    write(loc_sock, connector.c_str(), strlen(connector.c_str()));
    sleep(1);
    timer = std::clock();
    while (1) {
restart:
        hascontent = 0;
        i = 0;
        buf = (char *)malloc(MAX_MSG);
        lock.lock();
        while ((rdsz = read(loc_sock, buf, MAX_MSG)) <= 1) {
            lock.unlock();
            usleep(600);
            free(buf);
            goto restart;
        }
        
        while (buf[i] != '\0') {
            if (buf[i] == ':') {
                hascontent = 1;
                break;
            }
            i++;
        }
        
        if (!hascontent) {
            free(buf);
            lock.unlock();
            goto restart;
        }
        
        lock.unlock();
        std::cout << "Server: " << buf << std::endl;
        while (buf[i] != ':')
            i++;
        char * uid = (char *)malloc(i);
        memcpy(uid, buf, i);
        uid[i] = '\0';
        j = i + 1;
        
        i = 0;
        while(uid[i] != '.')
            i++;
        char * anum = (char *)malloc(i);
        memcpy(anum, uid, i);
        anum[i] = '\0';
        i++;
        
        char * rnum = (char *)malloc(strlen(uid) - i);
        strcpy(rnum, uid + i);
        int repn = atoi(rnum);
        std::string art(buf + j);
        
        if (repn == 0) {
            lock.lock();
            articles.push_back(art);
            in_election = 0;
            lock.unlock();
        } else {
            int index = atoi(anum);
            if (replies.find(index) != replies.end()) {
                lock.lock();
                replies[index].push_back(art);
                in_election = 0;
                lock.unlock();
            }
        }
        std::cout << "Unlocked" << std::endl;
        if (((std::clock() - timer) / (double) CLOCKS_PER_SEC) > 600) {
            std::cout << "Syncing with coordinator" << std::endl;
            lock.lock();
            synch();
            lock.unlock();
        }
    }
}

void create_connection(char * addr, int port, char * saddr) {
    cli_sock = socket(AF_INET, SOCK_STREAM, 0);
    cli.sin_family = AF_INET;
    cli.sin_port = htons(port);
    cli.sin_addr.s_addr = inet_addr(addr);
    
    std::thread client_thr(get_clients);
    
    loc_sock = socket(AF_INET, SOCK_STREAM, 0);
    int flags = fcntl(loc_sock, F_GETFL);
    fcntl(loc_sock, F_SETFL, flags | O_NONBLOCK);
    
    loc.sin_family = AF_INET;
    loc.sin_port = htons(SERVERPORT);
    loc.sin_addr.s_addr = inet_addr(saddr);
    if (connect(loc_sock, (struct sockaddr *)&loc, sizeof(loc)) < 0) {
        sleep(1);
    }
    
    udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    udp.sin_family = AF_INET;
    udp.sin_port = htons(port - 1);
    udp.sin_addr.s_addr = inet_addr(addr);
    std::thread voter(vote_listen);
    
    serve_updates(port, addr);
}

int main(int argc, char ** argv) {
    char * addr = NULL, * saddr = NULL;
    int port = -1, i;
    if (argc < 7) {
        std::cout << "USAGE: %s -p [port] -a [address] -s [address]" << std::endl;
        std::cout << "   p   =>   port for connecting to replicate" << std::endl;
        std::cout << "   a   =>   address for connecting to replicate" << std::endl;
        std::cout << "   s   =>   address for coordinator server" << std::endl;
        exit(0);
    }
    
    for (i = 0; i < argc; i++) {
        if (strlen(argv[i]) == 2) {
            if (argv[i][1] == 'a') {
                addr = strdup(argv[++i]);
            } if (argv[i][1] == 'p') {
                port = atoi(argv[++i]);
            } if (argv[i][1] == 's') {
                saddr = strdup(argv[++i]);
            }
        }
    }
    
    if (addr == NULL || port < 0 || saddr == NULL) {
        std::cout << "USAGE: %s -p [port] -a [address]" << std::endl;
        std::cout << "   p   =>   port for connecting to replicate" << std::endl;
        std::cout << "   a   =>   address for connecting to replicate" << std::endl;
        std::cout << "   s   =>   address for coordinator server" << std::endl;
        exit(0);
    }
    
    create_connection(addr, port, saddr);
}
