//
//  common.cpp
//  
//
//  Created by John La Velle on 3/26/18.
//
#include <iostream>
#include <sstream>
#include "common.hpp"

char * gather_uid(int mode, int artn) {
    std::stringstream stream;
    char * u_id = NULL;
    int page, pgent;
    
    if (mode) {
        stream << replies[artn].size();
        return (char *)stream.str().c_str();
    } else {
        stream << replies[artn].size();
        return (char *)stream.str().c_str();
    }    
}
