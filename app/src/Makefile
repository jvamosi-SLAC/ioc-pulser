TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build an ioc application
# Use PROD_IOC_RTEMS for RTEMS only
# Use PROD_IOC_Linux for Linux only
PROD_IOC = pulser

PROD_SYS_LIBS_DEFAULT += pcre

# pulser.dbd will be created and installed
DBD += pulser.dbd

# json.hpp requires C++11
drvPulser_CXXFLAGS_Linux += -std=c++11

# pulser.dbd will be made up from these files:
pulser_DBD += base.dbd
pulser_DBD += iocAdmin.dbd
pulser_DBD += asSupport.dbd
pulser_DBD += asyn.dbd
#pulser_DBD += stream.dbd
pulser_DBD += drvAsynIPPort.dbd
#pulser_DBD += drvAsynSerialPort.dbd
pulser_DBD += caPutLog.dbd
pulser_DBD += calcSupport.dbd
pulser_DBD += drvPulser.dbd

#INC         += drvpulser.h
#INC         += json.hpp
pulser_SRCS += drvPulser.cpp
pulser_SRCS += stmMsg.cpp
pulser_SRCS += stmConv.cpp

# <name>_registerRecordDeviceDriver.cpp will be created from <name>.dbd
pulser_SRCS += pulser_registerRecordDeviceDriver.cpp
pulser_SRCS_DEFAULT += pulserMain.cpp

# Add locally compiled object code
#pulser_SRCS += dbSubExample.c

pulser_LIBS += devIocStats
pulser_LIBS += autosave
pulser_LIBS += asyn
#pulser_LIBS += stream
pulser_LIBS += caPutLog
pulser_LIBS += calc
pulser_LIBS += $(EPICS_BASE_IOC_LIBS)

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE
