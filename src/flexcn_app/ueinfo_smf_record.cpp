
#include "ueinfo_smf_record.hpp"
#include "SmfEvent.h"

CNRecord::CNRecord() {
  m_TimeStamp               = "";
  m_Supi                    = "";
  m_SupiIsSet               = false;
  m_Gpsi                    = "";
  m_GpsiIsSet               = false;
  m_SourceDnai              = "";
  m_SourceDnaiIsSet         = false;
  m_TargetDnai              = "";
  m_TargetDnaiIsSet         = false;
  m_SourceUeIpv4Addr        = "";
  m_SourceUeIpv4AddrIsSet   = false;
  m_TargetUeIpv4Addr        = "";
  m_TargetUeIpv4AddrIsSet   = false;
  m_TargetTraRoutingIsSet   = false;
  m_UeMac                   = "";
  m_UeMacIsSet              = false;
  m_AdIpv4Addr              = "";
  m_AdIpv4AddrIsSet         = false;
  m_AdIpv6PrefixIsSet       = false;
  m_ReIpv6PrefixIsSet       = false;
  m_PlmnIdIsSet             = false;
  m_PduSeId                 = 0;
  m_PduSeIdIsSet            = false;
  m_DddStatusIsSet          = false;
  m_MaxWaitTime             = "";
  m_MaxWaitTimeIsSet        = false;
}

CNRecord::~CNRecord() {}


void CNRecord::merge(const EventNotification& event_data){
    // get the event 
    std::string event_type = event_data.getEvent().event;
    if (event_type == "AC_TY_CH") {
        mergeAccessTypeChange(event_data);
    }
    else if (event_type == "UP_PATH_CH") {
        mergeUpPathChange(event_data);
    }
    else if (event_type == "PDU_SES_REL") {
        mergePDUSessRelease(event_data);
    }
    else if (event_type == "PLMN_CH") {
        mergePLMNChange(event_data);
    }
    else if (event_type == "UE_IP_CH") {
        mergeUEIPChange(event_data);
    }
    else if (event_type == "DDDS") {
        mergeDDDS(event_data);
    }
    else {
      Logger::smf_app().info("Event from SMF not supported");
    }
}

void CNRecord::mergeAccessTypeChange(const EventNotification& event_data){
  Logger::smf_app().info("Merge with the access type change event: NOT SUPPORTED");
}

void CNRecord::mergeUpPathChange(const EventNotification& event_data){
  Logger::smf_app().info("Merge with the up path change event: NOT SUPPORTED");
}

void CNRecord::mergePDUSessRelease(const EventNotification& event_data){
  Logger::smf_app().info("Merge with pdu sess release event");
  if (event_data.pduSeIdIsSet()){
    m_PduSeId = event_data.getPduSeId();
  }
}

void CNRecord::mergePLMNChange(const EventNotification& event_data){
  Logger::smf_app().info("Merge with the PLMN change event");
  if (event_data.plmnIdIsSet()){
    m_PlmnId = event_data.getPlmnId();
  }
}

void CNRecord::mergeUEIPChange(const EventNotification& event_data){
  Logger::smf_app().info("Merge with the UE IP change event");
  if (event_data.adIpv4AddrIsSet()){
    m_AdIpv4Addr = event_data.getAdIpv4Addr();
  }
}

void CNRecord::mergeDDDS(const EventNotification& event_data){
  Logger::smf_app().info("Merge with the DDDS change event");
  if (event_data.dddStatusIsSet()){
    m_DddStatus = event_data.getDddStatus();
  }
}

