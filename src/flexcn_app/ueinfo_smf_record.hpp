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

/*! \file flexcn_data_item.hpp
 \brief
 \author  Hung NGUYEN
 \company Eurecom
 \date 2021
 \email: hnguyen@eurecom.fr
 */

#ifndef FILE_FLEXCN_DATA_ITEM_HPP_SEEN
#define FILE_FLEXCN_DATA_ITEM_HPP_SEEN


#include "DnaiChangeType.h"
#include "DddStatus.h"
#include <string>
#include "Ipv6Prefix.h"
#include "SmfEvent.h"
#include "PlmnId.h"
#include "RouteToLocation.h"
#include "AccessType.h"
#include <nlohmann/json.hpp>
#include "EventNotification.h"

// #include "3gpp_129.244.h"
#include "3gpp_24.501.h"
#include "common_root_types.h"
#include "3gpp_29.244.h"



using namespace oai::flexcn_server::model;

class CNRecord {
 public:
  CNRecord();
  virtual ~CNRecord();

  /////////////////////////////////////////////
  /// CNRecord members

  void merge(const EventNotification& event_data);
  void mergeAccessTypeChange(const EventNotification& event_data);
  void mergeUpPathChange(const EventNotification& event_data);
  void mergePDUSessRelease(const EventNotification& event_data);
  void mergePLMNChange(const EventNotification& event_data);
  void mergeUEIPChange(const EventNotification& event_data);
  void mergeDDDS(const EventNotification& event_data);

  /// <summary>
  ///
  /// </summary>
  SmfEvent getEvent() const;
  void setEvent(SmfEvent const& value);
  /// <summary>
  ///
  /// </summary>
  std::string getTimeStamp() const;
  void setTimeStamp(std::string const& value);
  /// <summary>
  ///
  /// </summary>
  std::string getSupi() const;
  void setSupi(std::string const& value);
  bool supiIsSet() const;
  void unsetSupi();
  /// <summary>
  ///
  /// </summary>
  std::string getGpsi() const;
  void setGpsi(std::string const& value);
  bool gpsiIsSet() const;
  void unsetGpsi();
  /// <summary>
  ///
  /// </summary>
  std::string getSourceDnai() const;
  void setSourceDnai(std::string const& value);
  bool sourceDnaiIsSet() const;
  void unsetSourceDnai();
  /// <summary>
  ///
  /// </summary>
  std::string getTargetDnai() const;
  void setTargetDnai(std::string const& value);
  bool targetDnaiIsSet() const;
  void unsetTargetDnai();
  /// <summary>
  ///
  /// <summary>
  ///
  /// </summary>
  std::string getSourceUeIpv4Addr() const;
  void setSourceUeIpv4Addr(std::string const& value);
  bool sourceUeIpv4AddrIsSet() const;
  void unsetSourceUeIpv4Addr();
  /// <summary>
  ///
  /// </summary>
  Ipv6Prefix getSourceUeIpv6Prefix() const;
  void setSourceUeIpv6Prefix(Ipv6Prefix const& value);
  bool sourceUeIpv6PrefixIsSet() const;
  void unsetSourceUeIpv6Prefix();
  /// <summary>
  ///
  /// </summary>
  std::string getTargetUeIpv4Addr() const;
  void setTargetUeIpv4Addr(std::string const& value);
  bool targetUeIpv4AddrIsSet() const;
  void unsetTargetUeIpv4Addr();
  /// <summary>
  ///
  /// </summary>
  Ipv6Prefix getTargetUeIpv6Prefix() const;
  void setTargetUeIpv6Prefix(Ipv6Prefix const& value);
  bool targetUeIpv6PrefixIsSet() const;
  void unsetTargetUeIpv6Prefix();
  /// <summary>
  ///
  /// </summary>
  RouteToLocation getTargetTraRouting() const;
  void setTargetTraRouting(RouteToLocation const& value);
  bool targetTraRoutingIsSet() const;
  void unsetTargetTraRouting();
  /// <summary>
  ///
  /// </summary>
  std::string getUeMac() const;
  void setUeMac(std::string const& value);
  bool ueMacIsSet() const;
  void unsetUeMac();
  /// <summary>
  ///
  /// </summary>
  std::string getAdIpv4Addr() const;
  void setAdIpv4Addr(std::string const& value);
  bool adIpv4AddrIsSet() const;
  void unsetAdIpv4Addr();
  /// <summary>
  ///
  /// </summary>
  Ipv6Prefix getAdIpv6Prefix() const;
  void setAdIpv6Prefix(Ipv6Prefix const& value);
  bool adIpv6PrefixIsSet() const;
  void unsetAdIpv6Prefix();
  /// <summary>
  ///
  /// </summary>
  Ipv6Prefix getReIpv6Prefix() const;
  void setReIpv6Prefix(Ipv6Prefix const& value);
  bool reIpv6PrefixIsSet() const;
  void unsetReIpv6Prefix();
  /// <summary>
  ///
  /// </summary>
  PlmnId getPlmnId() const;
  void setPlmnId(PlmnId const& value);
  bool plmnIdIsSet() const;
  void unsetPlmnId();
  /// <summary>
  ///
  /// </summary>
  DddStatus getDddStatus() const;
  void setDddStatus(DddStatus const& value);
  bool dddStatusIsSet() const;
  void unsetDddStatus();

  /// <summary>
  ///
  /// </summary>
  std::string getMaxWaitTime() const;
  void setMaxWaitTime(std::string const& value);
  bool maxWaitTimeIsSet() const;
  void unsetMaxWaitTime();

  // ===========================
  // Hung_TODO: extra fields that are important
  
  std::string getDnn() const;
  void setDnn(std::string const& value);
  bool dnnIsSet() const;
  void unsetDnn();

  std::string getAmfStatusUri() const;
  void setAmfStatusUri(std::string const& value);
  bool amfStatusUriIsSet() const;
  void unsetAmfStatusUri();

  // done

  // amf_addr
  std::string getAmfAddr() const;
  void setAmfAddr(std::string const& value);
  bool amfAddrIsSet() const;
  void unsetAmfAddr();

  // upf_node_id
  pfcp::node_id_t getUPFNodeId() const;
  void setUPFNodeId(pfcp::node_id_t const& value);
  bool upfNodeIdIsSet() const;
  void unsetUPFNodeId();

  pdu_session_type_t getPduSeType() const;
  void setPduSeType(pdu_session_type_t const& value);
  bool pduSeTypeIsSet() const;
  void unsetPduSeType();

  // seid, 
  seid_t getSeid() const;
  void setSeid(seid_t const& value);
  bool seidIsSet() const;
  void unsetSeid();

  // up_fseid,
  pfcp::fseid_t getUpFseid() const;
  void setUpFseid(pfcp::fseid_t const& value);
  bool upFseidIsSet() const;
  void unsetUpFseid();

  // amf_id, 
  std::string getAmfId() const;
  void setAmfId(std::string const& value);
  bool amfIdIsSet() const;
  void unsetAmfId();

  //pduseid
  int32_t getPduSeId() const;
  void setPduSeId(int32_t const value);
  bool pduSeIdIsSet() const;
  void unsetPduSeId();

  // Dl_fteid [SMF]  
  pfcp::fteid_t getDlFteid() const;
  void setDlFteid(pfcp::fteid_t const& value);
  bool dlFteidIsSet() const;
  void unsetDlFteid();

  //Ul_fteid [SMF]
  pfcp::fteid_t getUlFteid() const;
  void setUlFteid(pfcp::fteid_t const& value);
  bool ulFteidIsSet() const;
  void unsetUlFteid();

  // eNB/gNB/CU IP@
  std::string getRanId() const;
  void setRanId(int32_t const value);
  bool ranIdIsSet() const;
  void unsetRanId();


  // ===========================

  friend void to_json(nlohmann::json& j, const CNRecord& o);
  friend void from_json(const nlohmann::json& j, CNRecord& o);

 protected:
  SmfEvent m_Event;

  std::string m_TimeStamp;

  std::string m_Supi;
  bool m_SupiIsSet;
  std::string m_Gpsi;
  bool m_GpsiIsSet;
  std::string m_SourceDnai;
  bool m_SourceDnaiIsSet;
  std::string m_TargetDnai;
  bool m_TargetDnaiIsSet;
  std::string m_SourceUeIpv4Addr;
  bool m_SourceUeIpv4AddrIsSet;
  Ipv6Prefix m_SourceUeIpv6Prefix;
  bool m_SourceUeIpv6PrefixIsSet;
  std::string m_TargetUeIpv4Addr;
  bool m_TargetUeIpv4AddrIsSet;
  Ipv6Prefix m_TargetUeIpv6Prefix;
  bool m_TargetUeIpv6PrefixIsSet;
  RouteToLocation m_TargetTraRouting;
  bool m_TargetTraRoutingIsSet;
  std::string m_UeMac;
  bool m_UeMacIsSet;
  std::string m_AdIpv4Addr;
  bool m_AdIpv4AddrIsSet;
  Ipv6Prefix m_AdIpv6Prefix;
  bool m_AdIpv6PrefixIsSet;
  Ipv6Prefix m_ReIpv6Prefix;
  bool m_ReIpv6PrefixIsSet;
  PlmnId m_PlmnId;
  bool m_PlmnIdIsSet;
  DddStatus m_DddStatus;
  bool m_DddStatusIsSet;
  std::string m_MaxWaitTime;
  bool m_MaxWaitTimeIsSet;
  int32_t m_PduSeId;
  bool m_PduSeIdIsSet;

  // ===========================
  // Hung_TODO: extra fields that are important
  std::string m_Dnn;
  bool m_DnnIsSet;
  
  std::string m_AmfStatusUri;
  bool m_AmfStatusUriIsSet;

  std::string m_AmfAddr;
  bool m_AmfAddrIsSet;

  pfcp::node_id_t m_UpfNodeId;
  bool m_UpfNodeIdIsSet;

  pdu_session_type_t m_PduSeType;
  bool m_PduSeTypeIsSet;
  
  // seid,
  seid_t m_seid;
  bool m_seidIsSet;

  // up_fseid,
  pfcp::fseid_t m_UpFseid;
  bool m_UpFseidIsSet;

  // amf_id, 
  std::string m_AmfId;
  bool m_AmfIdIsSet;

  // Dl_fteid [SMF]  
  pfcp::fteid_t m_DlFteid;
  bool m_DlFteidIsSet;

  //Ul_fteid [SMF]
  pfcp::fteid_t m_UlFteid;
  bool m_UlFteidIsSet;

  // eNB/gNB/CU IP@
  std::string m_RanIP;
  bool m_RanIPIsSet;


 //Hung_TODO: flow id and QFI is this the same
  // ===========================
};

#endif /* FILE_FLEXCN_HPP_SEEN */