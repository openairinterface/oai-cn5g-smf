/*
 * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The OpenAirInterface Software Alliance licenses this file to You under
 * the OAI Public License, Version 1.1  (the "License"); you may not use this
 * file except in compliance with the License. You may obtain a copy of the
 * License at
 *
 *      http://www.openairinterface.org/?page_id=698
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------------------
 * For more information about the OpenAirInterface (OAI) Software Alliance:
 *      contact@openairinterface.org
 */

/*! \file conversions.cpp
 \brief
 \author Sebastien ROUX
 \company Eurecom
 */
#include "conversions.hpp"

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include "logger.hpp"

#include <regex>
#include <fmt/format.h>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/classification.hpp>

extern "C" {
#include "dynamic_memory_check.h"
}

static const char hex_to_ascii_table[16] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
};

static const signed char ascii_to_hex_table[0x100] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,
    9,  -1, -1, -1, -1, -1, -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1};

void conv::hexa_to_ascii(uint8_t* from, char* to, size_t length) {
  size_t i;

  for (i = 0; i < length; i++) {
    uint8_t upper = (from[i] & 0xf0) >> 4;
    uint8_t lower = from[i] & 0x0f;

    to[2 * i]     = hex_to_ascii_table[upper];
    to[2 * i + 1] = hex_to_ascii_table[lower];
  }
}

int conv::ascii_to_hex(uint8_t* dst, const char* h) {
  const unsigned char* hex = (const unsigned char*) h;
  unsigned i               = 0;

  for (;;) {
    int high, low;

    while (*hex && isspace(*hex)) hex++;

    if (!*hex) return 1;

    high = ascii_to_hex_table[*hex++];

    if (high < 0) return 0;

    while (*hex && isspace(*hex)) hex++;

    if (!*hex) return 0;

    low = ascii_to_hex_table[*hex++];

    if (low < 0) return 0;

    dst[i++] = (high << 4) | low;
  }
}

//------------------------------------------------------------------------------
std::string conv::mccToString(
    const uint8_t digit1, const uint8_t digit2, const uint8_t digit3) {
  std::string s  = {};
  uint16_t mcc16 = digit1 * 100 + digit2 * 10 + digit3;
  // s.append(std::to_string(digit1)).append(std::to_string(digit2)).append(std::to_string(digit3));
  s.append(std::to_string(mcc16));
  return s;
}

//------------------------------------------------------------------------------
std::string conv::mncToString(
    const uint8_t digit1, const uint8_t digit2, const uint8_t digit3) {
  std::string s  = {};
  uint16_t mcc16 = 0;

  if (digit3 == 0x0F) {
    mcc16 = digit1 * 10 + digit2;
  } else {
    mcc16 = digit1 * 100 + digit2 * 10 + digit3;
  }
  s.append(std::to_string(mcc16));
  return s;
}

//------------------------------------------------------------------------------
bool conv::plmnFromString(
    plmn_t& p, const std::string mcc, const std::string mnc) {
  // MCC
  if (isdigit(mcc[0]))
    p.mcc_digit1 = mcc[0] - '0';
  else
    return false;
  if (isdigit(mcc[1]))
    p.mcc_digit2 = mcc[1] - '0';
  else
    return false;

  if (isdigit(mcc[2]))
    p.mcc_digit3 = mcc[2] - '0';
  else
    return false;

  // MNC
  if (isdigit(mnc[0]))
    p.mnc_digit1 = mnc[0] - '0';
  else
    return false;
  if (isdigit(mnc[1]))
    p.mnc_digit2 = mnc[1] - '0';
  else
    return false;
  if (mnc.length() > 2) {
    if (isdigit(mnc[2]))
      p.mnc_digit3 = mnc[2] - '0';
    else
      return false;
  } else {
    p.mnc_digit3 = 0x0;
  }
  return true;
}

//------------------------------------------------------------------------------
void conv::plmnToMccMnc(
    const plmn_t& plmn, std::string& mcc, std::string& mnc) {
  int m_mcc = plmn.mcc_digit1 * 100 + plmn.mcc_digit2 * 10 + plmn.mcc_digit3;
  mcc       = std::to_string(m_mcc);
  if ((plmn.mcc_digit2 == 0) and (plmn.mcc_digit1 == 0)) {
    mcc = "00" + mcc;
  } else if (plmn.mcc_digit1 == 0) {
    mcc = "0" + mcc;
  }

  int m_mnc = 0;
  if (plmn.mnc_digit3 == 0xf) {
    m_mnc = plmn.mnc_digit1 * 10 + plmn.mnc_digit2;
    if (plmn.mnc_digit1 == 0) {
      mnc = "0" + std::to_string(m_mnc);
      return;
    }
  } else {
    m_mnc = plmn.mnc_digit3 * 100 + plmn.mnc_digit1 * 10 + plmn.mnc_digit2;
    mnc   = std::to_string(m_mnc);
    if ((plmn.mnc_digit2 == 0) and (plmn.mnc_digit1 == 0)) {
      mnc = "00" + mnc;
    } else if (plmn.mnc_digit1 == 0) {
      mnc = "0" + mnc;
    }
  }

  return;
}

