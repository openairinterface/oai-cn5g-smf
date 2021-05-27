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

/*! \file flexcn_app.cpp
 \brief
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#include "smf_app.hpp"

#include <boost/uuid/random_generator.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "3gpp_24.007.h"
#include "3gpp_24.501.h"
#include "3gpp_29.500.h"
#include "3gpp_29.502.h"
#include "3gpp_conversions.hpp"
#include "ProblemDetails.h"
#include "RefToBinaryData.h"
#include "SmContextCreateError.h" //
#include "SmContextCreatedData.h" //
#include "SmContextMessage.h" //
#include "SmContextUpdateError.h" //
#include "async_shell_cmd.hpp" //
#include "common_defs.h" //
#include "conversions.hpp" //
#include "itti.hpp" //
#include "itti_msg_nx.hpp" //
#include "logger.hpp" // common
#include "pfcp.hpp" // pfcp
#include "smf.h" // common
#include "smf_event.hpp" //smf_app
#include "smf_sbi.hpp"
#include "smf_n4.hpp"
#include "smf_paa_dynamic.hpp"
#include "string.hpp" // 

#include "EventNotification.h"

extern "C" {
#include "dynamic_memory_check.h"
}

using namespace smf;

extern util::async_shell_cmd* async_shell_cmd_inst;
extern flexcn_app* flexcn_app_inst;
extern smf_config smf_cfg;
smf_n4* smf_n4_inst   = nullptr;
smf_sbi* smf_sbi_inst = nullptr;
extern itti_mw* itti_inst;

void smf_app_task(void*);

//------------------------------------------------------------------------------
int flexcn_app::apply_config(const smf_config& cfg) {
  Logger::flexcn_app().info("Apply config...");

  paa_t paa = {};
  for (int ia = 0; ia < cfg.num_dnn; ia++) {
    if (cfg.dnn[ia].pool_id_iv4 >= 0) {
      int pool_id = cfg.dnn[ia].pool_id_iv4;
      int range   = be32toh(cfg.ue_pool_range_high[pool_id].s_addr) -
                  be32toh(cfg.ue_pool_range_low[pool_id].s_addr);
      paa_dynamic::get_instance().add_pool(
          cfg.dnn[ia].dnn, pool_id, cfg.ue_pool_range_low[pool_id], range);
      // TODO: check with dnn_label
      //Logger::smf_app().info("Applied config %s", cfg.dnn[ia].dnn.c_str());
      paa.ipv4_address = cfg.ue_pool_range_low[pool_id];
    }
    if (cfg.dnn[ia].pool_id_iv6 >= 0) {
      int pool_id = cfg.dnn[ia].pool_id_iv6;
      paa_dynamic::get_instance().add_pool(
          cfg.dnn[ia].dnn, pool_id, cfg.paa_pool6_prefix[pool_id],
          cfg.paa_pool6_prefix_len[pool_id]);
      paa.ipv6_address = cfg.paa_pool6_prefix[pool_id];

      // TODO: check with dnn_label
    //  Logger::smf_app().info(
    //      "Applied config for IPv6 %s", cfg.dnn[ia].dnn.c_str());
    }
  }

  Logger::flexcn_app().info("Applied config");
  return RETURNok;
}

//------------------------------------------------------------------------------
uint64_t flexcn_app::generate_seid() {
  std::unique_lock<std::mutex> ls(m_seid_n4_generator);
  uint64_t seid = ++seid_n4_generator;
  while ((is_seid_n4_exist(seid)) || (seid == UNASSIGNED_SEID)) {
    seid = ++seid_n4_generator;
  }
  set_seid_n4.insert(seid);
  ls.unlock();
  return seid;
}

// ----------------------------------------------------------
void flexcn_app::add_data_event(const EventNotification &data_event){
  Logger::flexcn_app().info("Add/Merge the following record to database : ");

  // genera
  // Logger::smf_app().info(data_event.to_json());

  // get UE ID from data_event
  std::string supi = data_event.getSupi();

  // get the item from the list of app that has the same ueid
  // data structure: ueid as key, 
  m_database[supi].merge(data_event);  

  // update the data related to this ue with the new data fields.
  // if event == event_0: 
  //     update these field of data that related to this ueid address
  // if ueid in m_datase:
  //     current_ue_context = m_database.get(ueid)
  //     current_ue_context.update(data_event)
  // else:
  //     new_context = new ue_context()     
  // how far should we update these items.
  // done the process. 
}


//------------------------------------------------------------------------------
void flexcn_app::generate_smf_context_ref(std::string& smf_ref) {
  smf_ref = std::to_string(sm_context_ref_generator.get_uid());
}

//------------------------------------------------------------------------------
scid_t flexcn_app::generate_smf_context_ref() {
  return sm_context_ref_generator.get_uid();
}

//------------------------------------------------------------------------------
void flexcn_app::generate_ev_subscription_id(std::string& sub_id) {
  sub_id = std::to_string(evsub_id_generator.get_uid());
}

//------------------------------------------------------------------------------
evsub_id_t flexcn_app::generate_ev_subscription_id() {
  return evsub_id_generator.get_uid();
}

//------------------------------------------------------------------------------
bool flexcn_app::is_seid_n4_exist(const uint64_t& seid) const {
  return bool{set_seid_n4.count(seid) > 0};
}

//------------------------------------------------------------------------------
void flexcn_app::free_seid_n4(const uint64_t& seid) {
  std::unique_lock<std::mutex> ls(m_seid_n4_generator);
  set_seid_n4.erase(seid);
  ls.unlock();
}

//------------------------------------------------------------------------------
void flexcn_app::set_seid_2_smf_context(
    const seid_t& seid, std::shared_ptr<smf_context>& pc) {
  std::unique_lock lock(m_seid2smf_context);
  seid2smf_context[seid] = pc;
}

//------------------------------------------------------------------------------
bool flexcn_app::seid_2_smf_context(
    const seid_t& seid, std::shared_ptr<smf_context>& pc) const {
  std::shared_lock lock(m_seid2smf_context);
  std::map<seid_t, std::shared_ptr<smf_context>>::const_iterator it =
      seid2smf_context.find(seid);
  if (it != seid2smf_context.end()) {
    pc = it->second;
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
void flexcn_app::delete_smf_context(std::shared_ptr<smf_context> spc) {
  supi64_t supi64 = smf_supi_to_u64(spc.get()->get_supi());
  std::unique_lock lock(m_supi2smf_context);
  supi2smf_context.erase(supi64);
}

//------------------------------------------------------------------------------
void flexcn_app::restore_n4_sessions(const seid_t& seid) const {
  std::shared_lock lock(m_seid2smf_context);
  // TODO
}

//------------------------------------------------------------------------------
void smf_app_task(void*) {
  const task_id_t task_id = TASK_FLEXCN_APP;
  itti_inst->notify_task_ready(task_id);

  do {
    std::shared_ptr<itti_msg> shared_msg = itti_inst->receive_msg(task_id);
    auto* msg                            = shared_msg.get();
    switch (msg->msg_type) {

      case N4_SESSION_MODIFICATION_RESPONSE:
        if (itti_n4_session_modification_response* m =
                dynamic_cast<itti_n4_session_modification_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N4_SESSION_DELETION_RESPONSE:
        if (itti_n4_session_deletion_response* m =
                dynamic_cast<itti_n4_session_deletion_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N4_SESSION_REPORT_REQUEST:
        flexcn_app_inst->handle_itti_msg(
            std::static_pointer_cast<itti_n4_session_report_request>(
                shared_msg));
        break;

      case N4_NODE_FAILURE:
        flexcn_app_inst->handle_itti_msg(
            std::static_pointer_cast<itti_n4_node_failure>(shared_msg));
        break;

      case N11_SESSION_N1N2_MESSAGE_TRANSFER_RESPONSE_STATUS:
        if (itti_n11_n1n2_message_transfer_response_status* m =
                dynamic_cast<itti_n11_n1n2_message_transfer_response_status*>(
                    msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N11_SESSION_UPDATE_PDU_SESSION_STATUS:
        if (itti_n11_update_pdu_session_status* m =
                dynamic_cast<itti_n11_update_pdu_session_status*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N11_SESSION_CREATE_SM_CONTEXT_RESPONSE:
        if (itti_n11_create_sm_context_response* m =
                dynamic_cast<itti_n11_create_sm_context_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N11_SESSION_UPDATE_SM_CONTEXT_RESPONSE:
        if (itti_n11_update_sm_context_response* m =
                dynamic_cast<itti_n11_update_sm_context_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N11_SESSION_RELEASE_SM_CONTEXT_RESPONSE:
        if (itti_n11_release_sm_context_response* m =
                dynamic_cast<itti_n11_release_sm_context_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N11_REGISTER_NF_INSTANCE_RESPONSE:
        if (itti_n11_register_nf_instance_response* m =
                dynamic_cast<itti_n11_register_nf_instance_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case N11_UPDATE_NF_INSTANCE_RESPONSE:
        if (itti_n11_update_nf_instance_response* m =
                dynamic_cast<itti_n11_update_nf_instance_response*>(msg)) {
          flexcn_app_inst->handle_itti_msg(std::ref(*m));
        }
        break;

      case TIME_OUT:
        if (itti_msg_timeout* to = dynamic_cast<itti_msg_timeout*>(msg)) {
          Logger::flexcn_app().info("TIME-OUT event timer id %d", to->timer_id);
          switch (to->arg1_user) {
            case TASK_SMF_APP_TRIGGER_T3591:
              flexcn_app_inst->timer_t3591_timeout(to->timer_id, to->arg2_user);
              break;
            case TASK_SMF_APP_TIMEOUT_NRF_HEARTBEAT:
              flexcn_app_inst->timer_nrf_heartbeat_timeout(
                  to->timer_id, to->arg2_user);
              break;
            case TASK_SMF_APP_TIMEOUT_NRF_DEREGISTRATION:
              flexcn_app_inst->timer_nrf_deregistration(
                  to->timer_id, to->arg2_user);
              break;
            default:;
          }
        }
        break;

      case TERMINATE:
        if (itti_msg_terminate* terminate =
                dynamic_cast<itti_msg_terminate*>(msg)) {
          Logger::flexcn_app().info("Received terminate message");
          return;
        }
        break;

      case HEALTH_PING:
        break;

      default:
        Logger::flexcn_app().info("no handler for msg type %d", msg->msg_type);
    }
  } while (true);
}

//------------------------------------------------------------------------------
flexcn_app::flexcn_app(const std::string& config_file)
    : m_seid2smf_context(),
      m_supi2smf_context(),
      m_scid2smf_context(),
      m_sm_context_create_promises(),
      m_sm_context_update_promises(),
      m_sm_context_release_promises() {
  Logger::flexcn_app().startup("Starting...");

  supi2smf_context  = {};
  set_seid_n4       = {};
  seid_n4_generator = 0;

  apply_config(smf_cfg);

  if (itti_inst->create_task(TASK_FLEXCN_APP, smf_app_task, nullptr)) {
    Logger::flexcn_app().error("Cannot create task TASK_FLEXCN_APP");
    throw std::runtime_error("Cannot create task TASK_FLEXCN_APP");
  }

  try {
    //smf_n4_inst  = new smf_n4();
    smf_sbi_inst = new smf_sbi();
  } catch (std::exception& e) {
    Logger::flexcn_app().error("Cannot create FLEXCN_APP: %s", e.what());
    throw;
  }

  //FLEXCN
  // Trigger SMFEventExpose subscription / to be noticed when a PDU session status changes
  trigger_pdu_session_status_notification_subscribe();

  if (smf_cfg.discover_upf) {
    // Trigger NFStatusNotify subscription to be noticed when a new UPF becomes
    // available (if this option is enabled)
    trigger_upf_status_notification_subscribe();
  }

  // Register to NRF (if this option is enabled)
  if (smf_cfg.register_nrf) {
    unsigned int microsecond = 10000;  // 10ms
    //usleep(microsecond);
    //register_to_nrf();
  }

  Logger::flexcn_app().startup("Started");
}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n4_session_modification_response& smresp) {

}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n4_session_deletion_response& smresp) {
  
}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(
    std::shared_ptr<itti_n4_session_report_request> snr) {
  
}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(std::shared_ptr<itti_n4_node_failure> snf) {
  
}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(
    itti_n11_n1n2_message_transfer_response_status& m) {

}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n11_update_pdu_session_status& m) {
  Logger::flexcn_app().info(
      "Set PDU Session Status to %s",
      pdu_session_status_e2str.at(static_cast<int>(m.pdu_session_status))
          .c_str());
  update_pdu_session_status(m.scid, m.pdu_session_status);
}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n11_create_sm_context_response& m) {

}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n11_update_sm_context_response& m) {

}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n11_release_sm_context_response& m) {
  
}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n11_register_nf_instance_response& r) {
  
}

//------------------------------------------------------------------------------
void flexcn_app::handle_itti_msg(itti_n11_update_nf_instance_response& u) {
  
}

//------------------------------------------------------------------------------
void flexcn_app::handle_pdu_session_update_sm_context_request(
    std::shared_ptr<itti_n11_update_sm_context_request> smreq) {
  
}
//------------------------------------------------------------------------------
void flexcn_app::handle_pdu_session_release_sm_context_request(
    std::shared_ptr<itti_n11_release_sm_context_request> smreq) {
  
}

//------------------------------------------------------------------------------
void flexcn_app::trigger_pdu_session_modification(
    const supi_t& supi, const std::string& dnn,
    const pdu_session_id_t pdu_session_id, const snssai_t& snssai,
    const pfcp::qfi_t& qfi, const uint8_t& http_version) {
  
}

//------------------------------------------------------------------------------
evsub_id_t flexcn_app::handle_event_exposure_subscription(
    std::shared_ptr<itti_sbi_event_exposure_request> msg) {
  
}

//------------------------------------------------------------------------------
bool flexcn_app::handle_nf_status_notification(
    std::shared_ptr<itti_sbi_notification_data>& msg,
    oai::smf_server::model::ProblemDetails& problem_details,
    uint8_t& http_code) {
  return true;
}

//------------------------------------------------------------------------------
bool flexcn_app::is_supi_2_smf_context(const supi64_t& supi) const {
  std::shared_lock lock(m_supi2smf_context);
  return bool{supi2smf_context.count(supi) > 0};
}

//------------------------------------------------------------------------------
std::shared_ptr<smf_context> flexcn_app::supi_2_smf_context(
    const supi64_t& supi) const {
  std::shared_lock lock(m_supi2smf_context);
  return supi2smf_context.at(supi);
}

//------------------------------------------------------------------------------
void flexcn_app::set_supi_2_smf_context(
    const supi64_t& supi, std::shared_ptr<smf_context> sc) {
  std::unique_lock lock(m_supi2smf_context);
  supi2smf_context[supi] = sc;
}

//------------------------------------------------------------------------------
void flexcn_app::set_scid_2_smf_context(
    const scid_t& id, std::shared_ptr<smf_context_ref> scf) {
  std::unique_lock lock(m_scid2smf_context);
  scid2smf_context[id] = scf;
}

//------------------------------------------------------------------------------
std::shared_ptr<smf_context_ref> flexcn_app::scid_2_smf_context(
    const scid_t& scid) const {
  std::shared_lock lock(m_scid2smf_context);
  return scid2smf_context.at(scid);
}

//------------------------------------------------------------------------------
bool flexcn_app::is_scid_2_smf_context(const scid_t& scid) const {
  std::shared_lock lock(m_scid2smf_context);
  return bool{scid2smf_context.count(scid) > 0};
}

//------------------------------------------------------------------------------
bool flexcn_app::is_scid_2_smf_context(
    const supi64_t& supi, const std::string& dnn, const snssai_t& snssai,
    const pdu_session_id_t& pid) const {
  std::shared_lock lock(m_scid2smf_context);
  for (auto it : scid2smf_context) {
    supi64_t supi64 = smf_supi_to_u64(it.second->supi);
    if ((supi64 == supi) and (it.second->dnn.compare(dnn) == 0) and
        (it.second->nssai == snssai) and (it.second->pdu_session_id == pid))
      return true;
  }
  return false;
}

//------------------------------------------------------------------------------
bool flexcn_app::scid_2_smf_context(
    const scid_t& scid, std::shared_ptr<smf_context_ref>& scf) const {
  std::shared_lock lock(m_scid2smf_context);
  if (scid2smf_context.count(scid) > 0) {
    scf = scid2smf_context.at(scid);
    return true;
  }
  return false;
}

//------------------------------------------------------------------------------
bool flexcn_app::use_local_configuration_subscription_data(
    const std::string& dnn_selection_mode) {
  // TODO: should be implemented
  return smf_cfg.use_local_subscription_info;
}

//------------------------------------------------------------------------------
bool flexcn_app::is_supi_dnn_snssai_subscription_data(
    const supi_t& supi, const std::string& dnn, const snssai_t& snssai) const {
  // TODO: should be implemented
  return false;  // Session Management Subscription from UDM isn't available
}

//------------------------------------------------------------------------------
bool flexcn_app::is_create_sm_context_request_valid() const {
  // TODO: should be implemented
  return true;
}

//---------------------------------------------------------------------------------------------
void flexcn_app::convert_string_2_hex(
    const std::string& input_str, std::string& output_str) {
  Logger::flexcn_app().debug("Convert string to Hex");
  unsigned char* data = (unsigned char*) malloc(input_str.length() + 1);
  memset(data, 0, input_str.length() + 1);
  memcpy((void*) data, (void*) input_str.c_str(), input_str.length());

#if DEBUG_IS_ON
  Logger::flexcn_app().debug("Input: ");
  for (int i = 0; i < input_str.length(); i++) {
    printf("%02x ", data[i]);
  }
  printf("\n");
#endif
  char* datahex = (char*) malloc(input_str.length() * 2 + 1);
  memset(datahex, 0, input_str.length() * 2 + 1);

  for (int i = 0; i < input_str.length(); i++)
    sprintf(datahex + i * 2, "%02x", data[i]);

  output_str = reinterpret_cast<char*>(datahex);
  Logger::flexcn_app().debug("Output: \n %s ", output_str.c_str());

  // free memory
  free_wrapper((void**) &data);
  free_wrapper((void**) &datahex);
}

//---------------------------------------------------------------------------------------------
void flexcn_app::update_pdu_session_status(
    const scid_t& scid, const pdu_session_status_e& status) {

}

//---------------------------------------------------------------------------------------------
void flexcn_app::update_pdu_session_upCnx_state(
    const scid_t& scid, const upCnx_state_e& state) {
  
}
//---------------------------------------------------------------------------------------------
void flexcn_app::timer_t3591_timeout(timer_id_t timer_id, uint64_t arg2_user) {
  // TODO: send session modification request again...
}

//---------------------------------------------------------------------------------------------
void flexcn_app::timer_nrf_heartbeat_timeout(
    timer_id_t timer_id, uint64_t arg2_user) {
  Logger::flexcn_app().debug("Send ITTI msg to N11 task to trigger NRF Heartbeat");

  std::shared_ptr<itti_n11_update_nf_instance_request> itti_msg =
      std::make_shared<itti_n11_update_nf_instance_request>(
          TASK_FLEXCN_APP, TASK_SMF_SBI);

  oai::smf_server::model::PatchItem patch_item = {};
  //{"op":"replace","path":"/nfStatus", "value": "REGISTERED"}
  patch_item.setOp("replace");
  patch_item.setPath("/nfStatus");
  patch_item.setValue("REGISTERED");
  itti_msg->patch_items.push_back(patch_item);
  itti_msg->smf_instance_id = smf_instance_id;

  int ret = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::flexcn_app().error(
        "Could not send ITTI message %s to task TASK_FLEXCN_SBI",
        itti_msg->get_msg_name());
  } else {
    Logger::flexcn_app().debug(
        "Set a timer to the next Heart-beat (%d)",
        nf_instance_profile.get_nf_heartBeat_timer());
    timer_nrf_heartbeat = itti_inst->timer_setup(
        nf_instance_profile.get_nf_heartBeat_timer(), 0, TASK_FLEXCN_APP,
        TASK_SMF_APP_TIMEOUT_NRF_HEARTBEAT,
        0);  // TODO arg2_user
  }
}

//---------------------------------------------------------------------------------------------
void flexcn_app::timer_nrf_deregistration(
    timer_id_t timer_id, uint64_t arg2_user) {
  Logger::flexcn_app().debug(
      "Send ITTI msg to N11 task to trigger NRF Deregistratino");
  trigger_nf_deregistration();
}

//---------------------------------------------------------------------------------------------
n2_sm_info_type_e flexcn_app::n2_sm_info_type_str2e(
    const std::string& n2_info_type) const {
  std::size_t number_of_types = n2_sm_info_type_e2str.size();
  for (auto i = 0; i < number_of_types; ++i) {
    if (n2_info_type.compare(n2_sm_info_type_e2str.at(i)) == 0) {
      return static_cast<n2_sm_info_type_e>(i);
    }
  }
}

//---------------------------------------------------------------------------------------------
bool flexcn_app::get_session_management_subscription_data(
    const supi64_t& supi, const std::string& dnn, const snssai_t& snssai,
    std::shared_ptr<session_management_subscription> subscription) {
  return true;
}

//---------------------------------------------------------------------------------------------
void flexcn_app::add_promise(
    uint32_t id,
    boost::shared_ptr<boost::promise<pdu_session_create_sm_context_response>>&
        p) {
  std::unique_lock lock(m_sm_context_create_promises);
  sm_context_create_promises.emplace(id, p);
}

//---------------------------------------------------------------------------------------------
void flexcn_app::add_promise(
    uint32_t id,
    boost::shared_ptr<boost::promise<pdu_session_update_sm_context_response>>&
        p) {
  std::unique_lock lock(m_sm_context_update_promises);
  sm_context_update_promises.emplace(id, p);
}

//---------------------------------------------------------------------------------------------
void flexcn_app::add_promise(
    uint32_t id,
    boost::shared_ptr<boost::promise<pdu_session_release_sm_context_response>>&
        p) {
  std::unique_lock lock(m_sm_context_release_promises);
  sm_context_release_promises.emplace(id, p);
}

//---------------------------------------------------------------------------------------------
void flexcn_app::trigger_create_context_error_response(
    const uint32_t& http_code, const uint8_t& cause,
    const std::string& n1_sm_msg, uint32_t& promise_id) {
  
}

//---------------------------------------------------------------------------------------------
void flexcn_app::trigger_update_context_error_response(
    const uint32_t& http_code, const uint8_t& cause, uint32_t& promise_id) {
  
}

//---------------------------------------------------------------------------------------------
void flexcn_app::trigger_update_context_error_response(
    const uint32_t& http_code, const uint8_t& cause,
    const std::string& n1_sm_msg, uint32_t& promise_id) {
  
}

//---------------------------------------------------------------------------------------------
void flexcn_app::trigger_http_response(
    const uint32_t& http_code, uint32_t& promise_id, uint8_t msg_type) {
}

//---------------------------------------------------------------------------------------------
void flexcn_app::generate_smf_profile() {
  // TODO: remove hardcoded values
  // generate UUID
  generate_uuid();
  nf_instance_profile.set_nf_instance_id(smf_instance_id);
  nf_instance_profile.set_nf_instance_name("OAI-FLEXCN");
  nf_instance_profile.set_nf_type("FLEXCN");
  nf_instance_profile.set_nf_status("REGISTERED");
  nf_instance_profile.set_nf_heartBeat_timer(50);
  nf_instance_profile.set_nf_priority(1);
  nf_instance_profile.set_nf_capacity(100);
  nf_instance_profile.add_nf_ipv4_addresses(smf_cfg.sbi.addr4);

  // NF services
  nf_service_t nf_service        = {};
  // HUNG-TODO: change the name of service to nflexcn 
  nf_service.service_instance_id = "nsmf-pdusession";
  nf_service.service_name        = "nsmf-pdusession";
  nf_service_version_t version   = {};
  version.api_version_in_uri     = "v1";
  version.api_full_version       = "1.0.0";  // TODO: to be updated
  nf_service.versions.push_back(version);
  nf_service.scheme            = "http";
  nf_service.nf_service_status = "REGISTERED";
  // IP Endpoint
  ip_endpoint_t endpoint = {};
  std::vector<struct in_addr> addrs;
  nf_instance_profile.get_nf_ipv4_addresses(addrs);
  endpoint.ipv4_address = addrs[0];  // TODO: use first IP ADDR for now
  endpoint.transport    = "TCP";
  endpoint.port         = smf_cfg.sbi.port;
  nf_service.ip_endpoints.push_back(endpoint);

  nf_instance_profile.add_nf_service(nf_service);

  // TODO: custom info

  int i = 0;
  for (auto sms : smf_cfg.session_management_subscription) {
    if (i < smf_cfg.num_session_management_subscription)
      i++;
    else
      break;

    // SNSSAIS
    snssai_t snssai = {};
    snssai.sD       = sms.single_nssai.sD;
    snssai.sST      = sms.single_nssai.sST;
    // Verify if this SNSSAI exist
    std::vector<snssai_t> ss = {};
    nf_instance_profile.get_nf_snssais(ss);
    bool found = false;
    for (auto it : ss) {
      if ((it.sD == snssai.sD) and (it.sST == snssai.sST)) {
        found = true;
        break;
      }
    }
    if (!found) nf_instance_profile.add_snssai(snssai);

    // SMF info
    dnn_smf_info_item_t dnn_item         = {.dnn = sms.dnn};
    snssai_smf_info_item_t smf_info_item = {};
    smf_info_item.dnn_smf_info_list.push_back(dnn_item);
    smf_info_item.snssai.sD  = sms.single_nssai.sD;
    smf_info_item.snssai.sST = sms.single_nssai.sST;
    nf_instance_profile.add_smf_info_item(smf_info_item);
  }

  // Display the profile
  nf_instance_profile.display();
}

//---------------------------------------------------------------------------------------------
void flexcn_app::register_to_nrf() {
  // Create a NF profile to this instance
  generate_smf_profile();
  // Send request to N11 to send NF registration to NRF
  trigger_nf_registration_request();
}

//------------------------------------------------------------------------------
void flexcn_app::generate_uuid() {
  smf_instance_id = to_string(boost::uuids::random_generator()());
}

//------------------------------------------------------------------------------
void flexcn_app::trigger_nf_registration_request() {
  Logger::flexcn_app().debug(
      "Send ITTI msg to N11 task to trigger the registration request to NRF");

  std::shared_ptr<itti_n11_register_nf_instance_request> itti_msg =
      std::make_shared<itti_n11_register_nf_instance_request>(
          TASK_FLEXCN_APP, TASK_SMF_SBI);
  itti_msg->profile = nf_instance_profile;
  int ret           = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::flexcn_app().error(
        "Could not send ITTI message %s to task TASK_FLEXCN_SBI",
        itti_msg->get_msg_name());
  }
}

//------------------------------------------------------------------------------
void flexcn_app::trigger_nf_deregistration() {
  Logger::flexcn_app().debug(
      "Send ITTI msg to N11 task to trigger the deregistration request to NRF");

  std::shared_ptr<itti_n11_deregister_nf_instance> itti_msg =
      std::make_shared<itti_n11_deregister_nf_instance>(
          TASK_FLEXCN_APP, TASK_SMF_SBI);
  itti_msg->smf_instance_id = smf_instance_id;
  int ret                   = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::flexcn_app().error(
        "Could not send ITTI message %s to task TASK_FLEXCN_SBI",
        itti_msg->get_msg_name());
  }
}

//------------------------------------------------------------------------------
void flexcn_app::trigger_upf_status_notification_subscribe() {
  
}

//------------------------------------------------------------------------------
void flexcn_app::trigger_pdu_session_status_notification_subscribe() {
  Logger::flexcn_app().debug(
      "Send ITTI msg to SBI task to subscribe to PDU Session Status notification "
      "from FLEXCN");

  std::shared_ptr<itti_n11_subscribe_pdu_session_status_notify> itti_msg =
      std::make_shared<itti_n11_subscribe_pdu_session_status_notify>(
          TASK_FLEXCN_APP, TASK_SMF_SBI);

  nlohmann::json json_data = {};
  json_data["notifUri"] =
      std::string(inet_ntoa(*((struct in_addr*) &smf_cfg.sbi.addr4))) + ":" +
      std::to_string(smf_cfg.sbi.port) + "/flexcn-status-notify/v1/notifid01";

  json_data["notifId"]                     = "notifid01";

  json_data["supi"] = "208950000000031";
  json_data["eventSubs"]  = nlohmann::json::array();
  nlohmann::json  tmp ={};
  tmp["event"] = "PDU_SES_REL";

  nlohmann::json  tmp2 ={};
  tmp2["event"] = "UE_IP_CH";
  
  nlohmann::json  tmp3 ={};
  tmp3["event"] = "DDDS";
  
  nlohmann::json  tmp4 ={};
  tmp4["event"] = "PLMN_CH";

  nlohmann::json  tmp5 ={};
  tmp5["event"] = "FLEXCN";

  json_data["eventSubs"].push_back(tmp);
  json_data["eventSubs"].push_back(tmp2);
  json_data["eventSubs"].push_back(tmp3);
  json_data["eventSubs"].push_back(tmp4);
  json_data["eventSubs"].push_back(tmp5);

  std::string url =
      std::string(inet_ntoa(*((struct in_addr*) &smf_cfg.nrf_addr.ipv4_addr))) +
      ":" + std::to_string(smf_cfg.nrf_addr.port) + "/nsmf_event-exposure/v1/subscriptions";

  itti_msg->url       = url;
  itti_msg->json_data = json_data;
  Logger::flexcn_app().info(json_data.dump().c_str());
  int ret             = itti_inst->send_msg(itti_msg);
  if (RETURNok != ret) {
    Logger::flexcn_app().error(
        "Could not send ITTI message %s to task TASK_FLEXCN_SBI",
        itti_msg->get_msg_name());
  }
}
