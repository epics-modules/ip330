TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

LIBRARY_IOC_vxWorks += ip330
LIBRARY_IOC_vxWorks += devIp330

ip330_SRCS += ip330.cc
ip330_SRCS += ip330Scan.cc
ip330_SRCS += ip330Sweep.cc
ip330_SRCS += ip330PID.cc
ip330_SRCS += ip330ConfigServer.cc

devIp330_SRCS += devLoIp330Config.cc
devIp330_SRCS += devAiIp330Scan.cc

INC += Ip330.h
DBD += devIp330.dbd

ip330_LIBS += $(EPICS_BASE_IOC_LIBS)
#=============================


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
