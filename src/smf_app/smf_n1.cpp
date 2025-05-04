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

#include "smf_n1.hpp"

#include <arpa/inet.h>

#include <stdexcept>

#include "smf_3gpp_conversions.hpp"
#include "AllowedSscMode.hpp"
#include "PduSessionAuthenticationCommand.hpp"
#include "PduSessionAuthenticationComplete.hpp"
#include "PduSessionAuthenticationResult.hpp"
#include "PduSessionEstablishmentAccept.hpp"
#include "PduSessionEstablishmentReject.hpp"
#include "PduSessionEstablishmentRequest.hpp"
#include "PduSessionModificationCommand.hpp"
#include "PduSessionModificationCommandReject.hpp"
#include "PduSessionModificationComplete.hpp"
#include "PduSessionModificationReject.hpp"
#include "PduSessionModificationRequest.hpp"
#include "PduSessionReleaseCommand.hpp"
#include "PduSessionReleaseComplete.hpp"
#include "PduSessionReleaseReject.hpp"
#include "PduSessionReleaseRequest.hpp"
#include "_5gsmStatus.hpp"
#include "epc.h"
#include "output_wrapper.hpp"
#include "smf.h"
#include "smf_app.hpp"
#include "string.hpp"
#include "AlwaysOnPduSessionIndication.hpp"
#include "common_defs.hpp"

