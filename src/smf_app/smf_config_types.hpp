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

/*! \file smf_config_types.hpp
 * \brief
 \author Stefan Spettel
 \company phine.tech
 \date 2023
 \email: stefan.spettel@phine.tech
 */

#pragma once

#include "config_types.hpp"
#include "3gpp_29.571.h"
#include "3gpp_24.501.h"
#include "smf.h"

namespace oai::config::smf {

class smf_support_features : public config_type {
 private:
  option_config_value m_local_subscription_infos{};
  option_config_value m_local_pcc_rules{};
  option_config_value m_external_ausf{};
  option_config_value m_external_udm{};
  option_config_value m_external_nssf{};

 public:
  explicit smf_support_features(
      bool local_subscription_info, bool local_pcc_rules);
  explicit smf_support_features(
      bool external_ausf, bool external_udm, bool external_nssf);

  void from_yaml(const YAML::Node& node) override;

  // TODO can we unify SMF-style use_local_subscription_infos and AMF
  // use_external_UDM?
  [[nodiscard]] std::string to_string(const std::string& indent) const override;
  [[nodiscard]] bool use_local_subscription_info() const;
  [[nodiscard]] bool use_local_pcc_rules() const;
  [[nodiscard]] bool use_external_ausf() const;
  [[nodiscard]] bool use_external_udm() const;
  [[nodiscard]] bool use_external_nssf() const;
};

// TODO remove this after refactor
class upf_info_config_value : public config_type {
 private:
  string_config_value m_n3_nwi;
  string_config_value m_n6_nwi;

 public:
  explicit upf_info_config_value(
      const std::string& n3_nwi, const std::string& n6_nwi);

  void from_yaml(const YAML::Node& node) override;

  [[nodiscard]] std::string to_string(const std::string& indent) const override;

  [[nodiscard]] const std::string& get_n3_nwi() const;

  [[nodiscard]] const std::string& get_n6_nwi() const;
};

class upf : public config_type {
 private:
  string_config_value m_host;
  int_config_value m_port;
  option_config_value m_usage_reporting;
  option_config_value m_dl_pdr_in_session_establishment;
  string_config_value m_local_n3_ipv4;
  // TODO this is just stupid
  // We have to refactor as follows:
  // 1) Use the UPF info from the model here
  // Write a small parser that converts YAML to JSON and then call the from_json
  // method Like I did on the PCF for the PCC rules Now we have 3(!!!!) UPF info
  // DTOs, but I can't / don't want to just easily move the smf_profile in the
  // common src submodule
  upf_info_config_value m_upf_config_value;

 public:
  explicit upf(
      const std::string& host, int port, bool enable_usage_reporting,
      bool enable_dl_pdr_in_session_establishment,
      const std::string& local_n3_ip);

  void from_yaml(const YAML::Node& node) override;

  [[nodiscard]] std::string to_string(const std::string& indent) const override;

  void validate() override;

  [[nodiscard]] const std::string& get_host() const;

  [[nodiscard]] uint16_t get_port() const;

  [[nodiscard]] bool enable_usage_reporting() const;
  [[nodiscard]] bool enable_dl_pdr_in_session_establishment() const;
  [[nodiscard]] const std::string& get_local_n3_ip() const;
  [[nodiscard]] const upf_info_config_value& get_upf_info() const;
};

class ims_config : public config_type {
 private:
  string_config_value m_pcscf_v4;
  string_config_value m_pcscf_v6;

  // generated values
  in_addr m_pcscf_v4_ip{};
  in6_addr m_pcscf_v6_ip{};

 public:
  explicit ims_config(
      const std::string& pcscf_ip_v4, const std::string& pcscf_ip_v6);

  void from_yaml(const YAML::Node& node) override;
  [[nodiscard]] std::string to_string(const std::string& indent) const override;

  void validate() override;

  [[nodiscard]] const in_addr& get_pcscf_v4() const;
  [[nodiscard]] const in6_addr& get_pcscf_v6() const;
};

class qos_profile_config_value : public config_type {
  // TODO this is not ideal we should merge this ( and also the validation) with
  // the QosProfile from the UDM API
 private:
  int_config_value m_5qi;
  int_config_value m_priority;
  int_config_value m_arp_priority;
  string_config_value m_arp_preempt_capability;
  string_config_value m_arp_preempt_vulnerability;
  string_config_value m_session_ambr_dl;
  string_config_value m_session_ambr_ul;

  // generated values
  subscribed_default_qos_t m_qos;
  session_ambr_t m_ambr;

 public:
  explicit qos_profile_config_value(
      const subscribed_default_qos_t& qos, const session_ambr_t& ambr);

  void from_yaml(const YAML::Node& node) override;

  [[nodiscard]] std::string to_string(const std::string& indent) const override;

  void validate() override;

  [[nodiscard]] const subscribed_default_qos_t& get_default_qos() const;
  [[nodiscard]] const session_ambr_t& get_session_ambr() const;
};

class snssai_config_value : public config_type {
 private:
  int_config_value m_sd;
  int_config_value m_sst;
  snssai_t m_snssai;

 public:
  explicit snssai_config_value(const snssai_t& snssai);

  void from_yaml(const YAML::Node& node) override;

  [[nodiscard]] std::string to_string(const std::string& indent) const override;

  void validate() override;

  [[nodiscard]] const snssai_t& get_snssai() const;
};

class subscription_info_config : public config_type {
 private:
  string_config_value m_dnn;
  int_config_value m_ssc_mode;
  qos_profile_config_value m_qos_profile;
  snssai_config_value m_snssai;

 public:
  explicit subscription_info_config(
      const std::string& dnn, uint8_t ssc_mode,
      const subscribed_default_qos_t& qos, const session_ambr_t& session_ambr,
      const snssai_t& snssai);

  void from_yaml(const YAML::Node& node) override;

  [[nodiscard]] std::string to_string(const std::string& indent) const override;

  void validate() override;

  [[nodiscard]] const std::string& get_dnn() const;
  [[nodiscard]] uint8_t get_ssc_mode() const;
  [[nodiscard]] const subscribed_default_qos_t& get_default_qos() const;
  [[nodiscard]] const session_ambr_t& get_session_ambr() const;
  [[nodiscard]] const snssai_t& get_single_nssai() const;
};

class smf_config_type : public nf {
 private:
  smf_support_features m_support_feature;
  ue_dns m_ue_dns;
  ims_config m_ims_config;
  std::vector<upf> m_upfs;
  std::vector<subscription_info_config> m_subscription_infos;

  int_config_value m_ue_mtu;

 public:
  explicit smf_config_type(
      const std::string& name, const std::string& host,
      const sbi_interface& sbi, const local_interface& n4);
  void from_yaml(const YAML::Node& node) override;

  [[nodiscard]] std::string to_string(const std::string& indent) const override;

  void validate() override;

  [[nodiscard]] const smf_support_features& get_smf_support_features() const;
  [[nodiscard]] const ue_dns& get_ue_dns() const;
  [[nodiscard]] const ims_config& get_ims_config() const;

  [[nodiscard]] uint16_t get_ue_mtu() const;

  [[nodiscard]] const std::vector<upf>& get_upfs() const;
  [[nodiscard]] std::vector<subscription_info_config>& get_subscription_info();
};

}  // namespace oai::config::smf
