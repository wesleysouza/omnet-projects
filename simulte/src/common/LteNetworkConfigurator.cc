//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#include "LteNetworkConfigurator.h"
#include <omnetpp/cstringtokenizer.h>


using namespace omnetpp;
using namespace inet;

Define_Module(LteNetworkConfigurator);

void LteNetworkConfigurator::initialize(int stage){
    Ipv4NetworkConfigurator::initialize(stage);
    if (stage == INITSTAGE_LOCAL){
        std::string wirelessInterfaceNames = par("wirelessInterfaceNames").stdstringValue();
        cStringTokenizer tokenizer(wirelessInterfaceNames.c_str(), ",");
        for(const auto& s : tokenizer.asVector()){
            interfaceComparator.push_back(std::make_pair(s, s.size()));
        }
    }
}

bool LteNetworkConfigurator::isWirelessInterface(InterfaceEntry *interfaceEntry)
{
    // if interfaceEntry starts with any of given interface names.
    // starts with is needed to allow vector names like wlan0, wlan1, ...
    for(const auto& e : interfaceComparator){
        if (!strncmp(interfaceEntry->getInterfaceName(), e.first.c_str(), e.second)){
            return true;
        }
    }
    return false;
}


