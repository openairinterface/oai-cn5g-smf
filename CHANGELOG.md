# RELEASE NOTES: #

## v1.5.0 -- January 2023 ##

* Feature improvements:
  * PDU SESS EST + Qos Monitoring events exposure
  * Giving some time for FQDN resolution
  * PFCP: removing dependency to folly
  * Add IEs to support UE Rel 16
  * Added and integrated UPF graph instead of PFCP associations list
    * Supported scenarios:
      - I-UPF / A-UPF N9
      - UL CL
  * Implement Npcf_SMPolicyControl API client
  * Changing the configuration file to jinja2-based approach
    * At least one slice (slice #0) shall be completely defined
  * Ubuntu22 and cgroup2 support
* Fixes:
  * Fix openapi cause
  * Fix small issue for Mime parser for normal message
  * Fix Volume Threshold IE to PFCP Create URR IE
  * Fix mcc mnc 00 udr
  * Fix issues to work with Release-16 COTS UE
  * Fix issue when there's no valid reply from UPF for PDU session release
  * Fix Protocol identifier Selected Bearer Control Mode
  * Fix issue for Static UE IP Addr
  * Fix URR ID in IE Create URR (post UPF graph implementation)
* Docker image improvements
  * Fixed docker exit by catching SIGTERM
  * release mode does not use libasan anymore --> allocation of 20T virtual memory is no longer done

## v1.4.0 -- July 2022 ##

* Fix for handling S-NSSAI 
* Add support for IMS
* HTTP/2 support for SBI interface
* Add usage reporting feature
* Flexible MTU
* Support for Ubuntu 20.04 bare-metal installation
* Official images produced by CI are pushed to `oaisoftwarealliance` Docker-Hub Team account
* Reduce image size


## v1.3.0 -- January 2022 ##

* Update NWI from NF profile
* Update SMF profile
* Added retry for PFCP association request
* More flexible DNN/IP ranges when deploying container
* Fix retrieving the list of NWI
* Fix entreprise IE decoding
* Fix issue for UPF selection
* Fix for IE Measurement Period
* Docker optimization

## v1.2.1 -- October 2021 ##

* Fix build issue
* Fix UPF selection
* Timers handling: T3591/T3952

## v1.2.0 -- September 2021 ##

* Event Exposure implemented
* UPF profile update
* Support for URR query
* Bug fixes

## v1.1.0 -- July 2021 ##

* PFCP Release 16 support
* NRF registration
  - with FQDN DNS resolution
* support for multiple UPF instances
* Dotted DNN support
* use (SST, SD) to get the subscription information
* Bug fixes
* Full support for Ubuntu18 and RHEL8

## v1.0.0 -- September 2020 ##

* Initial release, SMF can handle basic PDU session-related procedures e.g., session establishment request, session modification and session release.