// Hung_TODO: generate an example of json for Navid to decide 
// which format is the best.
void to_json(nlohmann::json& j, const CNRecord& o) {
  j              = nlohmann::json();
  j["event"]     = o.m_Event;
  j["timeStamp"] = o.m_TimeStamp;
  if (o.supiIsSet()) j["supi"] = o.m_Supi;
  if (o.gpsiIsSet()) j["gpsi"] = o.m_Gpsi;
  if (o.sourceDnaiIsSet()) j["sourceDnai"] = o.m_SourceDnai;
  if (o.targetDnaiIsSet()) j["targetDnai"] = o.m_TargetDnai;
  if (o.sourceUeIpv4AddrIsSet()) j["sourceUeIpv4Addr"] = o.m_SourceUeIpv4Addr;
  if (o.targetUeIpv4AddrIsSet()) j["targetUeIpv4Addr"] = o.m_TargetUeIpv4Addr;
  if (o.targetTraRoutingIsSet()) j["targetTraRouting"] = o.m_TargetTraRouting;
  if (o.ueMacIsSet()) j["ueMac"] = o.m_UeMac;
  if (o.adIpv4AddrIsSet()) j["adIpv4Addr"] = o.m_AdIpv4Addr;
  if (o.adIpv6PrefixIsSet()) j["adIpv6Prefix"] = o.m_AdIpv6Prefix;
  if (o.reIpv6PrefixIsSet()) j["reIpv6Prefix"] = o.m_ReIpv6Prefix;
  if (o.plmnIdIsSet()) j["plmnId"] = o.m_PlmnId;
  if (o.pduSeIdIsSet()) j["pduSeId"] = o.m_PduSeId;
  if (o.dddStatusIsSet()) j["dddStatus"] = o.m_DddStatus;
  if (o.maxWaitTimeIsSet()) j["maxWaitTime"] = o.m_MaxWaitTime;
}

void from_json(const nlohmann::json& j, CNRecord& o) {
  j.at("event").get_to(o.m_Event);
  j.at("timeStamp").get_to(o.m_TimeStamp);
  if (j.find("supi") != j.end()) {
    j.at("supi").get_to(o.m_Supi);
    o.m_SupiIsSet = true;
  }
  if (j.find("gpsi") != j.end()) {
    j.at("gpsi").get_to(o.m_Gpsi);
    o.m_GpsiIsSet = true;
  }
  if (j.find("sourceDnai") != j.end()) {
    j.at("sourceDnai").get_to(o.m_SourceDnai);
    o.m_SourceDnaiIsSet = true;
  }
  if (j.find("targetDnai") != j.end()) {
    j.at("targetDnai").get_to(o.m_TargetDnai);
    o.m_TargetDnaiIsSet = true;
  }
  if (j.find("sourceUeIpv4Addr") != j.end()) {
    j.at("sourceUeIpv4Addr").get_to(o.m_SourceUeIpv4Addr);
    o.m_SourceUeIpv4AddrIsSet = true;
  }
  if (j.find("targetUeIpv4Addr") != j.end()) {
    j.at("targetUeIpv4Addr").get_to(o.m_TargetUeIpv4Addr);
    o.m_TargetUeIpv4AddrIsSet = true;
  }
  if (j.find("targetTraRouting") != j.end()) {
    j.at("targetTraRouting").get_to(o.m_TargetTraRouting);
    o.m_TargetTraRoutingIsSet = true;
  }
  if (j.find("ueMac") != j.end()) {
    j.at("ueMac").get_to(o.m_UeMac);
    o.m_UeMacIsSet = true;
  }
  if (j.find("adIpv4Addr") != j.end()) {
    j.at("adIpv4Addr").get_to(o.m_AdIpv4Addr);
    o.m_AdIpv4AddrIsSet = true;
  }
  if (j.find("adIpv6Prefix") != j.end()) {
    j.at("adIpv6Prefix").get_to(o.m_AdIpv6Prefix);
    o.m_AdIpv6PrefixIsSet = true;
  }
  if (j.find("reIpv6Prefix") != j.end()) {
    j.at("reIpv6Prefix").get_to(o.m_ReIpv6Prefix);
    o.m_ReIpv6PrefixIsSet = true;
  }
  if (j.find("plmnId") != j.end()) {
    j.at("plmnId").get_to(o.m_PlmnId);
    o.m_PlmnIdIsSet = true;
  }
  if (j.find("pduSeId") != j.end()) {
    j.at("pduSeId").get_to(o.m_PduSeId);
    o.m_PduSeIdIsSet = true;
  }
  if (j.find("dddStatus") != j.end()) {
    j.at("dddStatus").get_to(o.m_DddStatus);
    o.m_DddStatusIsSet = true;
  }
  if (j.find("maxWaitTime") != j.end()) {
    j.at("maxWaitTime").get_to(o.m_MaxWaitTime);
    o.m_MaxWaitTimeIsSet = true;
  }
}


