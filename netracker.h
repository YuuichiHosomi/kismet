/*
    This file is part of Kismet

    Kismet is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Kismet is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Kismet; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef __NETRACKER_H__
#define __NETRACKER_H__

#include "config.h"

#include <stdio.h>
#include <time.h>
#include <list>
#include <map>
#include <vector>
#include <algorithm>
#include <string>

#include "globalregistry.h"
#include "packetchain.h"
#include "manuf.h"

// Core network tracker hooks to call back into our core tracker
// elements
int kis_80211_netracker_hook(CHAINCALL_PARMS);
int kis_80211_datatracker_hook(CHAINCALL_PARMS);

class Netracker {
public:
	// Forward defs
	class tracked_network;
	class tracked_client;

	// Enums explicitly defined for the ease of client writers
	enum network_type {
		network_ap = 0,
		network_adhoc = 1,
		network_probe = 2,
		network_turbocell = 3,
		network_data = 4,
		network_remove = 256
	};

	// Bitmask encryption type
	enum crypt_type {
		crypt_none = 0,
		crypt_wep = 1,
		crypt_layer3 = 64,
		crypt_decoded = 128,
		crypt_unknown = 256
	};

	enum client_type {
		client_unknown = 0,
		client_fromds = 1,
		client_tods = 2,
		client_interds = 3,
		client_established = 4
	};

	typedef struct ip_data {
		ip_data() {
			ip_addr_block = ip_netmask = ip_gateway = 0;
		}

		uint32_t ip_addr_block;
		uint32_t ip_netmask;
		uint32_t ip_gateway;

		ip_data& operator= (const ip_data& in) {
			ip_addr_block = in.ip_addr_block;
			ip_netmask = in.ip_netmask;
			ip_gateway = in.ip_gateway;

			return *this;
		}
	};

	typedef struct gps_data {
		gps_data() {
			gps_valid = 0;
			// Pick absurd initial values to be clearly out-of-bounds
			min_lat = min_lon = 1024;
			max_lat = max_lon = -1024;
			min_alt = 100000;
			max_alt = -100000;
			min_spd = 100000;
			max_spd = -100000;

			aggregate_lat = aggregate_lon = aggregate_alt = 0;
			aggregate_points = 0;
		}

		int gps_valid;
		double min_lat, min_lon, min_alt, min_spd;
		double max_lat, max_lon, max_alt, max_spd;
		// Aggregate/avg center position
		long double aggregate_lat, aggregate_lon, aggregate_alt;
		long aggregate_points;
	};

	// SNR info
	typedef struct signal_data {
		signal_data() {
			// These all go to 0 since we don't know if it'll be positive or
			// negative
			last_quality = last_signal = last_noise = 0;
			max_quality = max_signal = max_noise = 0;

			peak_lat = peak_lon = peak_alt = 0;

			maxseenrate = 0;
		}

		int last_quality, last_signal, last_noise;
		int max_quality, max_signal, max_noise;
		// Peak locations
		double peak_lat, peak_lon, peak_alt;

		// Max rate
		int maxseenrate;
	};

	class tracked_network {
	public:
		tracked_network();

		// What we last saw it as
		Netracker::network_type type;

		string ssid;
		string beacon_info;

		// Aggregate packet counts
		int llc_packets;
		int data_packets;
		int crypt_packets;
		int fmsweak_packets;

		int channel;

		uint32_t encryption;

		mac_addr bssid;

		// Is the SSID hidden
		int ssid_cloaked;
		// And have we exposed it
		int ssid_uncloaked;

		time_t last_time;
		time_t first_time;

		// Bitmask set of carrier types seen on the network
		uint32_t carrier_set;
		// Bitmask set of encoding types seen on the network
		uint32_t encoding_set;

		// GPS info
		Netracker::gps_data gpsdata;

		// SNR info
		Netracker::signal_data snrdata;

		// Maximum advertised rate
		double maxrate;
		// Maximum seen rate
		double maxseenrate;
		// Beacon interval
		int beaconrate;

		// Guesstimated IP data
		ip_data guess_ipdata;

		// state tracking elements
		// Number of client disconnects (decayed per second)
		int client_disconnects;
		// Last sequence value
		int last_sequence;
		// last BSS timestamp
		uint64_t bss_timestamp;

		// Amount of data seen
		uint64_t datasize;

		// Map of IVs seen (Is this a really bad idea for ram?  Probably.  Consider
		// nuking this if it can't be compressed somehow at runtime.  Or make it a 
		// config variable for people with ram to burn)
		map<uint32_t, int> iv_map;
		// Number of duplicate IV counts
		int dupeiv_packets;

		// Nonoverlapping so standard map is ok
		map<mac_addr, tracked_client *> cli_track_map;
	};

	class tracked_client {
	public:
		tracked_client();

		// DS detected type
		Netracker::client_type type;

		// timestamps
		time_t last_time;
		time_t first_time;

		// MAC of client
		mac_addr mac;

		// Last seen channel
		int channel;

		Netracker::gps_data gpsdata;
		Netracker::signal_data snrdata;

		// Client encryption bitfield
		uint32_t encryption;

		// Individual packet counts
		int llc_packets;
		int data_packets;
		int crypt_packets;
		int fmsweak_packets;

		// Manufacturer info - MAC address key to the manuf map and score
		// for easy mapping
		manuf *manuf_ref;
		int manuf_score;

		// Maximum advertised rate during a probe
		double maxrate;
		// How cast we've been seen to actually go, in 100kb/s units
		int maxseenrate;

		// Bitfield of encoding types seen from this client
		uint32_t encoding_set;

		// Last sequence number seen
		int last_sequence;

		// Amount of data seen
		uint64_t datasize;

		// Guesstimated IP data
		ip_data guess_ipdata;
	};

	Netracker();
	Netracker(GlobalRegistry *in_globalreg);

	typedef map<mac_addr, Netracker::tracked_network *>::iterator track_iter;
	typedef map<mac_addr, Netracker::tracked_client *>::iterator client_iter;

protected:
	GlobalRegistry *globalreg;

	// Actually handle the chain events
	int netracker_chain_handler(kis_packet *in_pack);
	int datatracker_chain_handler(kis_packet *in_pack);

	// All networks
	map<mac_addr, Netracker::tracked_network *> tracked_map;
	// Probe association to network that owns it
	map<mac_addr, Netracker::tracked_network *> probe_assoc_map;
	// All clients
	map<mac_addr, Netracker::tracked_client *> client_map;

	// Cached data
	map<mac_addr, Netracker::ip_data> bssid_ip_map;
	map<mac_addr, string> bssid_cloak_map;

	// Manufacturer maps
	macmap<vector<manuf *> > ap_manuf_map;
	macmap<vector<manuf *> > client_manuf_map;

	// Let the hooks call directly in
	friend int kis_80211_netracker_hook(CHAINCALL_PARMS);
	friend int kis_80211_datatracker_hook(CHAINCALL_PARMS);
};

#endif

