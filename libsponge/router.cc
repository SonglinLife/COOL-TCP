#include "router.hh"

#include "address.hh"

#include <cstdint>
#include <iostream>
#include <limits>

using namespace std;

// Dummy implementation of an IP router

// Given an incoming Internet datagram, the router decides
// (1) which interface to send it out on, and
// (2) what next hop address to send it to.

// For Lab 6, please replace with a real implementation that passes the
// automated checks run by `make check_lab6`.

// You will need to add private members to the class declaration in `router.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

//! \param[in] route_prefix The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
//! \param[in] prefix_length For this route to be applicable, how many high-order (most-significant) bits of the route_prefix will need to match the corresponding bits of the datagram's destination address?
//! \param[in] next_hop The IP address of the next hop. Will be empty if the network is directly attached to the router (in which case, the next hop address should be the datagram's final destination).
//! \param[in] interface_num The index of the interface to send the datagram out on.
void Router::add_route(const uint32_t route_prefix,
                       const uint8_t prefix_length,
                       const optional<Address> next_hop,
                       const size_t interface_num) {
    cerr << "DEBUG: adding route " << Address::from_ipv4_numeric(route_prefix).ip() << "/" << int(prefix_length)
         << " => " << (next_hop.has_value() ? next_hop->ip() : "(direct)") << " on interface " << interface_num << "\n";
    constexpr auto raw_mask = std::numeric_limits<uint32_t>::max();
    auto mask = prefix_length == 0 ? 0 : raw_mask << (32 - prefix_length);

    auto entry = RouteEntry(route_prefix & mask, mask, next_hop, interface_num);
    auto best_match_iter = match_router_table(entry._ip);
    if (best_match_iter == _router_table.end()) {
        _router_table.push_back(entry);
        return;
    }
    if (best_match_iter->_mask < entry._mask) {
        _router_table.push_back(entry);
        return;
    }
}

//! \param[in] dgram The datagram to be routed
void Router::route_one_datagram(InternetDatagram &dgram) {
    DUMMY_CODE(dgram);
    // Your code here.
    if (dgram.header().ttl <= 1) {
        return;
    }
    dgram.header().ttl -= 1;
    auto best_match_iter = match_router_table(dgram.header().dst);
    if (best_match_iter == _router_table.end()) {
        return;
    }
    auto interface_num = best_match_iter->_interface_num;
    auto &send_to = interface(interface_num);
    if (best_match_iter->_next_hop.has_value()) {
        send_to.send_datagram(dgram, best_match_iter->_next_hop.value());
    } else {
        send_to.send_datagram(dgram, Address::from_ipv4_numeric(dgram.header().dst));
    }
}

void Router::route() {
    // Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
    auto cnt = 0;
    for (auto &interface : _interfaces) {
        auto &queue = interface.datagrams_out();
        while (not queue.empty()) {
            route_one_datagram(queue.front());
            queue.pop();
        }
        cnt++;
    }
}

RouterTable::const_iterator Router::match_router_table(uint32_t ip) const {
    uint32_t best_match_mask = 0;
    auto best_match_iter = _router_table.end();
    for (auto iter = _router_table.begin(); iter != _router_table.end(); ++iter) {
        auto entry = *iter;
        if (entry.match(ip) && entry._mask >= best_match_mask) {
            best_match_mask = entry._mask;
            best_match_iter = iter;
        }
    }
    return best_match_iter;
}
