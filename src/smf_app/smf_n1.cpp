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

/*! \file smf_n1.cpp
 \brief
 \author  Tien-Thinh NGUYEN, Keliang DU
 \company Eurecom
 \date 2019
 \email:  tien-thinh.nguyen@eurecom.fr
 */

#include "smf_n1.hpp"

#include <arpa/inet.h>
#include <stdexcept>
#include "string.hpp"

#include "smf.h"
#include "smf_app.hpp"
#include "3gpp_conversions.hpp"
#include "epc.h"

extern "C" {
#include "dynamic_memory_check.h"
#include "nas_message.h"
}

using namespace smf;
extern smf_app* smf_app_inst;

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_establishment_accept(
    pdu_session_create_sm_context_response& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  return true;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_establishment_reject(
    pdu_session_msg& msg, std::string& nas_msg_str,
    cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info(
      "Create N1 SM Container, PDU Session Establishment Reject");
  int bytes                   = {0};
  unsigned char data[BUF_LEN] = {'\0'};
  nas_message_t nas_msg       = {};
  bool result                 = false;

  nas_msg.header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  nas_msg.header.security_header_type = SECURITY_HEADER_TYPE_NOT_PROTECTED;

  SM_msg* sm_msg = &nas_msg.plain.sm;

  // Fill the content of SM header
  sm_msg->header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  sm_msg->header.pdu_session_identity = msg.get_pdu_session_id();

  Logger::smf_n1().info("PDU_SESSION_ESTABLISHMENT_REJECT, encode starting...");

  // Fill the content of PDU Session Establishment Reject message
  sm_msg->header.pdu_session_identity = msg.get_pdu_session_id();
  sm_msg->header.procedure_transaction_identity =
      msg.get_pti().procedure_transaction_id;
  sm_msg->header.message_type = PDU_SESSION_ESTABLISHMENT_REJECT;
  Logger::smf_n1().debug(
      "NAS header, Extended Protocol Discriminator 0x%x, Security Header "
      "Type: 0x%x",
      nas_msg.header.extended_protocol_discriminator,
      nas_msg.header.security_header_type);
  Logger::smf_n1().debug(
      "SM header, PDU Session Identity 0x%x, Procedure Transaction Identity "
      "0x%x, Message Type 0x%x",
      sm_msg->header.pdu_session_identity,
      sm_msg->header.procedure_transaction_identity,
      sm_msg->header.message_type);

  sm_msg->pdu_session_establishment_reject._5gsmcause =
      static_cast<uint8_t>(sm_cause);
  // Presence, should be updated according to the following IEs
  sm_msg->pdu_session_establishment_reject.presence =
      PDU_SESSION_ESTABLISHMENT_REJECT_ALLOWED_SSC_MODE_PRESENCE;
  /*
   //TODO: GPRSTimer3
   sm_msg->pdu_session_establishment_reject.gprstimer3.unit =
   GPRSTIMER3_VALUE_IS_INCREMENTED_IN_MULTIPLES_OF_1_HOUR;
   sm_msg->pdu_session_establishment_reject.gprstimer3.timeValue = 0;
   */
  // AllowedSSCMode
  sm_msg->pdu_session_establishment_reject.allowedsscmode.is_ssc1_allowed =
      SSC_MODE1_ALLOWED;
  sm_msg->pdu_session_establishment_reject.allowedsscmode.is_ssc2_allowed =
      SSC_MODE2_NOT_ALLOWED;
  sm_msg->pdu_session_establishment_reject.allowedsscmode.is_ssc3_allowed =
      SSC_MODE3_NOT_ALLOWED;

  Logger::smf_n1().debug(
      "SM MSG, 5GSM Cause: 0x%x",
      sm_msg->pdu_session_establishment_reject._5gsmcause);
  Logger::smf_n1().debug(
      "SM MSG, Allowed SSC Mode, SSC1 allowed 0x%x, SSC2 allowed 0x%x, SSC3 "
      "allowed 0x%x",
      sm_msg->pdu_session_establishment_reject.allowedsscmode.is_ssc1_allowed,
      sm_msg->pdu_session_establishment_reject.allowedsscmode.is_ssc2_allowed,
      sm_msg->pdu_session_establishment_reject.allowedsscmode.is_ssc3_allowed);

  // Encode NAS message
  bytes = nas_message_encode(
      data, &nas_msg, sizeof(data) /*don't know the size*/, nullptr);

#if DEBUG_IS_ON
  Logger::smf_n1().debug("Buffer Data: ");
  for (int i = 0; i < bytes; i++) printf("%02x ", data[i]);
  printf(" (bytes %d)\n", bytes);
#endif

  if (bytes > 0) {
    std::string n1Message((char*) data, bytes);
    nas_msg_str = n1Message;
    result      = true;
  } else {
    result = false;
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

  int bytes                   = {0};
  unsigned char data[BUF_LEN] = {'\0'};
  nas_message_t nas_msg       = {};
  bool result                 = false;

  nas_msg.header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  nas_msg.header.security_header_type = SECURITY_HEADER_TYPE_NOT_PROTECTED;

  SM_msg* sm_msg = &nas_msg.plain.sm;

  // Fill the content of SM header
  sm_msg->header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  sm_msg->header.pdu_session_identity = sm_context_res.get_pdu_session_id();

  Logger::smf_n1().debug("PDU Session Modification Command");
  Logger::smf_n1().info("PDU_SESSION_MODIFICATION_COMMAND, encode starting...");

  // Get the SMF_PDU_Session
  std::shared_ptr<smf_context> sc     = {};
  std::shared_ptr<dnn_context> sd     = {};
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

  bool find_dnn = sc.get()->find_dnn_context(
      sm_context_res.get_snssai(), sm_context_res.get_dnn(), sd);
  bool find_pdu = false;
  if (find_dnn) {
    find_pdu =
        sd.get()->find_pdu_session(sm_context_res.get_pdu_session_id(), sp);
  }
  if (!find_dnn or !find_pdu) {
    // error
    Logger::smf_n1().warn("DNN or PDU session context does not exist!");
    return false;
  }

  sm_msg->header.procedure_transaction_identity =
      sm_context_res.get_pti().procedure_transaction_id;
  sm_msg->header.message_type = PDU_SESSION_MODIFICATION_COMMAND;
  sm_msg->pdu_session_modification_command.presence =
      0xff;  // TODO: to be updated
  sm_msg->pdu_session_modification_command._5gsmcause =
      static_cast<uint8_t>(sm_cause);
  // SessionAMBR (default)
  sc.get()->get_session_ambr(
      sm_msg->pdu_session_modification_command.sessionambr,
      sm_context_res.get_snssai(), sm_context_res.get_dnn());

  // TODO: GPRSTimer
  // TODO: AlwaysonPDUSessionIndication

  // QOSRules
  // Get the authorized QoS Rules
  std::vector<QOSRulesIE> qos_rules;
  sp.get()->get_qos_rules_to_be_synchronised(qos_rules);

  if (qos_rules.size() == 0) {
    return false;
  }

  sm_msg->pdu_session_modification_command.qosrules.lengthofqosrulesie =
      qos_rules.size();

  sm_msg->pdu_session_modification_command.qosrules.qosrulesie =
      (QOSRulesIE*) calloc(qos_rules.size(), sizeof(QOSRulesIE));
  for (int i = 0; i < qos_rules.size(); i++) {
    Logger::smf_n1().debug(
        "QoS Rule to be updated (Id %d)", qos_rules[i].qosruleidentifer);
    memcpy(
        &sm_msg->pdu_session_modification_command.qosrules.qosrulesie[i],
        &qos_rules[i], sizeof(QOSRulesIE));
  }

  // TODO: MappedEPSBearerContexts

  // QOSFlowDescriptions
  // TODO: get authorized QoS flow descriptions IE
  if (smf_app_inst->is_supi_2_smf_context(supi64)) {
    Logger::smf_n1().debug("Get SMF context with SUPI " SUPI_64_FMT "", supi64);
    sc = smf_app_inst->supi_2_smf_context(supi64);
    sm_msg->pdu_session_modification_command.qosflowdescriptions
        .qosflowdescriptionsnumber = 1;
    sm_msg->pdu_session_modification_command.qosflowdescriptions
        .qosflowdescriptionscontents = (QOSFlowDescriptionsContents*) calloc(
        1, sizeof(QOSFlowDescriptionsContents));
    sc.get()->get_default_qos_flow_description(
        sm_msg->pdu_session_modification_command.qosflowdescriptions
            .qosflowdescriptionscontents[0],
        sm_context_res.get_pdu_session_type(), qos_rules[0].qosflowidentifer);
  }

  // Encode NAS message
  bytes = nas_message_encode(
      data, &nas_msg, sizeof(data) /*don't know the size*/, nullptr);

#if DEBUG_IS_ON
  Logger::smf_n1().debug("Buffer Data: ");
  for (int i = 0; i < bytes; i++) printf("%02x ", data[i]);
  printf(" (bytes %d)\n", bytes);
#endif

  if (bytes > 0) {
    std::string n1Message((char*) data, bytes);
    nas_msg_str = n1Message;
    result      = true;
  } else {
    result = false;
  }

  // free memory
  free_wrapper(
      (void**) &sm_msg->pdu_session_modification_command.qosrules.qosrulesie);
  free_wrapper((void**) &sm_msg->pdu_session_modification_command
                   .qosflowdescriptions.qosflowdescriptionscontents);

  return result;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_modification_command(
    pdu_session_modification_network_requested& msg, std::string& nas_msg_str,
    cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info(
      "Create N1 SM Container, PDU Session Modification Command "
      "(pdu_session_modification_network_requested)");

  int bytes                   = {0};
  unsigned char data[BUF_LEN] = {'\0'};
  nas_message_t nas_msg       = {};
  bool result                 = false;

  nas_msg.header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  nas_msg.header.security_header_type = SECURITY_HEADER_TYPE_NOT_PROTECTED;

  SM_msg* sm_msg = &nas_msg.plain.sm;

  // Fill the content of SM header
  sm_msg->header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  sm_msg->header.pdu_session_identity = msg.get_pdu_session_id();

  Logger::smf_n1().debug("PDU Session Modification Command");
  Logger::smf_n1().info("PDU_SESSION_MODIFICATION_COMMAND, encode starting...");

  // Get the SMF_PDU_Session
  std::shared_ptr<smf_context> sc     = {};
  std::shared_ptr<dnn_context> sd     = {};
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

  bool find_dnn =
      sc.get()->find_dnn_context(msg.get_snssai(), msg.get_dnn(), sd);
  bool find_pdu = false;
  if (find_dnn) {
    find_pdu = sd.get()->find_pdu_session(msg.get_pdu_session_id(), sp);
  }
  if (!find_dnn or !find_pdu) {
    // error
    Logger::smf_n1().warn("DNN or PDU session context does not exist!");
    return false;
  }

  sm_msg->header.procedure_transaction_identity =
      msg.get_pti().procedure_transaction_id;
  sm_msg->header.message_type = PDU_SESSION_MODIFICATION_COMMAND;
  sm_msg->pdu_session_modification_command.presence =
      0xff;  // TODO: to be updated
  sm_msg->pdu_session_modification_command._5gsmcause =
      static_cast<uint8_t>(sm_cause);
  // SessionAMBR (default)
  sc.get()->get_session_ambr(
      sm_msg->pdu_session_modification_command.sessionambr, msg.get_snssai(),
      msg.get_dnn());

  // TODO: GPRSTimer
  // TODO: AlwaysonPDUSessionIndication

  // QOSRules
  // Get the authorized QoS Rules
  std::vector<QOSRulesIE> qos_rules;
  sp.get()->get_qos_rules_to_be_synchronised(qos_rules);

  if (qos_rules.size() == 0) {
    return false;
  }

  sm_msg->pdu_session_modification_command.qosrules.lengthofqosrulesie =
      qos_rules.size();

  sm_msg->pdu_session_modification_command.qosrules.qosrulesie =
      (QOSRulesIE*) calloc(qos_rules.size(), sizeof(QOSRulesIE));
  for (int i = 0; i < qos_rules.size(); i++) {
    Logger::smf_n1().debug(
        "QoS Rule to be updated (Id %d)", qos_rules[i].qosruleidentifer);
    memcpy(
        &sm_msg->pdu_session_modification_command.qosrules.qosrulesie[i],
        &qos_rules[i], sizeof(QOSRulesIE));
  }

  // TODO: MappedEPSBearerContexts

  // QOSFlowDescriptions, TODO: get authorized QoS flow descriptions IE
  if (smf_app_inst->is_supi_2_smf_context(supi64)) {
    Logger::smf_n1().debug("Get SMF context with SUPI " SUPI_64_FMT "", supi64);
    sc = smf_app_inst->supi_2_smf_context(supi64);
    sm_msg->pdu_session_modification_command.qosflowdescriptions
        .qosflowdescriptionsnumber = 1;
    sm_msg->pdu_session_modification_command.qosflowdescriptions
        .qosflowdescriptionscontents = (QOSFlowDescriptionsContents*) calloc(
        1, sizeof(QOSFlowDescriptionsContents));
    sc.get()->get_default_qos_flow_description(
        sm_msg->pdu_session_modification_command.qosflowdescriptions
            .qosflowdescriptionscontents[0],
        msg.get_pdu_session_type(), qos_rules[0].qosflowidentifer);
  }

  // Encode NAS message
  bytes = nas_message_encode(
      data, &nas_msg, sizeof(data) /*don't know the size*/, nullptr);

#if DEBUG_IS_ON
  Logger::smf_n1().debug("Buffer Data: ");
  for (int i = 0; i < bytes; i++) printf("%02x ", data[i]);
  printf(" (bytes %d)\n", bytes);
#endif

  if (bytes > 0) {
    std::string n1Message((char*) data, bytes);
    nas_msg_str = n1Message;
    result      = true;
  } else {
    result = false;
  }
  // free memory
  free_wrapper(
      (void**) &sm_msg->pdu_session_modification_command.qosrules.qosrulesie);
  free_wrapper((void**) &sm_msg->pdu_session_modification_command
                   .qosflowdescriptions.qosflowdescriptionscontents);

  return result;
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_release_reject(
    pdu_session_update_sm_context_request& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info("Create N1 SM Container, PDU Session Release Reject");

  int bytes                   = {0};
  unsigned char data[BUF_LEN] = {'\0'};
  nas_message_t nas_msg       = {};

  nas_msg.header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  nas_msg.header.security_header_type = SECURITY_HEADER_TYPE_NOT_PROTECTED;

  SM_msg* sm_msg = &nas_msg.plain.sm;

  // Fill the content of SM header
  sm_msg->header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  sm_msg->header.pdu_session_identity = sm_context_res.get_pdu_session_id();

  // Fill the content of PDU Session Release Reject
  sm_msg->header.pdu_session_identity = sm_context_res.get_pdu_session_id();
  sm_msg->header.procedure_transaction_identity =
      sm_context_res.get_pti().procedure_transaction_id;
  sm_msg->header.message_type = PDU_SESSION_RELEASE_REJECT;
  sm_msg->pdu_session_release_reject._5gsmcause =
      static_cast<uint8_t>(sm_cause);

  sm_msg->pdu_session_release_command.presence = 0x00;

  // Encode NAS message
  bytes = nas_message_encode(
      data, &nas_msg, sizeof(data) /*don't know the size*/, nullptr);

#if DEBUG_IS_ON
  Logger::smf_n1().debug("Buffer Data: ");
  for (int i = 0; i < bytes; i++) printf("%02x ", data[i]);
  printf(" (bytes %d)\n", bytes);
#endif

  if (bytes > 0) {
    std::string n1Message((char*) data, bytes);
    nas_msg_str = n1Message;
    return true;
  } else {
    return false;
  }
}

//-----------------------------------------------------------------------------------------------------
bool smf_n1::create_n1_pdu_session_release_command(
    pdu_session_update_sm_context_response& sm_context_res,
    std::string& nas_msg_str, cause_value_5gsm_e sm_cause) {
  Logger::smf_n1().info(
      "Create N1 SM Container, PDU Session Release Command "
      "(pdu_session_update_sm_context_response)");

  int bytes                   = {0};
  unsigned char data[BUF_LEN] = {'\0'};
  nas_message_t nas_msg       = {};

  nas_msg.header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  nas_msg.header.security_header_type = SECURITY_HEADER_TYPE_NOT_PROTECTED;

  SM_msg* sm_msg = &nas_msg.plain.sm;

  // Fill the content of SM header
  sm_msg->header.extended_protocol_discriminator =
      EPD_5GS_SESSION_MANAGEMENT_MESSAGES;
  sm_msg->header.pdu_session_identity = sm_context_res.get_pdu_session_id();

  Logger::smf_n1().info("PDU_SESSION_RELEASE_COMMAND, encode starting...");
  // Fill the content of PDU Session Release Command
  sm_msg->header.pdu_session_identity = sm_context_res.get_pdu_session_id();
  sm_msg->header.procedure_transaction_identity =
      sm_context_res.get_pti().procedure_transaction_id;
  sm_msg->header.message_type = PDU_SESSION_RELEASE_COMMAND;
  sm_msg->pdu_session_release_command._5gsmcause =
      static_cast<uint8_t>(sm_cause);
  // TODO: to be updated when adding the following IEs
  sm_msg->pdu_session_release_command.presence = 0x00;
  // GPRSTimer3
  // EAPMessage
  //_5GSMCongestionReattemptIndicator
  // ExtendedProtocolConfigurationOptions

  Logger::smf_n1().debug(
      "SM message, 5GSM Cause: 0x%x",
      sm_msg->pdu_session_release_command._5gsmcause);

  // Encode NAS message
  bytes = nas_message_encode(
      data, &nas_msg, sizeof(data) /*don't know the size*/, nullptr);

#if DEBUG_IS_ON
  Logger::smf_n1().debug("Buffer Data: ");
  for (int i = 0; i < bytes; i++) printf("%02x ", data[i]);
  printf(" (bytes %d)\n", bytes);
#endif

  if (bytes > 0) {
    std::string n1Message((char*) data, bytes);
    nas_msg_str = n1Message;
    return true;
  } else {
    return false;
  }
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
    nas_message_t& nas_msg, const std::string& n1_sm_msg) {
  Logger::smf_n1().info("Decode NAS message from N1 SM Container.");

  // step 1. Decode NAS  message (for instance, ... only served as an example)
  nas_message_decode_status_t decode_status = {0};
  int decoder_rc                            = RETURNok;

  unsigned int data_len = n1_sm_msg.length();
  unsigned char* data   = (unsigned char*) malloc(data_len + 1);
  memset(data, 0, data_len + 1);
  memcpy((void*) data, (void*) n1_sm_msg.c_str(), data_len);

#if DEBUG_IS_ON
  printf("Content: ");
  for (int i = 0; i < data_len; i++) printf(" %02x ", data[i]);
  printf("\n");
#endif

  // decode the NAS message
  decoder_rc =
      nas_message_decode(data, &nas_msg, data_len, nullptr, &decode_status);
  Logger::smf_n1().debug(
      "NAS message, Extended Protocol Discriminator 0x%x, PDU Session Identity "
      "0x%x, Procedure Transaction Identity 0x%x, Message Type 0x%x",
      nas_msg.plain.sm.header.extended_protocol_discriminator,
      nas_msg.plain.sm.header.pdu_session_identity,
      nas_msg.plain.sm.header.procedure_transaction_identity,
      nas_msg.plain.sm.header.message_type);

  // free memory
  free_wrapper((void**) &data);

  return decoder_rc;
}
