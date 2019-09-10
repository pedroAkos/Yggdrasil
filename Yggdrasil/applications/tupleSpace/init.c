//
// Created by Pedro Akos on 2019-09-04.
//

#include "ygg_runtime.h"


void main(int argc, char* argv[]) {
    NetworkConfig* ntconf = defineWirelessNetworkConfig("AdHoc", 0, 5, 0, "ledge", YGG_filter);

    ygg_runtime_init(ntconf);
}