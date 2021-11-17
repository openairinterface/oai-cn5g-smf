
#ifndef DnnConfiguration_H_
#define DnnConfiguration_H_

#include <nlohmann/json.hpp>

#include "Snssai.h"

namespace oai {
namespace smf_server {
namespace model {

class DnnConfigurationMessage {
 public:
  DnnConfigurationMessage();
  virtual ~DnnConfigurationMessage();

  void validate();

  std::string getDnn() const;
  void setDnn(std::string const& value);

  Snssai getSnssai() const;
  void setSnssai(Snssai const& value);

  std::string getSupi() const;
  void setSupi(std::string const& value);
  bool supiIsSet() const;

  std::string getPduSessionType() const;
  void setPduSessionType(std::string const& value);
  bool pduSessionTypeIsSet() const;

  std::string getIpv4Range() const;
  void setIpv4Range(std::string const& value);
  bool ipv4RangeIsSet() const;

  std::string getIpv6Prefix() const;
  void setIpv6Prefix(std::string const& value);
  bool ipv6PrefixIsSet() const;

  /////////////////////////////////////////////
  /// DnnConfigurationMessage members

  friend void to_json(nlohmann::json& j, const DnnConfigurationMessage& o);
  friend void from_json(const nlohmann::json& j, DnnConfigurationMessage& o);

 protected:
  std::string m_Dnn;  // Mandatory
  Snssai m_Snssai;    // Mandatory
  string m_Supi;      // Optional
  bool m_SupiIsSet;
  std::string m_PduSessionType;  // Optional
  bool m_PduSessionTypeIsSet;
  std::string m_Ipv4Range;  // Optional
  bool m_Ipv4RangeIsSet;
  std::string m_Ipv6Prefix;  // Optional
  bool m_Ipv6PrefixIsSet;
};

}  // namespace model
}  // namespace smf_server
}  // namespace oai

#endif /* DnnConfiguration_H_ */
