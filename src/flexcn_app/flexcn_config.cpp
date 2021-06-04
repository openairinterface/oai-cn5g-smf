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

/*! \file flexcn_config.cpp
 \brief
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#include "flexcn_config.hpp"

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include "string.hpp" //common/utils

// C includes
#include <arpa/inet.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include "common_defs.h"
// #include "epc.h" //common/utils
#include "if.hpp" //common/utils
#include "logger.hpp"
#include "flexcn_app.hpp"

using namespace std;
using namespace libconfig;
using namespace flexcn;

extern flexcn_config flexcn_cfg;

//------------------------------------------------------------------------------
int flexcn_config::finalize() {
  Logger::flexcn_app().info("Finalize config...");
  Logger::flexcn_app().info("Finalized config");
  return 0;
}

//------------------------------------------------------------------------------
int flexcn_config::load_thread_sched_params(
    const Setting& thread_sched_params_cfg, util::thread_sched_params& cfg) {
  try {
    thread_sched_params_cfg.lookupValue(
        FLEXCN_CONFIG_STRING_THREAD_RD_CPU_ID, cfg.cpu_id);
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  try {
    std::string thread_rd_sched_policy;
    thread_sched_params_cfg.lookupValue(
        FLEXCN_CONFIG_STRING_THREAD_RD_SCHED_POLICY, thread_rd_sched_policy);
    util::trim(thread_rd_sched_policy);
    if (boost::iequals(thread_rd_sched_policy, "SCHED_OTHER")) {
      cfg.sched_policy = SCHED_OTHER;
    } else if (boost::iequals(thread_rd_sched_policy, "SCHED_IDLE")) {
      cfg.sched_policy = SCHED_IDLE;
    } else if (boost::iequals(thread_rd_sched_policy, "SCHED_BATCH")) {
      cfg.sched_policy = SCHED_BATCH;
    } else if (boost::iequals(thread_rd_sched_policy, "SCHED_FIFO")) {
      cfg.sched_policy = SCHED_FIFO;
    } else if (boost::iequals(thread_rd_sched_policy, "SCHED_RR")) {
      cfg.sched_policy = SCHED_RR;
    } else {
      Logger::flexcn_app().error(
          "thread_rd_sched_policy: %s, unknown in config file",
          thread_rd_sched_policy.c_str());
      return RETURNerror;
    }
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  try {
    thread_sched_params_cfg.lookupValue(
        FLEXCN_CONFIG_STRING_THREAD_RD_SCHED_PRIORITY, cfg.sched_priority);
    if ((cfg.sched_priority > 99) || (cfg.sched_priority < 1)) {
      Logger::flexcn_app().error(
          "thread_rd_sched_priority: %d, must be in interval [1..99] in config "
          "file",
          cfg.sched_priority);
      return RETURNerror;
    }
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }
  return RETURNok;
}
//------------------------------------------------------------------------------
int flexcn_config::load_itti(const Setting& itti_cfg, itti_cfg_t& cfg) {
  try {
    const Setting& itti_timer_sched_params_cfg =
        itti_cfg[FLEXCN_CONFIG_STRING_ITTI_TIMER_SCHED_PARAMS];
    load_thread_sched_params(
        itti_timer_sched_params_cfg, cfg.itti_timer_sched_params);
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  try {
    const Setting& n4_sched_params_cfg =
        itti_cfg[FLEXCN_CONFIG_STRING_N4_SCHED_PARAMS];
    load_thread_sched_params(n4_sched_params_cfg, cfg.n4_sched_params);
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  try {
    const Setting& flexcn_app_sched_params_cfg =
        itti_cfg[FLEXCN_CONFIG_STRING_FLEXCN_APP_SCHED_PARAMS];
    load_thread_sched_params(
        flexcn_app_sched_params_cfg, cfg.flexcn_app_sched_params);
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  try {
    const Setting& async_cmd_sched_params_cfg =
        itti_cfg[FLEXCN_CONFIG_STRING_ASYNC_CMD_SCHED_PARAMS];
    load_thread_sched_params(
        async_cmd_sched_params_cfg, cfg.async_cmd_sched_params);
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  return RETURNok;
}

std::string dnn_label(const std::string& dnn) {
  std::string dnn_label = {};
  bool to_count         = true;
  uint8_t counted       = 0;
  int index             = 0;

  dnn_label.push_back('?');

  for (int i = 0; i < dnn.length(); ++i) {
    if (isalnum(dnn[i]) || (dnn[i] == '-')) {
      dnn_label.push_back(dnn[i]);
      counted++;
    } else if (dnn[i] == '.') {
      dnn_label.push_back('?');
      if (to_count) {  // always true
        dnn_label[index] = counted;
      }
      to_count = true;
      counted  = 0;
      index    = dnn_label.length() - 1;
    }
  }
  if (to_count) {
    dnn_label[index] = counted;
  }
  return dnn_label;
}

//------------------------------------------------------------------------------
int flexcn_config::load_interface(const Setting& if_cfg, interface_cfg_t& cfg) {
  if_cfg.lookupValue(FLEXCN_CONFIG_STRING_INTERFACE_NAME, cfg.if_name);
  util::trim(cfg.if_name);
  if (not boost::iequals(cfg.if_name, "none")) {
    std::string address = {};
    if_cfg.lookupValue(FLEXCN_CONFIG_STRING_IPV4_ADDRESS, address);
    util::trim(address);
    if (boost::iequals(address, "read")) {
      if (get_inet_addr_infos_from_iface(
              cfg.if_name, cfg.addr4, cfg.network4, cfg.mtu)) {
        Logger::flexcn_app().error(
            "Could not read %s network interface configuration", cfg.if_name);
        return RETURNerror;
      }
    } else {
      std::vector<std::string> words;
      boost::split(
          words, address, boost::is_any_of("/"), boost::token_compress_on);
      if (words.size() != 2) {
        Logger::flexcn_app().error(
            "Bad value " FLEXCN_CONFIG_STRING_IPV4_ADDRESS " = %s in config file",
            address.c_str());
        return RETURNerror;
      }
      unsigned char buf_in_addr[sizeof(struct in6_addr)];  // you never know...
      if (inet_pton(AF_INET, util::trim(words.at(0)).c_str(), buf_in_addr) ==
          1) {
        memcpy(&cfg.addr4, buf_in_addr, sizeof(struct in_addr));
      } else {
        Logger::flexcn_app().error(
            "In conversion: Bad value " FLEXCN_CONFIG_STRING_IPV4_ADDRESS
            " = %s in config file",
            util::trim(words.at(0)).c_str());
        return RETURNerror;
      }
      cfg.network4.s_addr = htons(
          ntohs(cfg.addr4.s_addr) &
          0xFFFFFFFF << (32 - std::stoi(util::trim(words.at(1)))));
    }
    if_cfg.lookupValue(FLEXCN_CONFIG_STRING_PORT, cfg.port);

    try {
      const Setting& sched_params_cfg = if_cfg[FLEXCN_CONFIG_STRING_SCHED_PARAMS];
      load_thread_sched_params(sched_params_cfg, cfg.thread_rd_sched_params);
    } catch (const SettingNotFoundException& nfex) {
      Logger::flexcn_app().info(
          "%s : %s, using defaults", nfex.what(), nfex.getPath());
    }
  }
  return RETURNok;
}

//------------------------------------------------------------------------------
int flexcn_config::load(const string& config_file) {
  Config cfg;
  unsigned char buf_in6_addr[sizeof(struct in6_addr)];

  // Read the file. If there is an error, report it and exit.
  try {
    cfg.readFile(config_file.c_str());
  } catch (const FileIOException& fioex) {
    Logger::flexcn_app().error(
        "I/O error while reading file %s - %s", config_file.c_str(),
        fioex.what());
    throw;
  } catch (const ParseException& pex) {
    Logger::flexcn_app().error(
        "Parse error at %s:%d - %s", pex.getFile(), pex.getLine(),
        pex.getError());
    throw;
  }

  const Setting& root = cfg.getRoot();

  try {
    const Setting& flexcn_cfg = root[FLEXCN_CONFIG_STRING_FLEXCN_CONFIG];
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().error("%s : %s", nfex.what(), nfex.getPath());
    return RETURNerror;
  }

  const Setting& flexcn_cfg = root[FLEXCN_CONFIG_STRING_FLEXCN_CONFIG];

  try {
    flexcn_cfg.lookupValue(FLEXCN_CONFIG_STRING_INSTANCE, instance);
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  try {
    flexcn_cfg.lookupValue(FLEXCN_CONFIG_STRING_PID_DIRECTORY, pid_dir);
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  try {
    const Setting& itti_cfg = flexcn_cfg[FLEXCN_CONFIG_STRING_ITTI_TASKS];
    load_itti(itti_cfg, itti);
  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().info(
        "%s : %s, using defaults", nfex.what(), nfex.getPath());
  }

  try {
    const Setting& nw_if_cfg = flexcn_cfg[FLEXCN_CONFIG_STRING_INTERFACES];

    const Setting& n4_cfg = nw_if_cfg[FLEXCN_CONFIG_STRING_INTERFACE_N4];
    load_interface(n4_cfg, n4);

    const Setting& sbi_cfg = nw_if_cfg[FLEXCN_CONFIG_STRING_INTERFACE_SBI];
    load_interface(sbi_cfg, sbi);

    // HTTP2 port
    if (!(sbi_cfg.lookupValue(
            FLEXCN_CONFIG_STRING_SBI_HTTP2_PORT, sbi_http2_port))) {
      Logger::flexcn_app().error(FLEXCN_CONFIG_STRING_SBI_HTTP2_PORT "failed");
      throw(FLEXCN_CONFIG_STRING_SBI_HTTP2_PORT "failed");
    }

    // SBI API VERSION
    if (!(sbi_cfg.lookupValue(
            FLEXCN_CONFIG_STRING_API_VERSION, sbi_api_version))) {
      Logger::flexcn_app().error(FLEXCN_CONFIG_STRING_API_VERSION "failed");
      throw(FLEXCN_CONFIG_STRING_API_VERSION "failed");
    }

  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().error("%s : %s", nfex.what(), nfex.getPath());
    return RETURNerror;
  }

  try {
    string astring;

    flexcn_cfg.lookupValue(FLEXCN_CONFIG_STRING_DEFAULT_DNS_IPV4_ADDRESS, astring);
    IPV4_STR_ADDR_TO_INADDR(
        util::trim(astring).c_str(), default_dnsv4,
        "BAD IPv4 ADDRESS FORMAT FOR DEFAULT DNS !");

    flexcn_cfg.lookupValue(
        FLEXCN_CONFIG_STRING_DEFAULT_DNS_SEC_IPV4_ADDRESS, astring);
    IPV4_STR_ADDR_TO_INADDR(
        util::trim(astring).c_str(), default_dns_secv4,
        "BAD IPv4 ADDRESS FORMAT FOR DEFAULT DNS !");

    flexcn_cfg.lookupValue(FLEXCN_CONFIG_STRING_DEFAULT_DNS_IPV6_ADDRESS, astring);
    if (inet_pton(AF_INET6, util::trim(astring).c_str(), buf_in6_addr) == 1) {
      memcpy(&default_dnsv6, buf_in6_addr, sizeof(struct in6_addr));
    } else {
      Logger::flexcn_app().error(
          "CONFIG : BAD ADDRESS in " FLEXCN_CONFIG_STRING_DEFAULT_DNS_IPV6_ADDRESS
          " %s",
          astring.c_str());
      throw(
          "CONFIG : BAD ADDRESS in " FLEXCN_CONFIG_STRING_DEFAULT_DNS_IPV6_ADDRESS
          " %s",
          astring.c_str());
    }
    flexcn_cfg.lookupValue(
        FLEXCN_CONFIG_STRING_DEFAULT_DNS_SEC_IPV6_ADDRESS, astring);
    if (inet_pton(AF_INET6, util::trim(astring).c_str(), buf_in6_addr) == 1) {
      memcpy(&default_dns_secv6, buf_in6_addr, sizeof(struct in6_addr));
    } else {
      Logger::flexcn_app().error(
          "CONFIG : BAD ADDRESS "
          "in " FLEXCN_CONFIG_STRING_DEFAULT_DNS_SEC_IPV6_ADDRESS " %s",
          astring.c_str());
      throw(
          "CONFIG : BAD ADDRESS "
          "in " FLEXCN_CONFIG_STRING_DEFAULT_DNS_SEC_IPV6_ADDRESS " %s",
          astring.c_str());
    }
    
    // Support features
    try {
      const Setting& support_features =
          flexcn_cfg[FLEXCN_CONFIG_STRING_SUPPORT_FEATURES];
      string opt;
      support_features.lookupValue(
          FLEXCN_CONFIG_STRING_SUPPORT_FEATURES_REGISTER_NRF, opt);
      if (boost::iequals(opt, "yes")) {
        register_nrf = true;
      } else {
        register_nrf = false;
      }

      support_features.lookupValue(
          FLEXCN_CONFIG_STRING_SUPPORT_FEATURES_DISCOVER_UPF, opt);
      if (boost::iequals(opt, "yes")) {
        discover_upf = true;
      } else {
        discover_upf = false;
      }
      

    } catch (const SettingNotFoundException& nfex) {
      Logger::flexcn_app().error(
          "%s : %s, using defaults", nfex.what(), nfex.getPath());
      return -1;
    }

    // AMF
    const Setting& amf_cfg = flexcn_cfg[FLEXCN_CONFIG_STRING_AMF];
    struct in_addr amf_ipv4_addr;
    unsigned int amf_port = 0;
    std::string amf_api_version;
    amf_cfg.lookupValue(FLEXCN_CONFIG_STRING_AMF_IPV4_ADDRESS, astring);
    IPV4_STR_ADDR_TO_INADDR(
        util::trim(astring).c_str(), amf_ipv4_addr,
        "BAD IPv4 ADDRESS FORMAT FOR AMF !");
    amf_addr.ipv4_addr = amf_ipv4_addr;
    if (!(amf_cfg.lookupValue(FLEXCN_CONFIG_STRING_AMF_PORT, amf_port))) {
      Logger::flexcn_app().error(FLEXCN_CONFIG_STRING_AMF_PORT "failed");
      throw(FLEXCN_CONFIG_STRING_AMF_PORT "failed");
    }
    amf_addr.port = amf_port;

    if (!(amf_cfg.lookupValue(
            FLEXCN_CONFIG_STRING_API_VERSION, amf_api_version))) {
      Logger::flexcn_app().error(FLEXCN_CONFIG_STRING_API_VERSION "failed");
      throw(FLEXCN_CONFIG_STRING_API_VERSION "failed");
    }
    amf_addr.api_version = amf_api_version;

    // UDM
    const Setting& udm_cfg = flexcn_cfg[FLEXCN_CONFIG_STRING_UDM];
    struct in_addr udm_ipv4_addr;
    unsigned int udm_port = 0;
    std::string udm_api_version;
    udm_cfg.lookupValue(FLEXCN_CONFIG_STRING_UDM_IPV4_ADDRESS, astring);
    IPV4_STR_ADDR_TO_INADDR(
        util::trim(astring).c_str(), udm_ipv4_addr,
        "BAD IPv4 ADDRESS FORMAT FOR UDM !");
    udm_addr.ipv4_addr = udm_ipv4_addr;
    if (!(udm_cfg.lookupValue(FLEXCN_CONFIG_STRING_UDM_PORT, udm_port))) {
      Logger::flexcn_app().error(FLEXCN_CONFIG_STRING_UDM_PORT "failed");
      throw(FLEXCN_CONFIG_STRING_UDM_PORT "failed");
    }
    udm_addr.port = udm_port;

    if (!(udm_cfg.lookupValue(
            FLEXCN_CONFIG_STRING_API_VERSION, udm_api_version))) {
      Logger::flexcn_app().error(FLEXCN_CONFIG_STRING_API_VERSION "failed");
      throw(FLEXCN_CONFIG_STRING_API_VERSION "failed");
    }
    udm_addr.api_version = udm_api_version;

    // UPF list
    unsigned char buf_in_addr[sizeof(struct in_addr) + 1];
    const Setting& upf_list_cfg = flexcn_cfg[FLEXCN_CONFIG_STRING_UPF_LIST];
    int count                       = upf_list_cfg.getLength();
    for (int i = 0; i < count; i++) {
      const Setting& upf_cfg = upf_list_cfg[i];

      string address = {};
      if (upf_cfg.lookupValue(FLEXCN_CONFIG_STRING_UPF_IPV4_ADDRESS, address)) {
        pfcp::node_id_t n = {};
        n.node_id_type    = pfcp::NODE_ID_TYPE_IPV4_ADDRESS;  // actually
        if (inet_pton(AF_INET, util::trim(address).c_str(), buf_in_addr) == 1) {
          memcpy(&n.u1.ipv4_address, buf_in_addr, sizeof(struct in_addr));
        } else {
          Logger::flexcn_app().error(
              "CONFIG: BAD IPV4 ADDRESS in " FLEXCN_CONFIG_STRING_UPF_LIST
              " item %d",
              i);
          throw("CONFIG: BAD ADDRESS in " FLEXCN_CONFIG_STRING_UPF_LIST);
        }
        upfs.push_back(n);
      } else {
        // TODO IPV6_ADDRESS, FQDN
        throw(
            "Bad value in section %s : item no %d in config file %s",
            FLEXCN_CONFIG_STRING_UPF_LIST, i, config_file.c_str());
      }
    }

    // NRF
    const Setting& nrf_cfg = flexcn_cfg[FLEXCN_CONFIG_STRING_NRF];
    struct in_addr nrf_ipv4_addr;
    unsigned int nrf_port = 0;
    std::string nrf_api_version;
    nrf_cfg.lookupValue(FLEXCN_CONFIG_STRING_NRF_IPV4_ADDRESS, astring);
    IPV4_STR_ADDR_TO_INADDR(
        util::trim(astring).c_str(), nrf_ipv4_addr,
        "BAD IPv4 ADDRESS FORMAT FOR NRF !");
    nrf_addr.ipv4_addr = nrf_ipv4_addr;
    if (!(nrf_cfg.lookupValue(FLEXCN_CONFIG_STRING_NRF_PORT, nrf_port))) {
      Logger::flexcn_app().error(FLEXCN_CONFIG_STRING_NRF_PORT "failed");
      throw(FLEXCN_CONFIG_STRING_NRF_PORT "failed");
    }
    nrf_addr.port = nrf_port;

    if (!(nrf_cfg.lookupValue(
            FLEXCN_CONFIG_STRING_API_VERSION, nrf_api_version))) {
      Logger::flexcn_app().error(FLEXCN_CONFIG_STRING_API_VERSION "failed");
      throw(FLEXCN_CONFIG_STRING_API_VERSION "failed");
    }
    nrf_addr.api_version = nrf_api_version;

  } catch (const SettingNotFoundException& nfex) {
    Logger::flexcn_app().error("%s : %s", nfex.what(), nfex.getPath());
    return RETURNerror;
  }
  return finalize();
}

//------------------------------------------------------------------------------
void flexcn_config::display() {
  Logger::flexcn_app().info(
      "==== EURECOM %s v%s ====", PACKAGE_NAME, PACKAGE_VERSION);
  Logger::flexcn_app().info("Configuration FLEXCN:");
  Logger::flexcn_app().info("- Instance ..............: %d\n", instance);
  Logger::flexcn_app().info("- PID dir ...............: %s\n", pid_dir.c_str());

  Logger::flexcn_app().info("- SBI Networking:");
  Logger::flexcn_app().info("    Interface name ......: %s", sbi.if_name.c_str());
  Logger::flexcn_app().info("    IPv4 Addr ...........: %s", inet_ntoa(sbi.addr4));
  Logger::flexcn_app().info("    Port ................: %d", sbi.port);
  Logger::flexcn_app().info("    HTTP2 port ..........: %d", sbi_http2_port);
  Logger::flexcn_app().info(
      "    API version..........: %s", sbi_api_version.c_str());

   Logger::flexcn_app().info("- FLEXCN:");
    Logger::flexcn_app().info(
        "    IPv4 Addr ...........: %s",
        inet_ntoa(*((struct in_addr*) &nrf_addr.ipv4_addr)));
    Logger::flexcn_app().info("    Port ................: %lu  ", nrf_addr.port);
    Logger::flexcn_app().info(
        "    API version .........: %s", nrf_addr.api_version.c_str());

  Logger::flexcn_app().info("- Supported Features:");
  Logger::flexcn_app().info(
      "    Register to NRF............: %s", register_nrf ? "Yes" : "No");
}

//------------------------------------------------------------------------------
int flexcn_config::get_pfcp_node_id(pfcp::node_id_t& node_id) {
  node_id = {};
  if (n4.addr4.s_addr) {
    node_id.node_id_type    = pfcp::NODE_ID_TYPE_IPV4_ADDRESS;
    node_id.u1.ipv4_address = n4.addr4;
    return RETURNok;
  }
  if (n4.addr6.s6_addr32[0] | n4.addr6.s6_addr32[1] | n4.addr6.s6_addr32[2] |
      n4.addr6.s6_addr32[3]) {
    node_id.node_id_type    = pfcp::NODE_ID_TYPE_IPV6_ADDRESS;
    node_id.u1.ipv6_address = n4.addr6;
    return RETURNok;
  }
  return RETURNerror;
}
//------------------------------------------------------------------------------
int flexcn_config::get_pfcp_fseid(pfcp::fseid_t& fseid) {
  int rc = RETURNerror;
  fseid  = {};
  if (n4.addr4.s_addr) {
    fseid.v4           = 1;
    fseid.ipv4_address = n4.addr4;
    rc                 = RETURNok;
  }
  if (n4.addr6.s6_addr32[0] | n4.addr6.s6_addr32[1] | n4.addr6.s6_addr32[2] |
      n4.addr6.s6_addr32[3]) {
    fseid.v6           = 1;
    fseid.ipv6_address = n4.addr6;
    rc                 = RETURNok;
  }
  return rc;
}

//------------------------------------------------------------------------------
flexcn_config::~flexcn_config() {}