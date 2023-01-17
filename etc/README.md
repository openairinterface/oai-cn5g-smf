 jinj2-generated configuration file #

We are switching to `python3-jinja2` tool in order to generate more complex configuration for our 5G core network functions.

Pre-requisites: install python3 and jinja2 packages:

```bash
sudo apt-get install -y python3 python3-jinja2
# or
sudo yum install -y python3 python3-pip
pip3 install jinja2
```

In a container deployment, you will still have to provide environment variables through a `docker-compose-file` or helm charts.

But you can also emulate how the entrypoint behaves locally on your workspace.

## Simple one-slice deployment ##

Create a `test-jinja.sh` file and edit it:

```bash
$ vi test-jinja.sh
cp ./etc/smf.conf ./etc/smf_copy.conf
export CONFIG_FILE=./etc/smf_copy.conf
export MOUNT_CONFIG=NO
export TZ=Europe/Paris
export SMF_INTERFACE_NAME_FOR_N4=eth0
export SMF_INTERFACE_NAME_FOR_SBI=eth0
export DEFAULT_DNS_IPV4_ADDRESS=172.21.3.100
export DEFAULT_DNS_SEC_IPV4_ADDRESS=8.8.8.8
export REGISTER_NRF=yes
export DISCOVER_UPF=yes
export USE_LOCAL_SUBSCRIPTION_INFO=yes
export USE_FQDN_DNS=yes

export AMF_IPV4_ADDRESS=192.168.70.132
export AMF_FQDN=oai-amf
export UDM_IPV4_ADDRESS=192.168.70.137
export UDM_API_VERSION=v2
export UDM_FQDN=oai-udm
export NRF_IPV4_ADDRESS=192.168.70.130
export NRF_FQDN=oai-nrf
export UPF_IPV4_ADDRESS=192.168.70.134
export UPF_FQDN_0=oai-spgwu

export DNN_NI0=oai
export TYPE0=IPv4
export DNN_RANGE0='12.1.1.151 - 12.1.1.253'
export NSSAI_SST0=1
#export NSSAI_SD0=123
export SESSION_AMBR_UL0=200Mbps
export SESSION_AMBR_DL0=400Mbps

./scripts/entrypoint.py
$ chmod 755 test-jinja.sh
$ ./test-jinja2.sh 
Configuration file ./etc/smf_copy.conf is ready
```

## List of fields ##

### Features ###

Here is the current list of fields, with their mandatory status and any default values.

If there is no default value associated to a field, it means it is **MANDATORY** to provide one.

| Field Name | Mandatory / Optional | Default value if any |
|:-----------|----------------------|---------------------:|
| INSTANCE | Optional | 0 |
| PID_DIRECTORY | Optional | /var/run |
| UE_MTU | Optional | 1500 |
| REGISTER_NRF | Optional | no |
| DISCOVER_UPF | Optional | no |
| DISCOVER_PCF | Optional | no |
| USE_LOCAL_SUBSCRIPTION_INFO | Optional | no |
| USE_LOCAL_PCC_RULES | Optional | yes |
| USE_FQDN_DNS | Optional | no |
| HTTP_VERSION | Optional | 1 |
| USE_NETWORK_INSTANCE | Optional | 1 |
| ENABLE_USAGE_REPORTING | Optional | no |

### Networking ###

| Field Name | Mandatory / Optional | Default value if any |
|:-----------|----------------------|---------------------:|
| SMF_INTERFACE_NAME_FOR_N4 | Mandatory | |
| SMF_INTERFACE_NAME_FOR_SBI | Mandatory | |
| SMF_INTERFACE_PORT_FOR_SBI | Optional | 80 |
| SMF_INTERFACE_HTTP2_PORT_FOR_SBI | Optional | 8080 |
| SMF_API_VERSION | Optional | v1 |
| DEFAULT_DNS_IPV4_ADDRESS | Mandatory | |
| DEFAULT_DNS_SEC_IPV4_ADDRESS | Mandatory | |
| DEFAULT_CSCF_IPV4_ADDRESS | Optional | 127.0.0.1 |
| AMF_HOSTNAME | Optional | |
| AMF_IPV4_ADDRESS | Mandatory if AMF_HOSTNAME not defined | 0.0.0.0 |
| AMF_PORT | Optional | 80 |
| AMF_API_VERSION | Optional | v1 |
| AMF_FQDN | Optional | localhost |
| UDM_HOSTNAME | Optional | |
| UDM_IPV4_ADDRESS | Mandatory if UDM_HOSTNAME not defined | 0.0.0.0 |
| UDM_PORT | Optional | 80 |
| UDM_API_VERSION | Optional | v1 |
| UDM_FQDN | Optional | localhost |
| NRF_HOSTNAME | Optional | |
| NRF_IPV4_ADDRESS | Mandatory if NRF_HOSTNAME not defined | 0.0.0.0 |
| NRF_PORT | Optional | 80 |
| NRF_API_VERSION | Optional | v1 |
| NRF_FQDN | Optional | localhost |
| PCF_HOSTNAME | Optional | |
| PCF_IPV4_ADDRESS | Mandatory if PCF_HOSTNAME not defined | 0.0.0.0 |
| PCF_PORT | Optional | 80 |
| PCF_API_VERSION | Optional | v1 |
| PCF_FQDN | Optional | localhost |

Currently we are supporting only one UPF.

| Field Name | Mandatory / Optional | Default value if any |
|:-----------|----------------------|---------------------:|
| UPF0_HOSTNAME | Optional | |
| UPF_IPV4_ADDRESS | Mandatory if UPF0_HOSTNAME not defined | 0.0.0.0 |
| UPF_FQDN_0 | Optional | localhost |
| DOMAIN_ACCESS_0 | Optional | access.oai.org |
| DOMAIN_CORE_0 | Optional | core.oai.org |

### Slice Management ###

At least 1 slice (`SLICE0`) **SHALL** be configured. See example in first section above.

We currently support up to 4 slices. When configuring, please do in order (`SLICE0`, `SLICE1`, ...).

The mandatory keywords per slice are `DNN_NIx` and `NSSAI_SSTx`. Once they are both defined for a slice, all parameters **SHALL** be defined.

| Field Name | Mandatory / Optional | Default value if any |
|:-----------|----------------------|---------------------:|
| DNN_NIx | Mandatory | |
| TYPEx | Optional | IPv4 |
| DNN_RANGEx | Mandatory | |
| NSSAI_SSTx | Mandatory | |
| NSSAI_SDx | Optional | 0xFFFFFF |
| QOS_PROFILE_5QIx | Optional | 6 or 7 or 8 or 9 |
| SESSION_AMBR_ULx | Optional | 20Mbps |
| SESSION_AMBR_DLx | Optional | 22Mbps |

`DNN_RANGEx` shall be formatted like this : `'12.2.1.2 - 12.2.1.128'`.

See example in first section above.