std::string CNRecord::getDnn() const{
    return m_Dnn;
}

void CNRecord::setDnn(std::string const& value){
    m_Dnn = value;
}
bool CNRecord::dnnIsSet() const{
    return m_DnnIsSet;
}
void CNRecord::unsetDnn(){
    m_DnnIsSet = false;
}

std::string CNRecord::getAmfStatusUri() const{
    return m_AmfStatusUri;
}

void CNRecord::setAmfStatusUri(std::string const& value){
    m_AmfStatusUri = value;
}

bool CNRecord::amfStatusUriIsSet() const{
    return m_AmfStatusUriIsSet;
}

void CNRecord::unsetAmfStatusUri() {
    m_AmfStatusUri = false;
}

std::string CNRecord::getAmfAddr() const {
    return m_AmfAddr;
}

void CNRecord::setAmfAddr(std::string const& value){
    m_AmfAddr = value;
}

bool CNRecord::amfAddrIsSet() const{
   return m_AmfAddrIsSet;
}
void CNRecord::unsetAmfAddr(){
    m_AmfAddrIsSet = false;
}

// upf_node_id
pfcp::node_id_t CNRecord::getUPFNodeId() const {
    return m_UpfNodeId;
}

void CNRecord::setUPFNodeId(pfcp::node_id_t const& value){
    m_UpfNodeId = value;
}
  
bool CNRecord::upfNodeIdIsSet() const{
    return m_UpfNodeIdIsSet;
}

void CNRecord::unsetUPFNodeId(){
    m_UpfNodeIdIsSet = false;
}

pdu_session_type_t CNRecord::getPduSeType() const{
    return m_PduSeType; 
}
void CNRecord::setPduSeType(pdu_session_type_t const& value){
    m_PduSeType = value;
}
bool CNRecord::pduSeTypeIsSet() const{
    return m_PduSeTypeIsSet;
}
void CNRecord::unsetPduSeType(){
    m_PduSeTypeIsSet = false;
}

// seid 
seid_t CNRecord::getSeid() const{
    return m_seid; 
}
void CNRecord::setSeid(seid_t const& value){
    m_seid = value;
}
bool CNRecord::seidIsSet() const{
    return m_seidIsSet;
}
void CNRecord::unsetSeid(){
    m_seidIsSet = false;
}

// up_fseid,
pfcp::fseid_t CNRecord::getUpFseid() const{
    return m_UpFseid;
}
void CNRecord::setUpFseid(pfcp::fseid_t const& value){
    m_UpFseid = value;
}
bool CNRecord::upFseidIsSet() const{
 return m_UpFseidIsSet;
}
void CNRecord::unsetUpFseid(){
    m_UpFseidIsSet = false;
}
 
// amf_id, 
std::string CNRecord::getAmfId() const{
    return m_AmfId;
}

void CNRecord::setAmfId(std::string const& value){
    m_AmfId = value;
}

bool CNRecord::amfIdIsSet() const{
return m_AmfIdIsSet;
}
void CNRecord::unsetAmfId(){
    m_AmfIdIsSet = false;
}

// Dl_fteid [SMF]  
pfcp::fteid_t CNRecord::getDlFteid() const{
    return m_DlFteid;
}
void CNRecord::setDlFteid(pfcp::fteid_t const& value){
    m_DlFteid = value;
}
bool CNRecord::dlFteidIsSet() const{
    return m_DlFteidIsSet;
}
void CNRecord::unsetDlFteid(){
    m_DlFteidIsSet = false;
}

