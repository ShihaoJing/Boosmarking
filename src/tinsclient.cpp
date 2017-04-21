//
// Created by shihaojing on 4/20/17.
//
#include <iostream>
#include <iomanip>
#include <vector>
#include <set>
#include <string>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <tins/ip.h>
#include <tins/tcp.h>
#include <tins/ip_address.h>
#include <tins/ethernetII.h>
#include <tins/network_interface.h>
#include <tins/sniffer.h>
#include <tins/utils.h>
#include <tins/packet_sender.h>

using namespace Tins;
using namespace std;

int main() {
  PacketSender sender;
  // Construct a packet which has no link layer protocol.
  IP ip = IP("127.0.0.1") / TCP(8080, 928);
// Here the kernel will figure out which interface to use and it will
// append the appropriate link layer protocol PDU. It will also perform
// the necessary ARP lookups in order to use the destination host's
// hardware address.
//
// libtins will find which is the appropriate source IP address to use.
// This will be done by the kernel as well, but it's required when
// calculating checksums.
  TCP& tcp = ip.rfind_pdu<TCP>();
  // Set the SYN flag on.
  for (int i = 0; i < 10; ++i) {
    tcp.set_flag(TCP::SYN, 1);
    sender.send(ip);
  }

  return 0;
}
