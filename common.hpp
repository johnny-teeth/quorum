//
//  common.hpp
//  
//
//  Created by John La Velle on 3/26/18.
//

#ifndef common_hpp
#define common_hpp

#include <stdio.h>
#include <vector>
#include <map>
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define MAX_MSG 128

std::string gather_uid(int mode, int artn);
#endif /* common_hpp */
