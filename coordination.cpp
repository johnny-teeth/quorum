//
//  coordination.cpp
//  
//
//  Created by John La Velle on 3/26/18.
//
#include <fcntl.h>
#include <iostream>
#include <sstream>
#include <thread>
#include "common.hpp"
#include "coordination.hpp"

#define PORT 7773
#define LISTEN 10

std::vector<struct client> clients;
int num_serve;
int cli_sock, loc_sock;
struct sockaddr_in cli_serve, loc_serve;
int vote_num = 0;

std::string gather_uid(int mode, int artn) {
    std::stringstream stream;
    char * u_id = NULL;
    int page, pgent;
    
    if (mode) {
        stream << artn << "." << replies[artn].size() + 1;
        return stream.str();
    } else {
        stream << articles.size() + 1 << ".0";
        std::cout << stream.str() << std::endl;
        return stream.str();
    }
}

int election(int sock, int mode) {
    int quorum, abort = 0;
    size_t rdsz;
    std::stringstream strs;
    std::string ballot;
    struct sockaddr_in qsadr;
    char * resp = (char *)malloc(MAX_MSG);
    
    int quorsock = socket(AF_INET, SOCK_DGRAM, 0);
    int flags = fcntl(quorsock, F_GETFL);
    fcntl(quorsock, F_SETFL, flags | O_NONBLOCK);
    qsadr.sin_family = AF_INET;
    qsadr.sin_port = htons(PORT - 1);
    qsadr.sin_addr.s_addr = htonl(INADDR_ANY);
    
    std::cout << "Performing election" << std::endl;
    
    if (bind(quorsock, (struct sockaddr *)&qsadr, sizeof(qsadr)) < 0) {
        fprintf(stdout, "Election bind failed\n");
        free(resp);
        return 1;
    }
    
    if (num_serve) {
        if (mode) {
            quorum = clients.size();
        } else {
            quorum = 1;
        }
    } else {
        if (mode) {
            quorum = (clients.size() / 2) + 1;
        } else {
            quorum = (clients.size() / 2);
        }
    }
    
    if (mode) {
        strs << "VOTE:WRITE:" << vote_num;
        vote_num++;
    } else {
        strs << "VOTE:READ" << vote_num;
        vote_num++;
    }
    
    ballot = strs.str();
    
    for (int i = 0; i < quorum; i++) {
        struct sockaddr_in clad;
        struct client cl = clients.at(i);
        if (cl.sock == sock) {
            if (quorum != clients.size()) {
                quorum++;
            }
            continue;
        }
        clad.sin_family = AF_INET;
        clad.sin_port = htons(cl.port - 1);
        clad.sin_addr.s_addr = inet_addr(cl.addr.c_str());
        sendto(quorsock, ballot.c_str(), strlen(ballot.c_str()), 0, (struct sockaddr *)&clad, sizeof(clad));
    }
    for (int i = 0; i < quorum; i++) {
        int count = 0;
        struct sockaddr_in cladr;
        socklen_t slen;
        struct client cl = clients.at(i);
        if (cl.sock == sock)
            continue;
        
        while((rdsz = recvfrom(quorsock, resp, MAX_MSG, 0, (struct sockaddr *)&cladr, &slen)) == -1 && count < 300) {
            usleep(100);
            count++;
        }
        if (count < 300) {
            resp[rdsz] = '\0';
            if (strncmp(resp, "ABORT", 5) == 0) {
                abort++;
            }
        }
    }
    close(quorsock);
    if (abort > (quorum / 2)) {
        return 1;
    }
    return 0;
}