// Ul_fteid [SMF]  
pfcp::fteid_t CNRecord::getUlFteid() const{
    return m_UlFteid;
}
void CNRecord::setUlFteid(pfcp::fteid_t const& value){
    m_UlFteid = value;
}
bool CNRecord::ulFteidIsSet() const{
    return m_UlFteidIsSet;
}
void CNRecord::unsetUlFteid(){
    m_UlFteidIsSet = false;
}

// eNB/gNB/CU IP@
std::string CNRecord::getRanId() const{
   return m_RanIP;
}
void CNRecord::setRanId(int32_t const value){
   m_RanIP = value;
}
bool CNRecord::ranIdIsSet() const{
   return m_RanIPIsSet;
}
void CNRecord::unsetRanId(){
   m_RanIPIsSet = false;
}

SmfEvent CNRecord::getEvent() const {
  return m_Event;
}
void CNRecord::setEvent(SmfEvent const& value) {
  m_Event = value;
}
std::string CNRecord::getTimeStamp() const {
  return m_TimeStamp;
}
void CNRecord::setTimeStamp(std::string const& value) {
  m_TimeStamp = value;
}
std::string CNRecord::getSupi() const {
  return m_Supi;
}
void CNRecord::setSupi(std::string const& value) {
  m_Supi      = value;
  m_SupiIsSet = true;
}
bool CNRecord::supiIsSet() const {
  return m_SupiIsSet;
}
void CNRecord::unsetSupi() {
  m_SupiIsSet = false;
}
std::string CNRecord::getGpsi() const {
  return m_Gpsi;
}
void CNRecord::setGpsi(std::string const& value) {
  m_Gpsi      = value;
  m_GpsiIsSet = true;
}
bool CNRecord::gpsiIsSet() const {
  return m_GpsiIsSet;
}
void CNRecord::unsetGpsi() {
  m_GpsiIsSet = false;
}
std::string CNRecord::getSourceDnai() const {
  return m_SourceDnai;
}
void CNRecord::setSourceDnai(std::string const& value) {
  m_SourceDnai      = value;
  m_SourceDnaiIsSet = true;
}
bool CNRecord::sourceDnaiIsSet() const {
  return m_SourceDnaiIsSet;
}
void CNRecord::unsetSourceDnai() {
  m_SourceDnaiIsSet = false;
}
std::string CNRecord::getTargetDnai() const {
  return m_TargetDnai;
}
void CNRecord::setTargetDnai(std::string const& value) {
  m_TargetDnai      = value;
  m_TargetDnaiIsSet = true;
}
bool CNRecord::targetDnaiIsSet() const {
  return m_TargetDnaiIsSet;
}
void CNRecord::unsetTargetDnai() {
  m_TargetDnaiIsSet = false;
}
std::string CNRecord::getSourceUeIpv4Addr() const {
  return m_SourceUeIpv4Addr;
}
void CNRecord::setSourceUeIpv4Addr(std::string const& value) {
  m_SourceUeIpv4Addr      = value;
  m_SourceUeIpv4AddrIsSet = true;
}
bool CNRecord::sourceUeIpv4AddrIsSet() const {
  return m_SourceUeIpv4AddrIsSet;
}
void CNRecord::unsetSourceUeIpv4Addr() {
  m_SourceUeIpv4AddrIsSet = false;
}
std::string CNRecord::getTargetUeIpv4Addr() const {
  return m_TargetUeIpv4Addr;
}
void CNRecord::setTargetUeIpv4Addr(std::string const& value) {
  m_TargetUeIpv4Addr      = value;
  m_TargetUeIpv4AddrIsSet = true;
}
bool CNRecord::targetUeIpv4AddrIsSet() const {
  return m_TargetUeIpv4AddrIsSet;
}
void CNRecord::unsetTargetUeIpv4Addr() {
  m_TargetUeIpv4AddrIsSet = false;
}
RouteToLocation CNRecord::getTargetTraRouting() const {
  return m_TargetTraRouting;
}
void CNRecord::setTargetTraRouting(RouteToLocation const& value) {
  m_TargetTraRouting      = value;
  m_TargetTraRoutingIsSet = true;
}
bool CNRecord::targetTraRoutingIsSet() const {
  return m_TargetTraRoutingIsSet;
}
void CNRecord::unsetTargetTraRouting() {
  m_TargetTraRoutingIsSet = false;
}
std::string CNRecord::getUeMac() const {
  return m_UeMac;
}
void CNRecord::setUeMac(std::string const& value) {
  m_UeMac      = value;
  m_UeMacIsSet = true;
}
bool CNRecord::ueMacIsSet() const {
  return m_UeMacIsSet;
}
void CNRecord::unsetUeMac() {
  m_UeMacIsSet = false;
}
std::string CNRecord::getAdIpv4Addr() const {
  return m_AdIpv4Addr;
}
void CNRecord::setAdIpv4Addr(std::string const& value) {
  m_AdIpv4Addr      = value;
  m_AdIpv4AddrIsSet = true;
}
bool CNRecord::adIpv4AddrIsSet() const {
  return m_AdIpv4AddrIsSet;
}
void CNRecord::unsetAdIpv4Addr() {
  m_AdIpv4AddrIsSet = false;
}
Ipv6Prefix CNRecord::getAdIpv6Prefix() const {
  return m_AdIpv6Prefix;
}
void CNRecord::setAdIpv6Prefix(Ipv6Prefix const& value) {
  m_AdIpv6Prefix      = value;
  m_AdIpv6PrefixIsSet = true;
}
bool CNRecord::adIpv6PrefixIsSet() const {
  return m_AdIpv6PrefixIsSet;
}
void CNRecord::unsetAdIpv6Prefix() {
  m_AdIpv6PrefixIsSet = false;
}
Ipv6Prefix CNRecord::getReIpv6Prefix() const {
  return m_ReIpv6Prefix;
}
void CNRecord::setReIpv6Prefix(Ipv6Prefix const& value) {
  m_ReIpv6Prefix      = value;
  m_ReIpv6PrefixIsSet = true;
}
bool CNRecord::reIpv6PrefixIsSet() const {
  return m_ReIpv6PrefixIsSet;
}
void CNRecord::unsetReIpv6Prefix() {
  m_ReIpv6PrefixIsSet = false;
}
PlmnId CNRecord::getPlmnId() const {
  return m_PlmnId;
}
void CNRecord::setPlmnId(PlmnId const& value) {
  m_PlmnId      = value;
  m_PlmnIdIsSet = true;
}
bool CNRecord::plmnIdIsSet() const {
  return m_PlmnIdIsSet;
}
void CNRecord::unsetPlmnId() {
  m_PlmnIdIsSet = false;
}
int32_t CNRecord::getPduSeId() const {
  return m_PduSeId;
}
void CNRecord::setPduSeId(int32_t const value) {
  m_PduSeId      = value;
  m_PduSeIdIsSet = true;
}
bool CNRecord::pduSeIdIsSet() const {
  return m_PduSeIdIsSet;
}
void CNRecord::unsetPduSeId() {
  m_PduSeIdIsSet = false;
}
DddStatus CNRecord::getDddStatus() const {
  return m_DddStatus;
}
void CNRecord::setDddStatus(DddStatus const& value) {
  m_DddStatus      = value;
  m_DddStatusIsSet = true;
}
bool CNRecord::dddStatusIsSet() const {
  return m_DddStatusIsSet;
}
void CNRecord::unsetDddStatus() {
  m_DddStatusIsSet = false;
}
std::string CNRecord::getMaxWaitTime() const {
  return m_MaxWaitTime;
}
void CNRecord::setMaxWaitTime(std::string const& value) {
  m_MaxWaitTime      = value;
  m_MaxWaitTimeIsSet = true;
}
bool CNRecord::maxWaitTimeIsSet() const {
  return m_MaxWaitTimeIsSet;
}
void CNRecord::unsetMaxWaitTime() {
  m_MaxWaitTimeIsSet = false;
}
