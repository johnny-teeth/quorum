//
//  coordination.hpp
//  
//
//  Created by John La Velle on 3/26/18.
//

#ifndef coordination_hpp
#define coordination_hpp

#include <stdio.h>

struct client {
    int sock;
    int port;
    std::string addr;
};


std::vector<std::string> articles;
std::map<int, std::vector<std::string> > replies;

void write_post(int sock, char * buf);
void write_reply(int sock, char * buf);

#endif /* coordination_hpp */
