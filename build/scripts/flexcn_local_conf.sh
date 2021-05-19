# prompt has been removed for easier Ctrl+C Ctrl+V
sudo ifconfig eno1:flexcn  172.16.1.101 up # FLEXCN 
sudo ifconfig eno1:amf  172.16.1.102 up # AMF 
sudo ifconfig eno1:udm  172.16.1.103 up # UDM

sudo ifconfig eno1:sn4 172.16.2.101 up # FLEXCN N4 interface
sudo ifconfig eno1:un4 172.16.2.102 up # UPF N4 interface

INSTANCE=1
PREFIX='/usr/local/etc/oai'
sudo mkdir -m 0777 -p $PREFIX
cp ../../etc/flexcn.conf  $PREFIX

declare -A FLEXCN_CONF

FLEXCN_CONF[@INSTANCE@]=$INSTANCE
FLEXCN_CONF[@PREFIX@]=$PREFIX
FLEXCN_CONF[@PID_DIRECTORY@]='/var/run'

FLEXCN_CONF[@FLEXCN_INTERFACE_NAME_FOR_N4@]='eno1:sn4'
FLEXCN_CONF[@FLEXCN_INTERFACE_NAME_FOR_SBI@]='eno1:flexcn'

FLEXCN_CONF[@FLEXCN_INTERFACE_IPV4_ADDRESS_FOR_SBI@]='172.16.1.101'
FLEXCN_CONF[@FLEXCN_INTERFACE_PORT_FOR_SBI@]='80'
FLEXCN_CONF[@FLEXCN_INTERFACE_HTTP2_PORT_FOR_SBI@]='9090'

FLEXCN_CONF[@UDM_IPV4_ADDRESS@]='172.16.1.103'
FLEXCN_CONF[@UDM_PORT@]='80'

FLEXCN_CONF[@AMF_IPV4_ADDRESS@]='172.16.1.102'
FLEXCN_CONF[@AMF_PORT@]='80'

FLEXCN_CONF[@UPF_IPV4_ADDRESS@]='172.16.2.102'

FLEXCN_CONF[@DEFAULT_DNS_IPV4_ADDRESS@]='8.8.8.8'
FLEXCN_CONF[@DEFAULT_DNS_SEC_IPV4_ADDRESS@]='4.4.4.4'

for K in "${!FLEXCN_CONF[@]}"; do 
  egrep -lRZ "$K" $PREFIX | xargs -0 -l sed -i -e "s|$K|${FLEXCN_CONF[$K]}|g"
  ret=$?;[[ ret -ne 0 ]] && echo "Tried to replace $K with ${FLEXCN_CONF[$K]}"
done
