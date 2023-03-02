#include "network_interface.hh"

#include "address.hh"
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include "ethernet_header.hh"
#include "ipv4_datagram.hh"
#include "ipv4_header.hh"
#include "parser.hh"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

[[maybe_unused]] constexpr std::array<unsigned char, 6> broadcast_mac{255, 255, 255, 255, 255, 255};
[[maybe_unused]] constexpr std::array<unsigned char, 6> zero_mac{0, 0, 0, 0, 0, 0};

template <typename T>
std::string u2str(T u) {
    const char *t;
    t = reinterpret_cast<decltype(t)>(&u);
    auto str = std::string{t, t + sizeof(u)};
    std::reverse(str.begin(), str.end());
    return str;
}
auto eth2str(const EthernetAddress adr) {
    std::string str;
    for (auto byte : adr) {
        str.push_back(char(byte));
    }
    // if that address need to be reverse?
    return str;
}
//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    std::ostringstream ostr;
    if (_arp_cache.count(next_hop_ip)) {
        EthernetFrame eth_frame;
        auto des = _arp_cache[next_hop_ip];
        ostr << eth2str(des);
        ostr << eth2str(_ethernet_address);
        ostr << u2str(EthernetHeader::TYPE_IPv4);
        ostr << dgram.serialize().concatenate();

        eth_frame.parse(ostr.str());
        _frames_out.push(eth_frame);
        return;
    }

    if (_last_send_arp_time.count(next_hop_ip) && _current_time - _last_send_arp_time[next_hop_ip] <= 5 * 1000) {
        // i dont want flood the network with arp requests.
        _need_reslove_mac[next_hop_ip].push_back(dgram);
        return;
    }
    EthernetFrame eth_frame;
    ostringstream arp_ostr;

    ostr << eth2str(broadcast_mac);
    ostr << eth2str(_ethernet_address);
    ostr << u2str(EthernetHeader::TYPE_ARP);

    arp_ostr << u2str(ARPMessage::TYPE_ETHERNET);
    arp_ostr << u2str(EthernetHeader::TYPE_IPv4);
    arp_ostr << char(6);
    arp_ostr << char(4);
    arp_ostr << u2str(ARPMessage::OPCODE_REQUEST);

    arp_ostr << eth2str(_ethernet_address);
    arp_ostr << u2str(_ip_address.ipv4_numeric());
    arp_ostr << eth2str(zero_mac);
    arp_ostr << u2str(next_hop_ip);

    ostr << arp_ostr.str();
    eth_frame.parse(ostr.str());
    _frames_out.push(eth_frame);
    _last_send_arp_time[next_hop_ip] = _current_time;
    _need_reslove_mac[next_hop_ip].push_back(dgram);
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    auto eth_type = frame.header().type;
    auto eth_dst = frame.header().dst;
    auto eth_src = frame.header().src;

    if (eth_type == EthernetHeader::TYPE_IPv4 && eth_dst == _ethernet_address) {
        InternetDatagram ipv4_frame;
        auto res = ipv4_frame.parse(frame.payload().concatenate());
        if (res != ParseResult::NoError) {
            std::cerr << "parse ip wrong!\n";
            return {};
        }
        /* if (ipv4_frame.header().dst != _ip_address.ipv4_numeric()) { */
        /*     return {}; */
        /* } */
        return ipv4_frame;
    }

    if (eth_type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_msg;
        auto res = arp_msg.parse(frame.payload().concatenate());
        if (res != ParseResult::NoError) {
            std::cerr << "parse arp wrong!\n";
        }
        _arp_cache[arp_msg.sender_ip_address] = eth_src;
        _arp_cache_time[arp_msg.sender_ip_address] = _current_time;

        if (arp_msg.opcode == ARPMessage::OPCODE_REQUEST && arp_msg.target_ip_address == _ip_address.ipv4_numeric()) {
            // need reply

            EthernetFrame eth_frame;
            ostringstream ostr;
            ostringstream arp_ostr;

            ostr << eth2str(eth_src);
            ostr << eth2str(_ethernet_address);
            ostr << u2str(EthernetHeader::TYPE_ARP);

            arp_ostr << u2str(ARPMessage::TYPE_ETHERNET);
            arp_ostr << u2str(EthernetHeader::TYPE_IPv4);
            arp_ostr << char(6);
            arp_ostr << char(4);
            arp_ostr << u2str(ARPMessage::OPCODE_REPLY);

            arp_ostr << eth2str(_ethernet_address);
            arp_ostr << u2str(_ip_address.ipv4_numeric());
            arp_ostr << eth2str(eth_src);
            arp_ostr << u2str(arp_msg.sender_ip_address);

            ostr << arp_ostr.str();
            eth_frame.parse(ostr.str());
            _frames_out.push(eth_frame);
        }
        for (auto &f : _need_reslove_mac[arp_msg.sender_ip_address]) {
            Address adr = Address::from_ipv4_numeric(arp_msg.sender_ip_address);
            send_datagram(f, adr);
        }
        _last_send_arp_time.erase(arp_msg.sender_ip_address);
        _need_reslove_mac.erase(arp_msg.sender_ip_address);
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _current_time += ms_since_last_tick;
    vector<uint32_t> need_expired;
    for (auto p : _arp_cache_time) {
        auto ip = p.first;
        auto last_time = p.second;
        if (_current_time - last_time > 30 * 1000) {
            need_expired.push_back(ip);
        }
    }
    for (auto ip : need_expired) {
        _arp_cache_time.erase(ip);
        _arp_cache.erase(ip);
    }
}