//------------------------------------------------------------------------------
struct in_addr conv::fromString(const std::string& addr4) {
  unsigned char buf[sizeof(struct in6_addr)] = {};
  struct in_addr ipv4_addr;
  ipv4_addr.s_addr = INADDR_ANY;
  if (inet_pton(AF_INET, addr4.c_str(), buf) == 1) {
    memcpy(&ipv4_addr, buf, sizeof(struct in_addr));
  }
  return ipv4_addr;
}

struct in6_addr conv::fromStringV6(const std::string& addr6) {
  unsigned char buf[sizeof(struct in6_addr)] = {};
  struct in6_addr ipv6_addr {};
  if (inet_pton(AF_INET6, addr6.c_str(), buf) == 1) {
    memcpy(&ipv6_addr, buf, sizeof(struct in6_addr));
  }
  return ipv6_addr;
}

//------------------------------------------------------------------------------
std::string conv::toString(const struct in_addr& inaddr) {
  std::string s              = {};
  char str[INET6_ADDRSTRLEN] = {};
  if (inet_ntop(AF_INET, (const void*) &inaddr, str, INET6_ADDRSTRLEN) ==
      NULL) {
    s.append("");
  } else {
    s.append(str);
  }
  return s;
}
//------------------------------------------------------------------------------
std::string conv::toString(const struct in6_addr& in6addr) {
  std::string s              = {};
  char str[INET6_ADDRSTRLEN] = {};
  if (inet_ntop(AF_INET6, (const void*) &in6addr, str, INET6_ADDRSTRLEN) ==
      nullptr) {
    s.append("");
  } else {
    s.append(str);
  }
  return s;
}

//---------------------------------------------------------------------------------------------
void conv::convert_string_2_hex(
    const std::string& input_str, std::string& output_str) {
  Logger::smf_app().debug("Convert string to Hex");
  unsigned char* data = (unsigned char*) malloc(input_str.length() + 1);
  memset(data, 0, input_str.length() + 1);
  memcpy((void*) data, (void*) input_str.c_str(), input_str.length());

  Logger::smf_app().debug("Input: ");
  if (Logger::should_log(spdlog::level::debug)) {
    for (int i = 0; i < input_str.length(); i++) {
      printf("%02x ", data[i]);
    }
    printf("\n");
  }

  char* datahex = (char*) malloc(input_str.length() * 2 + 1);
  memset(datahex, 0, input_str.length() * 2 + 1);

  for (int i = 0; i < input_str.length(); i++)
    sprintf(datahex + i * 2, "%02x", data[i]);

  output_str = reinterpret_cast<char*>(datahex);
  Logger::smf_app().debug("Output: \n %s ", output_str.c_str());

  // free memory
  free_wrapper((void**) &data);
  free_wrapper((void**) &datahex);
}

using namespace oai::utils::conversions;

sdf_filter sdf_filter::from_string(const std::string& filter_string) {
  sdf_filter filter;
  // according to 29.212, we may also need to encode the destination IP
  // accordingly (instead of assigned)

  // example for parsing: permit out ip from 1.2.3.4/24 80,433-500 to assigned

  std::string regex = "permit out (\\S*) from (\\S*) ?(.*)? to assigned";
  //                               proto        src   ports

  std::regex re(regex);
  std::smatch matches;
  if (!std::regex_match(filter_string, matches, re)) {
    Logger::smf_app().error(
        "SDF Filter %s cannot be parsed, does not follow the "
        "specification, use default filter",
        filter_string);
    return filter;
  }
  std::string proto = matches[1];
  if (!proto.empty() && proto != "ip") {
    filter.protocol_identifier     = std::stoi(proto);
    filter.use_protocol_identifier = true;
    filter.default_filter          = false;
    filter.filter_components++;
  }

  std::string src_ip = matches[2];
  if (!src_ip.empty() && src_ip != "any") {
    filter.src_ip_range   = ip_range::from_string(src_ip);
    filter.default_filter = false;
    filter.filter_components++;
  }

  std::string src_ports = matches[3];
  if (!src_ports.empty()) {
    std::vector<std::string> splits;
    boost::split(
        splits, src_ports, boost::is_any_of(","), boost::token_compress_on);
    for (auto& split : splits) {
      boost::trim(split);
      port_range range = port_range::from_string(split);
      if (range.use_port_range) {
        filter.src_port_ranges.push_back(range);
        filter.filter_components++;
      }
    }
  }
  return filter;
}