void synch(int sock, char * buf) {
    size_t rdsz = read(sock, buf, MAX_MSG);
    int numrep, repsz = 0;
    int numarts = atoi(buf);
    if (numarts == articles.size()) {
        write(sock, (char *)"GOOD", 4);
    } else {
        write(sock, (char *)"FAIL", 4);
        memset(buf, 0, MAX_MSG);
        for (std::vector<std::string>::iterator iter = articles.begin(); iter != articles.end(); ++iter){
            rdsz = read(sock, buf, MAX_MSG);
            buf[rdsz] = '\0';
            if (std::string(buf) == (*iter)) {
                write(sock, (char *)"GOOD", 4);
            } else {
                write(sock, (*iter).c_str(), strlen((*iter).c_str()));
            }
            memset(buf, 0, MAX_MSG);
        }
        write(sock, (char *)"END", 3);
    }
    rdsz = read(sock, buf, MAX_MSG);
    buf[rdsz] = '\0';
    numrep = atoi(buf);
    for (int k = 0; k < replies.size(); k++) {
        repsz += replies[k].size();
    }
    if (numrep == repsz) {
        write(sock, (char *)"GOOD", 4);
    } else {
        write(sock, (char *)"FAIL", 4);
        memset(buf, 0, MAX_MSG);
        for (std::map<int, std::vector<std::string> >::iterator iter = replies.begin(); iter != replies.end(); ++iter) {
            for (std::vector<std::string>::iterator it = ((*iter).second).begin(); it != ((*iter).second).end(); ++it) {
                rdsz = read(sock, buf, MAX_MSG);
                buf[rdsz] = '\0';
                if (std::string(buf) == *it) {
                    write(sock, (char *)"GOOD", 4);
                } else {
                    write(sock, (*it).c_str(), strlen((*it).c_str()));
                }
                memset(buf, 0 , MAX_MSG);
            }
            write(sock, (char *)"END", 3);
        }
        write(sock, (char *)"END", 3);
    }
}

void get_page(int sock, char * buf) {
    int page = atoi(buf + 5);
    if (page > (articles.size() / 10)) {
        write(sock, (char *)"PAGE UNAVAILABLE", strlen((char *)"PAGE UNAVAILABLE"));
    } else {
        std::string message = "";
        int start = page * 10;
        if (!start) {
            for (int k = start; (k < start + 10) && (k < articles.size()); k++) {
                message = message + std::to_string(k + 1) + ".0 ) " + articles.at(k).substr(0, 25) + "...\n";
            }
        } else {
            for (int k = start - 1; (k < start + 9) && (k < articles.size()); k++) {
                message = message + std::to_string(k + 1) + ".0 )" + articles.at(k).substr(0, 25) + "...\n";
            }
        }
        if (message.length() == 0)
            message = "PAGE UNAVAILABLE";
        write(sock, message.c_str(), strlen(message.c_str()));
    }
}

void get_article(int sock, char * buf) {
    std::string article_id(buf);
    int index = article_id.find_first_of('.', 0);
    int article = atoi(article_id.substr(0, index).c_str());
    int reply = atoi(article_id.substr(index + 1).c_str());
    std::cout << article_id << " " << index << " " << article << " " << reply << std::endl;
    std::cout << articles.at(article - 1) << std::endl;
    if (reply == 0) {
        write(sock, articles.at(article - 1).c_str(), strlen(articles.at(article - 1).c_str()));
    } else {
        std::string message = "";
        for (int i = 0; i < replies[article - 1].size(); i++) {
            message += replies[article - 1].at(i);
        }
        if (message.length() == 0) {
            message = "No replies";
        }
        write(sock, message.c_str(), strlen(message.c_str()));
    }
}
              
void write_post(int sock, char * buf) {
    size_t rdsz;
    memset(buf, 0, MAX_MSG);
    write(sock, (char *)"COMMIT", strlen((char *)"COMMIT"));
    rdsz = read(sock, buf, MAX_MSG);
    buf[rdsz] = '\0';

    std::string uid = gather_uid(0, 0);
    char * comstr = (char *)malloc(snprintf(NULL, 0, "%s:%s", uid.c_str(), buf));
    rdsz = sprintf(comstr, "%s:%s", uid.c_str(), buf);
    comstr[rdsz] = '\0';
    std::string commit(buf);
    articles.push_back(commit);

    for (int i = 0; i < clients.size(); i++) {
      struct client cl = clients.at(i);
      write(cl.sock, comstr, strlen(comstr));
    }
}
              
