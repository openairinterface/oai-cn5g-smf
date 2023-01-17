# prompt has been removed for easier Ctrl+C Ctrl+V
sudo ifconfig eno1:smf  172.16.1.101 up # SMF 
sudo ifconfig eno1:amf  172.16.1.102 up # AMF 
sudo ifconfig eno1:udm  172.16.1.103 up # UDM

sudo ifconfig eno1:sn4 172.16.2.101 up # SMF N4 interface
sudo ifconfig eno1:un4 172.16.2.102 up # UPF N4 interface

INSTANCE=1
PREFIX='/usr/local/etc/oai'
sudo mkdir -m 0777 -p $PREFIX
cp ../../etc/smf.conf  $PREFIX

export CONFIG_FILE=$PREFIX/smf.conf
export MOUNT_CONFIG=NO

export INSTANCE=1
export SMF_INTERFACE_NAME_FOR_N4='eno1:sn4'
export SMF_INTERFACE_NAME_FOR_SBI='eno1:smf'
export UDM_IPV4_ADDRESS='172.16.1.103'
export UDM_PORT='80'
export AMF_IPV4_ADDRESS='172.16.1.102'
export AMF_PORT='80'
export UPF_IPV4_ADDRESS='172.16.2.102'

export DEFAULT_DNS_IPV4_ADDRESS='8.8.8.8'
export DEFAULT_DNS_SEC_IPV4_ADDRESS='4.4.4.4'

# One mandatory slice
export DNN_NI0=oai
export TYPE0=IPv4
export DNN_RANGE0='12.1.1.151 - 12.1.1.253'
export NSSAI_SST0=1
#export NSSAI_SD0=123
export SESSION_AMBR_UL0=800Mbps
export SESSION_AMBR_DL0=1000Mbps

../../scripts/entrypoint.py
