include_directories(../../CommonUtilitiesLib)
add_library (RTCPUtilitiesLib 
             RTCPAckPacket.cpp RTCPAckPacket.h
	     RTCPPacket.cpp RTCPPacket.h
	     RTCPAPPPacket.cpp RTCPAPPPacket.h
	     RTCPAPPQTSSPacket.cpp RTCPAPPQTSSPacket.h
	     RTCPAPPNADUPacket.cpp RTCPAPPNADUPacket.h
	     RTCPSRPacket.cpp RTCPSRPacket.h)
