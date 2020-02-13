//
//  replicate.hpp
//  
//
//  Created by John La Velle on 3/26/18.
//

#ifndef replicate_hpp
#define replicate_hpp

#include <stdio.h>
#include <string>

struct client {
    int sock;
    struct sockaddr_in sa;
};

std::vector<std::string> articles;
std::map<int, std::vector<std::string> > replies;

#endif /* replicate_hpp */