void write_reply(int sock, char * buf) {
    size_t rdsz;
    int coloncnt = 0, i = 0;
    std::cout << buf << std::endl;
    while (coloncnt != 2) {
      if (buf[i] == ':')
          coloncnt++;
      i++;
    }
    
    char * anumstr = (char *)malloc(strlen(buf) - i);
    strncpy(anumstr, buf + i, strlen(buf) - i);
    int artn = atoi(anumstr);
    std::cout << buf << " " << artn << " " << anumstr << std::endl;
    std::string uid = gather_uid(1, artn);
    
    free(buf);
    buf = (char *)malloc(MAX_MSG);
    write(sock, (char*)"COMMIT", strlen((char *)"COMMIT"));
    rdsz = read(sock, buf, MAX_MSG);
    buf[rdsz] = '\0';
    
    replies[artn - 1].push_back(buf);
    std::cout << replies[artn - 1].at(0) << std::endl;
    char * comstr = (char *)malloc(snprintf(NULL, 0, "%s:%s", uid.c_str(), buf));
    rdsz = sprintf(comstr, "%s:%s", uid.c_str(), buf);
    comstr[rdsz] = '\0';
    
    for (i = 0; i < clients.size(); i++) {
      struct client cl = clients.at(i);
      write(cl.sock, comstr, strlen(comstr));
    }
}

void serve_select(int sock) {
    fd_set readset;
    size_t rdsz;
    FD_SET(sock, &readset);
    char * buf;
    while (1) {
        buf = (char *)malloc(MAX_MSG);
        select(sock + 1, &readset, NULL, NULL, NULL);
        if (FD_ISSET(sock, &readset)) {
            if((rdsz = read(sock, buf, MAX_MSG)) <= 0) {
                fprintf(stdout, "Read failure: serve_select\n");
                free(buf);
                close(sock);
                return;
            }
            buf[rdsz] = '\0';
            std::cout << "Select: " << buf << std::endl;
            if (strncmp(buf, (char *)"READ", 4) == 0) {
                if (election(sock, 0) == 0) {
                    free(buf);
                    write(sock, (char *)"COMMIT", strlen("COMMIT"));
                    buf = (char *)malloc(MAX_MSG);
                    rdsz = read(sock, buf, MAX_MSG);
                    buf[rdsz] = '\0';
                    
                    if (strncmp(buf, (char *)"PAGE:", 4) == 0) {
                        get_page(sock, buf);
                    } else {
                        std::cout << "Getting article" << std::endl;
                        get_article(sock, buf);
                    }
                
                }
                
            } else if (strncmp(buf, (char *)"WRITE", 5) == 0) {
                if (election(sock, 1) == 0) {
                    if (strncmp(buf, (char*)"WRITE:POST", 10) == 0) {
                        write_post(sock, buf);
                    } else if (strncmp(buf, (char *)"WRITE:REPLY:", 10) == 0){
                        write_reply(sock, buf);
                    }
                
                } else {
                    write(sock, (char *)"ABORT", strlen((char *)"ABORT"));
                }
            } else if (strncmp(buf, (char *)"SYNCH", 5) == 0) {
                memset(buf, 0, MAX_MSG);
                synch(sock, buf);
            } else {
                std::cout << "Unknown message: " << buf << std::endl;
            }
        }
    }
}

