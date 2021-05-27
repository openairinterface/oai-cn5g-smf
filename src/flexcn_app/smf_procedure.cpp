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

/*! \file smf_procedure.cpp
 \author  Lionel GAUTHIER, Tien-Thinh NGUYEN
 \company Eurecom
 \date 2019
 \email: lionel.gauthier@eurecom.fr, tien-thinh.nguyen@eurecom.fr
 */

#include "smf_procedure.hpp"

#include <algorithm>  // std::search

#include "3gpp_29.244.h"
#include "3gpp_29.500.h"
#include "3gpp_29.502.h"
#include "3gpp_conversions.hpp"
#include "SmContextCreatedData.h"
#include "common_defs.h"
#include "conversions.hpp"
#include "itti.hpp"
#include "itti_msg_n4_restore.hpp"
#include "logger.hpp"
#include "smf_app.hpp"
#include "smf_config.hpp"
#include "smf_context.hpp"
#include "smf_sbi.hpp"
#include "smf_pfcp_association.hpp"
#include "ProblemDetails.h"
#include "3gpp_24.501.h"

using namespace pfcp;
using namespace smf;
using namespace std;

extern itti_mw* itti_inst;
extern smf::flexcn_app* flexcn_app_inst;
extern smf::smf_config smf_cfg;

//------------------------------------------------------------------------------
int n4_session_restore_procedure::run() {
  return RETURNok;
}

//------------------------------------------------------------------------------
int session_create_sm_context_procedure::run(
    std::shared_ptr<itti_n11_create_sm_context_request> sm_context_req,
    std::shared_ptr<itti_n11_create_sm_context_response> sm_context_resp,
    std::shared_ptr<smf::smf_context> sc) {
  return RETURNok;
}

//------------------------------------------------------------------------------
void session_create_sm_context_procedure::handle_itti_msg(
    itti_n4_session_establishment_response& resp,
    std::shared_ptr<smf::smf_context> sc) {

}

//------------------------------------------------------------------------------
int session_update_sm_context_procedure::run(
    std::shared_ptr<itti_n11_update_sm_context_request> sm_context_req,
    std::shared_ptr<itti_n11_update_sm_context_response> sm_context_resp,
    std::shared_ptr<smf::smf_context> sc) {
    return RETURNok;
}

//------------------------------------------------------------------------------
void session_update_sm_context_procedure::handle_itti_msg(
    itti_n4_session_modification_response& resp,
    std::shared_ptr<smf::smf_context> sc) {

}

//------------------------------------------------------------------------------
int session_release_sm_context_procedure::run(
    std::shared_ptr<itti_n11_release_sm_context_request> sm_context_req,
    std::shared_ptr<itti_n11_release_sm_context_response> sm_context_res,
    std::shared_ptr<smf::smf_context> sc) {
  return RETURNok;
}

//------------------------------------------------------------------------------
void session_release_sm_context_procedure::handle_itti_msg(
    itti_n4_session_deletion_response& resp,
    std::shared_ptr<smf::smf_context> sc) {
  
}