port_range port_range::from_string(const std::string& port_string) {
  port_range range;

  std::vector<std::string> splits;
  boost::split(
      splits, port_string, boost::is_any_of("-"), boost::token_compress_on);
  boost::trim(splits[0]);
  range.start = std::stoi(splits[0]);
  if (splits.size() > 1) {
    boost::trim(splits[1]);
    range.end      = std::stoi(splits[1]);
    range.is_range = true;
  }
  range.use_port_range = true;

  return range;
}

ip_range ip_range::from_string(const std::string& ip_string) {
  ip_range range;
  std::vector<std::string> splits;
  boost::split(
      splits, ip_string, boost::is_any_of("/"), boost::token_compress_on);

  if (splits[0] == "any") {
    range.use_ip_range = false;
    return range;
  }
  range.use_ip_range = true;
  in_addr ip_addr    = conv::fromString(splits[0]);
  if (splits.size() == 1) {
    // there is no SNM, so we take 255.255.255.255
    range.snm.s_addr = 0xffffffff;
    range.ip_addr    = ip_addr;
  } else {
    uint8_t snm       = std::stoi(splits[1]);
    uint8_t left_bits = 32 - snm;
    range.snm.s_addr  = ntohl(0xffffffff << left_bits);
  }
  return range;
}

bool oai::utils::conversions::parse_bitrate_string(
    const std::string& bitrate, uint16_t& value, bitrate_unit_e& unit) {
  // TODO update BANDWIDTH_REGEX in model/Helpers.h with this value, the only
  // difference is capture groups after resynch with common-src
  // Here, the space is optional, so "100Mbps" is okay and "100 Mbps", but in
  // the standard the space is mandatory
  std::string bandwidth_regex = R"((^\d+(\.\d+)?) ?(bps|Kbps|Mbps|Gbps|Tbps)$)";

  std::regex re(bandwidth_regex);
  std::smatch matches;
  if (!std::regex_match(bitrate, matches, re)) {
    Logger::smf_app().error(
        "Bitrate %s cannot be parsed, does not follow the specification",
        bitrate);
    return false;
  }

  std::string string_bw_value = matches[1];
  // matches[2] is the fractional part but that is included in matches[1]
  // already
  std::string string_unit = matches[3];

  try {
    double bw_value = std::stod(string_bw_value);

    if (string_unit == "bps") {
      unit     = bitrate_unit_e::KBPS;
      bw_value = bw_value / 1000;
    } else if (string_unit == "Kbps") {
      unit = bitrate_unit_e::KBPS;
    } else if (string_unit == "Mbps") {
      unit = bitrate_unit_e::MBPS;
    } else if (string_unit == "Gbps") {
      unit = bitrate_unit_e::GBPS;
    } else if (string_unit == "Tbps") {
      unit = bitrate_unit_e::TBPS;
    }

    // a bit hacky, but like this we can use arithmetic to calculate the unit
    int bitrate_int = static_cast<std::underlying_type_t<bitrate_unit_e>>(unit);

    // Here we convert up so that there is enough space in the int buffer
    while (bw_value > UINT16_MAX) {
      if (bitrate_int == 6) {
        Logger::smf_app().warn(
            "Bitrate cannot be higher than %d x 256 PBPS", bw_value);
      }
      if (bitrate_int == 5) {
        bw_value = bw_value / 256;
      } else {
        bw_value = bw_value / 1000;
      }
      bitrate_int++;
    }

    // Here we have to handle the fractional part, as 3GPP also allows that
    // we check if bw_value has fractional parts
    while (long(bw_value) != bw_value) {
      if (bitrate_int == 1 || (long(bw_value / 1000) > (UINT16_MAX / 1000))) {
        // we possibly cant make it smaller, so we just cut it off
        break;
      }
      bw_value = bw_value * 1000;
      bitrate_int--;
    }

    value = long(bw_value);
    unit  = static_cast<bitrate_unit_e>(bitrate_int);
    return true;
  } catch (std::invalid_argument&) {
    Logger::smf_app().error(
        "Bitrate value part %s is not a number, cannot parse.",
        string_bw_value);
    return false;
  }
}
