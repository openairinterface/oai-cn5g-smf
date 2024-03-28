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

/*! \file conversions.hpp
 \brief
 \author Sebastien ROUX, Lionel Gauthier
 \company Eurecom
 \email: lionel.gauthier@eurecom.fr
 */

#ifndef FILE_CONVERSIONS_HPP_SEEN
#define FILE_CONVERSIONS_HPP_SEEN
#include <stdint.h>
#include <string>
#include <netinet/in.h>
#include <vector>

#include "3gpp_23.003.h"

/* Used to format an uint32_t containing an ipv4 address */
#define IN_ADDR_FMT "%u.%u.%u.%u"
#define PRI_IN_ADDR(aDDRESS)                                                   \
  (uint8_t)((aDDRESS.s_addr) & 0x000000ff),                                    \
      (uint8_t) (((aDDRESS.s_addr) & 0x0000ff00) >> 8),                        \
      (uint8_t) (((aDDRESS.s_addr) & 0x00ff0000) >> 16),                       \
      (uint8_t) (((aDDRESS.s_addr) & 0xff000000) >> 24)

#define IPV4_ADDR_DISPLAY_8(aDDRESS)                                           \
  (aDDRESS)[0], (aDDRESS)[1], (aDDRESS)[2], (aDDRESS)[3]

/* Convert an integer on 32 bits to the given bUFFER */
#define INT32_TO_BUFFER(x, buf)                                                \
  (buf)[0] = (x) >> 24, (buf)[1] = (x) >> 16, (buf)[2] = (x) >> 8,             \
  (buf)[3] = (x)

#define INT64_TO_BUFFER(x, buf)                                                \
  (buf)[0] = (x) >> 56, (buf)[1] = (x) >> 48, (buf)[2] = (x) >> 40,            \
  (buf)[3] = (x) >> 32, (buf)[4] = (x) >> 24, (buf)[5] = (x) >> 16,            \
  (buf)[6] = (x) >> 8, (buf)[7] = (x)

class conv {
 public:
  static void hexa_to_ascii(uint8_t* from, char* to, size_t length);
  static int ascii_to_hex(uint8_t* dst, const char* h);
  static struct in_addr fromString(const std::string& addr4);
  static struct in6_addr fromStringV6(const std::string& addr6);
  static bool plmnFromString(
      plmn_t& p, const std::string mcc, const std::string mnc);
  static void plmnToMccMnc(
      const plmn_t& plmn, std::string& mcc, std::string& mnc);
  static std::string toString(const struct in_addr& inaddr);
  static std::string toString(const struct in6_addr& in6addr);
  static std::string mccToString(
      const uint8_t digit1, const uint8_t digit2, const uint8_t digit3);
  static std::string mncToString(
      const uint8_t digit1, const uint8_t digit2, const uint8_t digit3);

  /*
   * Convert a string to hex representing this string
   * @param [const std::string&] input_str Input string
   * @param [std::string&] output_str String represents string in hex format
   * @return void
   */
  static void convert_string_2_hex(
      const std::string& input_str, std::string& output_str);
};

namespace oai::utils::conversions {

struct port_range {
  bool use_port_range = false;
  bool is_range =
      false;  // if range is false, only start should be used and set
  uint16_t start = 0;
  uint16_t end   = UINT16_MAX;

  static port_range from_string(const std::string& port_string);
};

struct ip_range {
  bool use_ip_range = false;
  in_addr ip_addr{};
  in_addr snm{};

  static ip_range from_string(const std::string& ip_string);
};

struct sdf_filter {
  bool default_filter          = true;
  bool use_protocol_identifier = false;
  uint8_t protocol_identifier  = 0;
  // as I understood the spec, there is only one IP range (not like for ports)
  ip_range src_ip_range;
  std::vector<port_range> src_port_ranges;
  int filter_components = 0;
  // TODO there are some more things in RFC 6733 but this should cover most
  // cases

  static sdf_filter from_string(const std::string& filter_string);
};

// DO NOT CHANGE the int values of the enum, they are used
// Note: 3GPP also allows bit/s, but it is neither used in NAS nor in PFCP so we
// ignore it
enum class bitrate_unit_e {
  KBPS     = 1,
  MBPS     = 2,
  GBPS     = 3,
  TBPS     = 4,
  PBPS     = 5,
  _256PBPS = 6  // to support maximum NAS value
};

/**
 * Parses 3GPP 29.571 BitRate string into value and unit
 * @param bitrate input: bitrate string
 * @param value output: bitrate value
 * @param unit output: bitrate unit
 * @return true if parsing is successful
 */
bool parse_bitrate_string(
    const std::string& bitrate, uint16_t& value, bitrate_unit_e& unit);

}  // namespace oai::utils::conversions

#endif /* FILE_CONVERSIONS_HPP_SEEN */