void gather_nodes() {
    int j;
    size_t rdsz;
    if (setsockopt(loc_sock, SOL_SOCKET, SO_REUSEADDR, &j, sizeof(int)) == -1) {
        fprintf(stdout, "setsockopt fail\n");
        return;
    }
    if (bind(loc_sock, (struct sockaddr *)&loc_serve, sizeof(loc_serve)) == -1) {
        fprintf(stdout, "rep bind fail\n");
        return;
    }
    if (listen(loc_sock, LISTEN) == -1) {
        fprintf(stdout, "listen fail\n");
        return;
    }
    
    while (1) {
        int repsock, i = 0, k;
        struct sockaddr_in repserve;
        socklen_t slen;
        struct client cl;
        char * attrs = (char *)malloc(MAX_MSG);
        
        repsock = accept(loc_sock, (struct sockaddr *)&repserve, &slen);
        cl.sock = repsock;
        
        rdsz = read(repsock, attrs, MAX_MSG);
        attrs[rdsz] = '\0';
        
        while(attrs[i] != ':')
            i++;
        char * temp = (char *)malloc(i);
        memcpy(temp, attrs, i);
        temp[i] = '\0';
        
        cl.port = atoi(temp);
        cl.addr = std::string(attrs + i + 1);
        
        std::cout << "Replicate added: " << cl.port << " " << cl.addr << std::endl;
        
        clients.push_back(cl);
        free(temp);
        
        for (i = 0; i < articles.size(); i++) {
            std::cout << "Sending: " << articles.at(i) << std::endl;
            char * temp = (char *)malloc(MAX_MSG);
            std::string article = std::to_string(i) + ".0:" + articles.at(i);
            write(repsock, article.c_str(), strlen(article.c_str()));
            usleep(300);
            for (k = 0; k < replies[i].size(); k++) {
                std::string reply = std::to_string(i) + "." + std::to_string(k) + ":" + replies[i].at(k);
                write(repsock, reply.c_str(), strlen(reply.c_str()));
                usleep(300);
            }
        }
        std::thread serve_sel(serve_select, repsock);
        serve_sel.detach();
    }
}

void gather_cli() {
    int j;
    if (setsockopt(cli_sock, SOL_SOCKET, SO_REUSEADDR, &j, sizeof(int)) == -1) {
        fprintf(stdout, "setsockopt fail\n");
        return;
    }
    if (bind(cli_sock, (struct sockaddr *)&cli_serve, sizeof(cli_serve)) == -1) {
        fprintf(stdout, "cli bind fail\n");
        return;
    }
    if (listen(cli_sock, LISTEN) == -1) {
        fprintf(stdout, "listen fail\n");
        return;
    }
    j = 0;
    while (1) {
        struct sockaddr_in temp;
        socklen_t slen;
        std::stringstream strm;
        
        int cs = accept(cli_sock, (struct sockaddr *)&temp, &slen);
        
        struct client cl = clients.at((j % clients.size()));
        strm << cl.port << ":" << cl.addr;
        write(cs, strm.str().c_str(), strlen(strm.str().c_str()));
        close(cs);
        j++;
    }
}

void create_connection(char * addr) {
    loc_sock = socket(AF_INET, SOCK_STREAM, 0);
    loc_serve.sin_family = AF_INET;
    loc_serve.sin_port = htons(PORT);
    loc_serve.sin_addr.s_addr = inet_addr(addr);
    
    cli_sock = socket(AF_INET, SOCK_STREAM, 0);
    cli_serve.sin_family = AF_INET;
    cli_serve.sin_port = htons(PORT + 3);
    cli_serve.sin_addr.s_addr = inet_addr(addr);
    
    std::thread repthread(gather_nodes);
    repthread.detach();
    gather_cli();
}

int main(int argc, char ** argv) {
    char * addr = NULL;
    if (argc < 5) {
        fprintf(stdout, "USAGE: %s -a [ip address] -q [1 | 0]\n", argv[0]);
        fprintf(stdout, " a   =>   ip address of host\n");
        fprintf(stdout, " q   =>   1 - single read quorum\n");
        exit(0);
    }
    
    for (int i = 0; i < argc; i++) {
        if (strlen(argv[i]) == 2) {
            if (argv[i][1] == 'a') {
                addr = strdup(argv[++i]);
            } else if (argv[i][1] == 'q') {
                num_serve = atoi(argv[++i]);
            }
        }
    }
    
    if (addr == NULL || num_serve > 1) {
        fprintf(stdout, "USAGE: %s -a [ip address] -q [1 | 0]\n", argv[0]);
        fprintf(stdout, " a   =>   ip address of host\n");
        fprintf(stdout, " q   =>   1 - single read quorum\n");
        exit(0);
    }
    
    create_connection(addr);
}