using namespace smf;
using namespace oai::utils;
using namespace oai::nas;
extern smf_app* smf_app_inst;

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_establishment_accept(
    pdu_session_create_sm_context_response& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info(
      "Create N1 SM Container, PDU Session Establishment Accept");
  int bytes                   = {0};
  unsigned char data[BUF_LEN] = {'\0'};
  // nas_message_t nas_msg       = {};
  bool result = false;

  auto pdu_session_estb_accept =
      std::make_unique<PduSessionEstablishmentAccept>();

  pdu_session_estb_accept->SetHeader(
      sm_context_res.get_pdu_session_id(),
      sm_context_res.get_pti().procedure_transaction_id);
  Logger::smf_n1().debug(
      "PDU Session Establishment Accept, PDU Session Identity 0x%x, Procedure "
      "Transaction Identity "
      "0x%x",
      sm_context_res.get_pdu_session_id(),
      sm_context_res.get_pti().procedure_transaction_id);

  std::map<uint8_t, qos_flow_context_updated> qos_flows = {};
  sm_context_res.get_all_qos_flow_context_created(qos_flows);

  Logger::smf_n1().info("PDU_SESSION_ESTABLISHMENT_ACCEPT, encode starting...");

  // Fill the content of PDU Session Establishment Accept message
  // PDU Session Type
  PduSessionType pdu_session_type;
  pdu_session_type.SetValue(sm_context_res.get_pdu_session_type());
  pdu_session_estb_accept->SetSelectedPduSessionType(pdu_session_type);
  Logger::smf_n1().debug("PDU Session Type: %d", pdu_session_type.GetValue());

  // TODO: Selected SSC mode
  SscMode ssc_mode;
  ssc_mode.Set(true);        // 4 high bits
  ssc_mode.SetSscMode(0x1);  // SSC mode 1 allowed, SSC mode 2/3 not allowed
  pdu_session_estb_accept->SetSelectedSscMode(ssc_mode);

  // authorized QoS rules of the PDU session: QOSRules (Section 6.2.5@3GPP
  // TS 24.501) (Section 6.4.1.3@3GPP TS 24.501 V16.1.0) Make sure that the
  // number of the packet filters used in the authorized QoS rules of the PDU
  // Session does not exceed the maximum number of packet filters supported by
  // the UE for the PDU session

  std::vector<oai::nas::QosRule> qos_rule_list = {};
  for (const auto& qos_flow_pair : qos_flows) {
    for (auto qos_rule : qos_flow_pair.second.qos_rules) {
      qos_rule_list.push_back(qos_rule.second);
    }
  }
  oai::nas::QosRules qos_rules;
  qos_rules.Set(qos_rule_list);
  pdu_session_estb_accept->SetAuthorizedQosRules(qos_rules);

  // 5GSM Cause
  // _5gsmCause cause = {};
  // cause.SetValue(static_cast<uint8_t>(sm_cause));
  // pdu_session_estb_accept->Set5gsmCause(cause);

  // SessionAMBR
  Logger::smf_n1().debug("Get default values for Session-AMBR");
  supi_t supi                        = sm_context_res.get_supi();
  supi64_t supi64                    = smf_supi_to_u64(supi);
  std::shared_ptr<smf_context> sc    = {};
  oai::nas::SessionAmbr session_ambr = {};
  if (smf_app_inst->is_supi_2_smf_context(supi64)) {
    Logger::smf_n1().debug("Get SMF context with SUPI " SUPI_64_FMT "", supi64);
    sc = smf_app_inst->supi_2_smf_context(supi64);
    sc.get()->get_session_ambr(
        session_ambr, sm_context_res.get_snssai(), sm_context_res.get_dnn());
  } else {
    Logger::smf_n1().warn(
        "SMF context with SUPI " SUPI_64_FMT " does not exist!", supi64);
    return false;
  }
  pdu_session_estb_accept->SetSessionAmbr(session_ambr);

  // PDUAddress
  paa_t paa = sm_context_res.get_paa();
  Logger::smf_n1().debug(
      "PDU Session Type %s", paa.pdu_session_type.to_string().c_str());
  oai::nas::PduAddress pdu_address(kIeiPduAddress);
  pdu_address.SetPduSessionType(paa.pdu_session_type.pdu_session_type);
  if (paa.pdu_session_type.pdu_session_type == PDU_SESSION_TYPE_E_IPV4) {
    pdu_address.SetIpv4Address(paa.ipv4_address);
    Logger::smf_n1().debug(
        "UE IPv4 Address %s", conv::toString(paa.ipv4_address).c_str());
  } else if (
      paa.pdu_session_type.pdu_session_type == PDU_SESSION_TYPE_E_IPV4V6) {
    pdu_address.SetIpv4v6Address(paa.ipv4_address, paa.ipv6_address);
    Logger::smf_n1().debug(
        "UE IPv4 Address %s", conv::toString(paa.ipv4_address).c_str());
    char str_addr6[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &paa.ipv6_address, str_addr6, sizeof(str_addr6))) {
      Logger::smf_n1().debug("UE IPv6 Address: %s", str_addr6);
    }
  } else if (paa.pdu_session_type.pdu_session_type == PDU_SESSION_TYPE_E_IPV6) {
    // TODO:
    Logger::smf_n1().debug("IPv6 is not fully supported yet!");
  }
  pdu_session_estb_accept->SetPduAddress(pdu_address);

  // TODO: GPRSTimer
  // oai::nas::GprsTimer gprs_timer(kIeiRqTimerValue);
  // gprs_timer.SetUnit(oai::nas::kGprsTimerUnitValueIsIncrementedInMultiplesOf2Seconds);
  // gprs_timer.SetValue(0);

  // SNSSAI
  oai::nas::SNssai snssai(kIeiSNssai);
  snssai.SetSNSSAI(
      sm_context_res.get_snssai().sst,
      sm_context_res.get_snssai().get_sd_int());
  Logger::smf_n1().debug(
      "SNSSAI SST %d, SD %ld (0x%x)", sm_context_res.get_snssai().sst,
      sm_context_res.get_snssai().get_sd_int(),
      sm_context_res.get_snssai().get_sd_int());
  pdu_session_estb_accept->SetSNssai(snssai);

  // AlwaysonPDUSessionIndication
  oai::nas::AlwaysOnPduSessionIndication always_on_pdu_session_indication(
      kIeiAlwaysOnPduSessionIndication);
  always_on_pdu_session_indication.SetApsi(
      oai::nas::kAlwaysOnPduSessionRequired);
  pdu_session_estb_accept->SetAlwaysOnPduSessionIndication(
      always_on_pdu_session_indication);

  // TODO: MappedEPSBearerContexts
  // TODO: EAPMessage

  // Authorized QoS flow descriptions
  // TODO: we may not need this IE (see section 6.4.1.3 @3GPP TS 24.501)
  if (smf_app_inst->is_supi_2_smf_context(supi64) and !qos_flows.empty()) {
    Logger::smf_n1().debug("Get SMF context with SUPI " SUPI_64_FMT "", supi64);
    sc = smf_app_inst->supi_2_smf_context(supi64);
    oai::nas::QosFlowDescriptions qos_flow_descriptions(
        kIeiAuthorizedQosFlowDescriptions);
    std::vector<oai::nas::QosFlowDescription> qos_flow_description_list = {};
    for (const auto& qos_flow_pair : qos_flows) {
      oai::nas::QosFlowDescription qos_flow_description =
          qos_flow_pair.second.get_qos_flow_descriptions();
      qos_flow_description_list.push_back(qos_flow_description);
    }
    qos_flow_descriptions.Set(qos_flow_description_list);
    pdu_session_estb_accept->SetAuthorizedQosFlowDescriptions(
        qos_flow_descriptions);
  }

  // ExtendedProtocolConfigurationOptions
  protocol_configuration_options_t pco_res = {};
  sm_context_res.get_epco(pco_res);
  oai::nas::ExtendedProtocolConfigurationOptions epco = {};
  epco.Set(pco_res);
  pdu_session_estb_accept->SetExtendedProtocolConfigurationOptions(epco);

  // DNN
  plmn_t plmn = {};
  sc->get_plmn(plmn);
  std::string gprs = EPC::Utility::home_network_gprs(plmn);
  std::string full_dnn =
      sm_context_res.get_dnn() + gprs;  //".mnc011.mcc110.gprs";
  std::string dotted;
  string_to_dotted(full_dnn, dotted);
  Logger::smf_n1().debug(
      "Full DNN %s, dotted DNN %s", full_dnn.c_str(), dotted.c_str());
  oai::nas::Dnn dnn = {};
  bstring dnn_bstr  = bfromcstralloc(dotted.length() + 1, "\0");
  string_to_dnn(dotted, dnn_bstr);
  dnn.SetValue(dnn_bstr);
  pdu_session_estb_accept->SetDnn(dnn);

  // TODO: 5GSM network feature support
  // TODO: Serving PLMN rate control
  // TODO: ATSSS container
  // TODO: Control plane only indication
  // TODO: IP header compression Configuration
  // TODO: Ethernet header compression configuration

  // Encode NAS message
  uint32_t msg_len = pdu_session_estb_accept->GetLength();
  Logger::smf_n1().debug(
      "Size of PDU Session Establishment Accept message: %ld (octets)",
      msg_len);

  uint8_t buffer[msg_len] = {0};
  int encoded_size        = pdu_session_estb_accept->Encode(buffer, msg_len);
  if (encoded_size == KEncodeDecodeError) {
    Logger::smf_n1().error(
        "Encode PDU Session Establishment Accept message error");
    return false;
  }

  oai::utils::output_wrapper::print_buffer(
      {}, "Buffer Data:", buffer, encoded_size);

  if (encoded_size > 0) {
    nas_msg_str.assign((char*) buffer, encoded_size);
    result = true;
  }

  return result;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_establishment_reject(
    pdu_session_msg& msg, std::string& nas_msg_str,
    cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info(
      "Create N1 SM Container, PDU Session Establishment Reject");
  bool result = false;

  Logger::smf_n1().info("PDU Session Establishment Reject, encode starting...");
  auto pdu_session_estb_reject =
      std::make_unique<PduSessionEstablishmentReject>();
  // PDU Session ID and Procedure Transaction ID
  pdu_session_estb_reject->SetHeader(
      msg.get_pdu_session_id(), msg.get_pti().procedure_transaction_id);
  Logger::smf_n1().debug(
      "PDU Session Establishment Reject, PDU Session Identity 0x%x, Procedure "
      "Transaction Identity "
      "0x%x",
      msg.get_pdu_session_id(), msg.get_pti().procedure_transaction_id);

  // 5GSM Cause
  _5gsmCause cause = {};
  cause.SetValue(static_cast<uint8_t>(sm_cause));
  pdu_session_estb_reject->Set5gsmCause(cause);
  Logger::smf_n1().debug(
      "PDU Session Establishment Reject,, 5GSM Cause: 0x%x",
      static_cast<uint8_t>(sm_cause));

  // TODO: Back-off timer value

  // AllowedSSCMode
  AllowedSscMode allow_ssc_mode = {};
  allow_ssc_mode.SetValue(0x1);  // SSC mode 1 allowed, SSC mode 2/3 not allowed
  pdu_session_estb_reject->SetAllowedSscMode(allow_ssc_mode);

  // TODO:EAP message
  // TODO: 5GSM congestion re-attempt indicator
  // TODO: Extended protocol configuration options
  // TODO: Re-attempt indicator

  // Encode NAS message
  uint32_t msg_len = pdu_session_estb_reject->GetLength();
  Logger::smf_n1().debug(
      "Size of PDU Session Establishment Reject message: %ld (octets)",
      msg_len);

  uint8_t buffer[msg_len] = {0};
  int encoded_size        = pdu_session_estb_reject->Encode(buffer, msg_len);
  if (encoded_size == KEncodeDecodeError) {
    Logger::smf_n1().error(
        "Encode PDU Session Establishment Reject message error");
    return false;
  }

  oai::utils::output_wrapper::print_buffer(
      {}, "Buffer Data:", buffer, encoded_size);

  if (encoded_size > 0) {
    nas_msg_str.assign((char*) buffer, encoded_size);
    result = true;
  }

  return result;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_modification_command(
    pdu_session_update_sm_context_response& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info(
      "Create N1 SM Container, PDU Session Modification Command "
      "(pdu_session_update_sm_context_response)");

  bool result = false;

  auto pdu_session_modification_command =
      std::make_unique<PduSessionModificationCommand>();
  // PDU Session ID and Procedure Transaction ID
  pdu_session_modification_command->SetHeader(
      sm_context_res.get_pdu_session_id(),
      sm_context_res.get_pti().procedure_transaction_id);
  Logger::smf_n1().debug(
      "PDU Session Modification Command, PDU Session Identity 0x%x, Procedure "
      "Transaction Identity "
      "0x%x",
      sm_context_res.get_pdu_session_id(),
      sm_context_res.get_pti().procedure_transaction_id);

  Logger::smf_n1().debug("PDU Session Modification Command");
  Logger::smf_n1().info("PDU_SESSION_MODIFICATION_COMMAND, encode starting...");

  // Get the SMF_PDU_Session
  std::shared_ptr<smf_context> sc     = {};
  std::shared_ptr<smf_pdu_session> sp = {};
  supi_t supi                         = sm_context_res.get_supi();
  supi64_t supi64                     = smf_supi_to_u64(supi);

  if (smf_app_inst->is_supi_2_smf_context(supi64)) {
    Logger::smf_n1().debug("Get SMF context with SUPI " SUPI_64_FMT "", supi64);
    sc = smf_app_inst->supi_2_smf_context(supi64);
  } else {
    Logger::smf_n1().warn(
        "SMF context with SUPI " SUPI_64_FMT " does not exist!", supi64);
    return false;
  }

  if (!sc->find_pdu_session(sm_context_res.get_pdu_session_id(), sp)) {
    Logger::smf_n1().warn("PDU session context does not exist!");
    return false;
  }

  std::string dnn = sp->get_dnn();
  if (dnn.compare(sm_context_res.get_dnn()) != 0) {
    // error
    Logger::smf_n1().warn("DNN doesn't matched with this session!");
    return false;
  }

  if (!(sp->get_snssai() == sm_context_res.get_snssai())) {
    // error
    Logger::smf_n1().warn("SNSSAI doesn't matched with this session!");
    return false;
  }

  // 5GSM Cause
  _5gsmCause cause = {};
  cause.SetValue(static_cast<uint8_t>(sm_cause));
  pdu_session_modification_command->Set5gsmCause(cause);

  // SessionAMBR
  oai::nas::SessionAmbr session_ambr = {};
  sc->get_session_ambr(
      session_ambr, sm_context_res.get_snssai(), sm_context_res.get_dnn());
  pdu_session_modification_command->SetSessionAmbr(session_ambr);

  // TODO: RQ timer value
  // TODO: AlwaysonPDUSessionIndication

  // Authorized QoS rules
  // Get the authorized QoS Rules
  std::vector<oai::nas::QosRule> qos_rule_list =
      sp->get_session_handler()->get_qos_rules();
  oai::nas::QosRules qos_rules = {};
  qos_rules.Set(qos_rule_list);
  pdu_session_modification_command->SetAuthorizedQosRules(qos_rules);

  // TODO: MappedEPSBearerContexts

  // TODO: Authorized QoS Flow Descriptions
  std::vector<::smf::qos_flow_context_updated> qos_flows =
      sp->get_session_handler()->get_qos_flows_context_updated();

  if (smf_app_inst->is_supi_2_smf_context(supi64) and !qos_flows.empty()) {
    oai::nas::QosFlowDescriptions qos_flow_descriptions                 = {};
    std::vector<oai::nas::QosFlowDescription> qos_flow_description_list = {};
    for (const auto& qf : qos_flows) {
      oai::nas::QosFlowDescription qos_flow_description =
          qf.get_qos_flow_descriptions();
      qos_flow_description_list.push_back(qos_flow_description);
    }
    qos_flow_descriptions.Set(qos_flow_description_list);
    pdu_session_modification_command->SetAuthorizedQosFlowDescriptions(
        qos_flow_descriptions);
  }

  // TODO: Extended protocol configuration options
  // TODO: ATSSS container
  // TODO: IP header compression Configuration
  // TODO: Port management information Container
  // TODO: Serving PLMN rate control
  // TODO: Ethernet header compression Configuration

  // Encode NAS message
  uint32_t msg_len = pdu_session_modification_command->GetLength();
  Logger::smf_n1().debug(
      "Size of PDU Session Modification Command message: %ld (octets)",
      msg_len);

  uint8_t buffer[msg_len] = {0};
  int encoded_size = pdu_session_modification_command->Encode(buffer, msg_len);
  if (encoded_size == KEncodeDecodeError) {
    Logger::smf_n1().error(
        "Encode PDU Session Modification Command message error");
    return false;
  }

  oai::utils::output_wrapper::print_buffer(
      {}, "Buffer Data:", buffer, encoded_size);

  if (encoded_size > 0) {
    nas_msg_str.assign((char*) buffer, encoded_size);
    result = true;
  }

  return result;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_modification_command(
    pdu_session_modification_network_requested& msg, std::string& nas_msg_str,
    cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info(
      "Create N1 SM Container, PDU Session Modification Command "
      "(pdu_session_modification_network_requested)");

  bool result = false;

  auto pdu_session_modification_command =
      std::make_unique<PduSessionModificationCommand>();
  // PDU Session ID and Procedure Transaction ID
  pdu_session_modification_command->SetHeader(
      msg.get_pdu_session_id(), msg.get_pti().procedure_transaction_id);
  Logger::smf_n1().debug(
      "PDU Session Modification Command, PDU Session Identity 0x%x, Procedure "
      "Transaction Identity "
      "0x%x",
      msg.get_pdu_session_id(), msg.get_pti().procedure_transaction_id);

  Logger::smf_n1().debug("PDU Session Modification Command");
  Logger::smf_n1().info("PDU_SESSION_MODIFICATION_COMMAND, encode starting...");

  // Get the SMF_PDU_Session
  std::shared_ptr<smf_context> sc     = {};
  std::shared_ptr<smf_pdu_session> sp = {};
  supi_t supi                         = msg.get_supi();
  supi64_t supi64                     = smf_supi_to_u64(supi);

  if (smf_app_inst->is_supi_2_smf_context(supi64)) {
    Logger::smf_n1().debug("Get SMF context with SUPI " SUPI_64_FMT "", supi64);
    sc = smf_app_inst->supi_2_smf_context(supi64);
  } else {
    Logger::smf_n1().warn(
        "SMF context with SUPI " SUPI_64_FMT " does not exist!", supi64);
    return false;
  }

  if (!sc->find_pdu_session(msg.get_pdu_session_id(), sp)) {
    Logger::smf_n1().warn("PDU session context does not exist!");
    return false;
  }

  std::string dnn = sp->get_dnn();
  if (dnn.compare(msg.get_dnn()) != 0) {
    // error
    Logger::smf_n1().warn("DNN doesn't matched with this session!");
    return false;
  }
  /*
    if (!(sp.get()->get_snssai() ==  msg.get_snssai())){
              // error
              Logger::smf_n1().warn("SNSSAI doesn't matched with this
    session!"); return false;
    }
    */

  // 5GSM Cause
  _5gsmCause cause = {};
  cause.SetValue(static_cast<uint8_t>(sm_cause));
  pdu_session_modification_command->Set5gsmCause(cause);

  // SessionAMBR (default)
  oai::nas::SessionAmbr session_ambr = {};
  sc->get_session_ambr(session_ambr, msg.get_snssai(), msg.get_dnn());
  pdu_session_modification_command->SetSessionAmbr(session_ambr);

  // TODO: RQ timer value
  // TODO: AlwaysonPDUSessionIndication

  // Authorized QoS rules
  // Get the authorized QoS Rules
  std::vector<oai::nas::QosRule> qos_rule_list =
      sp->get_session_handler()->get_qos_rules();

  oai::nas::QosRules qos_rules = {};
  qos_rules.Set(qos_rule_list);
  pdu_session_modification_command->SetAuthorizedQosRules(qos_rules);

  // TODO: MappedEPSBearerContexts

  // TODO: Authorized QoS Flow Descriptions
  std::vector<::smf::qos_flow_context_updated> qos_flows =
      sp->get_session_handler()->get_qos_flows_context_updated();

  if (smf_app_inst->is_supi_2_smf_context(supi64) and !qos_flows.empty()) {
    oai::nas::QosFlowDescriptions qos_flow_descriptions                 = {};
    std::vector<oai::nas::QosFlowDescription> qos_flow_description_list = {};
    for (const auto& qf : qos_flows) {
      oai::nas::QosFlowDescription qos_flow_description =
          qf.get_qos_flow_descriptions();
      qos_flow_description_list.push_back(qos_flow_description);
    }
    qos_flow_descriptions.Set(qos_flow_description_list);
    pdu_session_modification_command->SetAuthorizedQosFlowDescriptions(
        qos_flow_descriptions);
  }

  // TODO: Extended protocol configuration options
  // TODO: ATSSS container
  // TODO: IP header compression Configuration
  // TODO: Port management information Container
  // TODO: Serving PLMN rate control
  // TODO: Ethernet header compression Configuration

  // Encode NAS message
  uint32_t msg_len = pdu_session_modification_command->GetLength();
  Logger::smf_n1().debug(
      "Size of PDU Session Modification Command message: %ld (octets)",
      msg_len);

  uint8_t buffer[msg_len] = {0};
  int encoded_size = pdu_session_modification_command->Encode(buffer, msg_len);
  if (encoded_size == KEncodeDecodeError) {
    Logger::smf_n1().error(
        "Encode PDU Session Modification Command message error");
    return false;
  }

  oai::utils::output_wrapper::print_buffer(
      {}, "Buffer Data:", buffer, encoded_size);

  if (encoded_size > 0) {
    nas_msg_str.assign((char*) buffer, encoded_size);
    result = true;
  }

  return result;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_release_reject(
    pdu_session_update_sm_context_request& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info("Create N1 SM Container, PDU Session Release Reject");
  bool result                     = false;
  auto pdu_session_release_reject = std::make_unique<PduSessionReleaseReject>();
  // PDU Session ID and Procedure Transaction ID
  pdu_session_release_reject->SetHeader(
      sm_context_res.get_pdu_session_id(),
      sm_context_res.get_pti().procedure_transaction_id);
  Logger::smf_n1().debug(
      "PDU Session Release Reject, PDU Session Identity 0x%x, Procedure "
      "Transaction Identity "
      "0x%x",
      sm_context_res.get_pdu_session_id(),
      sm_context_res.get_pti().procedure_transaction_id);

  // 5GSM Cause
  _5gsmCause cause = {};
  cause.SetValue(static_cast<uint8_t>(sm_cause));
  pdu_session_release_reject->Set5gsmCause(cause);

  // TODO: Extended protocol configuration options

  // Encode NAS message
  uint32_t msg_len = pdu_session_release_reject->GetLength();
  Logger::smf_n1().debug(
      "Size of PDU Session Release Reject message: %ld (octets)", msg_len);

  uint8_t buffer[msg_len] = {0};
  int encoded_size        = pdu_session_release_reject->Encode(buffer, msg_len);
  if (encoded_size == KEncodeDecodeError) {
    Logger::smf_n1().error("Encode PDU Session Release Reject message error");
    return false;
  }

  oai::utils::output_wrapper::print_buffer(
      {}, "Buffer Data:", buffer, encoded_size);

  if (encoded_size > 0) {
    nas_msg_str.assign((char*) buffer, encoded_size);
    result = true;
  }

  return result;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_release_command(
    const std::shared_ptr<pdu_session_msg>& msg, std::string& nas_msg_str,
    cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info("Create N1 SM Container, PDU Session Release Command");

  bool result = false;

  auto pdu_session_release_command =
      std::make_unique<PduSessionReleaseCommand>();
  // PDU Session ID and Procedure Transaction ID
  pdu_session_release_command->SetHeader(
      msg->get_pdu_session_id(), msg->get_pti().procedure_transaction_id);
  Logger::smf_n1().debug(
      "PDU Session Release Command, PDU Session Identity 0x%x, Procedure "
      "Transaction Identity "
      "0x%x",
      msg->get_pdu_session_id(), msg->get_pti().procedure_transaction_id);

  // TODO: if PDU session release procedure
  // is not triggered by a UE-requested PDU
  // session release set the PTI IE of the
  // PDU SESSION RELEASE COMMAND message
  // to "No procedure transaction identity
  // assigned"

  // 5GSM Cause
  _5gsmCause cause = {};
  cause.SetValue(static_cast<uint8_t>(sm_cause));
  pdu_session_release_command->Set5gsmCause(cause);

  // TODO: Back-off timer value
  // TODO: EAP Message
  // TODO: 5GSM Congestion Reattempt Indicator
  // TODO: Extended Protocol Configuration Options
  // TODO: Access Type

  // Encode NAS message
  uint32_t msg_len = pdu_session_release_command->GetLength();
  Logger::smf_n1().debug(
      "Size of PDU Session Release Command message: %ld (octets)", msg_len);

  uint8_t buffer[msg_len] = {0};
  int encoded_size = pdu_session_release_command->Encode(buffer, msg_len);
  if (encoded_size == KEncodeDecodeError) {
    Logger::smf_n1().error("Encode PDU Session Release Command message error");
    return false;
  }

  oai::utils::output_wrapper::print_buffer(
      {}, "Buffer Data:", buffer, encoded_size);

  if (encoded_size > 0) {
    nas_msg_str.assign((char*) buffer, encoded_size);
    result = true;
  }

  return result;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_release_command(
    pdu_session_update_sm_context_response& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info(
      "Create N1 SM Container, PDU Session Release Command "
      "(pdu_session_update_sm_context_response)");

  bool result = false;

  auto pdu_session_release_command =
      std::make_unique<PduSessionReleaseCommand>();
  // PDU Session ID and Procedure Transaction ID
  pdu_session_release_command->SetHeader(
      sm_context_res.get_pdu_session_id(),
      sm_context_res.get_pti().procedure_transaction_id);
  Logger::smf_n1().debug(
      "PDU Session Release Command, PDU Session Identity 0x%x, Procedure "
      "Transaction Identity "
      "0x%x",
      sm_context_res.get_pdu_session_id(),
      sm_context_res.get_pti().procedure_transaction_id);

  // TODO: if PDU session release procedure
  // is not triggered by a UE-requested PDU
  // session release set the PTI IE of the
  // PDU SESSION RELEASE COMMAND message
  // to "No procedure transaction identity
  // assigned"

  // 5GSM Cause
  _5gsmCause cause = {};
  cause.SetValue(static_cast<uint8_t>(sm_cause));
  pdu_session_release_command->Set5gsmCause(cause);

  // TODO: Back-off timer value
  // TODO: EAP Message
  // TODO: 5GSM Congestion Reattempt Indicator
  // TODO: Extended Protocol Configuration Options
  // TODO: Access Type

  // Encode NAS message
  uint32_t msg_len = pdu_session_release_command->GetLength();
  Logger::smf_n1().debug(
      "Size of PDU Session Release Command message: %ld (octets)", msg_len);

  uint8_t buffer[msg_len] = {0};
  int encoded_size = pdu_session_release_command->Encode(buffer, msg_len);
  if (encoded_size == KEncodeDecodeError) {
    Logger::smf_n1().error("Encode PDU Session Release Command message error");
    return false;
  }

  oai::utils::output_wrapper::print_buffer(
      {}, "Buffer Data:", buffer, encoded_size);

  if (encoded_size > 0) {
    nas_msg_str.assign((char*) buffer, encoded_size);
    result = true;
  }

  return result;
}

//-----------------------------------------------------------------------------------------------------
bool create_n1_pdu_session_release_command(
    pdu_session_modification_network_requested& msg, std::string& nas_msg_str,
    cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info(
      "Create N1 SM Container, PDU Session Release Command "
      "(pdu_session_modification_network_requested)");
  // TODO:
  return true;
}

//------------------------------------------------------------------------------
int smf_n1::decode_n1_sm_container(
    std::shared_ptr<Nas5gsmMessage>& nas_msg, const std::string& n1_sm_msg) {
  Logger::smf_n1().info("Decode NAS message from N1 SM Container.");

  // step 1. Decode NAS  message
  unsigned int data_len = n1_sm_msg.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  if (!data) {
    Logger::smf_n1().debug("Error when allocating memory.");
    return KEncodeDecodeError;
  }
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n1_sm_msg.c_str(), data_len);

  if (Logger::should_log(spdlog::level::debug)) {
    printf("Content: ");
    for (int i = 0; i < data_len; i++) printf(" %02x ", data[i]);
    printf("\n");
  }

  if (nas_msg->Decode(data, data_len) == KEncodeDecodeError) {
    Logger::smf_n1().debug("Error when decode NAS header.");
    return KEncodeDecodeError;
  }

  switch (nas_msg->GetHeader().GetMessageType()) {
    case kPduSessionEstablishmentRequest: {
      nas_msg = std::make_shared<PduSessionEstablishmentRequest>();
    } break;

    case kPduSessionEstablishmentAccept: {
      nas_msg = std::make_shared<PduSessionEstablishmentAccept>();
    } break;
    case kPduSessionEstablishmentReject: {
      nas_msg = std::make_shared<PduSessionEstablishmentReject>();
    } break;
    case kPduSessionAuthenticationCommand: {
      nas_msg = std::make_shared<PduSessionAuthenticationCommand>();
    } break;
    case kPduSessionAuthenticationComplete: {
      nas_msg = std::make_shared<PduSessionAuthenticationComplete>();
    } break;
    case kPduSessionAuthenticationResult: {
      nas_msg = std::make_shared<PduSessionAuthenticationResult>();
    } break;
    case kPduSessionModificationRequest: {
      nas_msg = std::make_shared<PduSessionModificationRequest>();
    } break;
    case kPduSessionModificationReject: {
      nas_msg = std::make_shared<PduSessionModificationReject>();
    } break;
    case kPduSessionModificationCommand: {
      nas_msg = std::make_shared<PduSessionModificationCommand>();
    } break;
    case kPduSessionModificationComplete: {
      nas_msg = std::make_shared<PduSessionModificationComplete>();
    } break;
    case kPduSessionModificationCommandReject: {
      nas_msg = std::make_shared<PduSessionModificationCommandReject>();
    } break;
    case kPduSessionReleaseRequest: {
      nas_msg = std::make_shared<PduSessionReleaseRequest>();
    } break;
    case kPduSessionReleaseReject: {
      nas_msg = std::make_shared<PduSessionReleaseReject>();
    } break;
    case kPduSessionReleaseCommand: {
      nas_msg = std::make_shared<PduSessionReleaseCommand>();
    } break;
    case kPduSessionReleaseComplete: {
      nas_msg = std::make_shared<PduSessionReleaseComplete>();
    } break;
    case k5gsmStatus: {
      nas_msg = std::make_shared<_5gsmStatus>();
    } break;
    default: {
      return KEncodeDecodeError;
    }
  }

  int decoded_size = nas_msg->Decode(data, data_len);
  if (decoded_size == KEncodeDecodeError) {
    Logger::smf_n1().debug("Error when decode NAS message.");
    return KEncodeDecodeError;
  }

  return decoded_size;
}
