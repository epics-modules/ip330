TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

LIBRARY_IOC_vxWorks += ip330
LIBRARY_IOC_vxWorks += devIp330

ip330_SRCS += drvIp330.c
ip330_SRCS += devIp330.c

INC += asynIp330.h
DBD += ip330Support.dbd

ip330_LIBS += $(EPICS_BASE_IOC_LIBS)
#=============================


#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
