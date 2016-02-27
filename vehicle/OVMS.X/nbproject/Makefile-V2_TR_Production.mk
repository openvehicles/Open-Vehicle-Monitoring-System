#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Include project Makefile
ifeq "${IGNORE_LOCAL}" "TRUE"
# do not include local makefile. User is passing all local related variables already
else
include Makefile
# Include makefile containing local settings
ifeq "$(wildcard nbproject/Makefile-local-V2_TR_Production.mk)" "nbproject/Makefile-local-V2_TR_Production.mk"
include nbproject/Makefile-local-V2_TR_Production.mk
endif
endif

# Environment
MKDIR=mkdir -p
RM=rm -f 
MV=mv 
CP=cp 

# Macros
CND_CONF=V2_TR_Production
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
IMAGE_TYPE=debug
OUTPUT_SUFFIX=cof
DEBUGGABLE_SUFFIX=cof
FINAL_IMAGE=dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
else
IMAGE_TYPE=production
OUTPUT_SUFFIX=hex
DEBUGGABLE_SUFFIX=cof
FINAL_IMAGE=dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}
endif

# Object Directory
OBJECTDIR=build/${CND_CONF}/${IMAGE_TYPE}

# Distribution Directory
DISTDIR=dist/${CND_CONF}/${IMAGE_TYPE}

# Source Files Quoted if spaced
SOURCEFILES_QUOTED_IF_SPACED=UARTIntC.c crypt_base64.c crypt_hmac.c crypt_md5.c crypt_rc4.c led.c net.c net_msg.c net_sms.c ovms.c params.c utils.c inputs.c diag.c vehicle_teslaroadster.c vehicle.c vehicle_none.c logging.c acc.c

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/UARTIntC.o ${OBJECTDIR}/crypt_base64.o ${OBJECTDIR}/crypt_hmac.o ${OBJECTDIR}/crypt_md5.o ${OBJECTDIR}/crypt_rc4.o ${OBJECTDIR}/led.o ${OBJECTDIR}/net.o ${OBJECTDIR}/net_msg.o ${OBJECTDIR}/net_sms.o ${OBJECTDIR}/ovms.o ${OBJECTDIR}/params.o ${OBJECTDIR}/utils.o ${OBJECTDIR}/inputs.o ${OBJECTDIR}/diag.o ${OBJECTDIR}/vehicle_teslaroadster.o ${OBJECTDIR}/vehicle.o ${OBJECTDIR}/vehicle_none.o ${OBJECTDIR}/logging.o ${OBJECTDIR}/acc.o
POSSIBLE_DEPFILES=${OBJECTDIR}/UARTIntC.o.d ${OBJECTDIR}/crypt_base64.o.d ${OBJECTDIR}/crypt_hmac.o.d ${OBJECTDIR}/crypt_md5.o.d ${OBJECTDIR}/crypt_rc4.o.d ${OBJECTDIR}/led.o.d ${OBJECTDIR}/net.o.d ${OBJECTDIR}/net_msg.o.d ${OBJECTDIR}/net_sms.o.d ${OBJECTDIR}/ovms.o.d ${OBJECTDIR}/params.o.d ${OBJECTDIR}/utils.o.d ${OBJECTDIR}/inputs.o.d ${OBJECTDIR}/diag.o.d ${OBJECTDIR}/vehicle_teslaroadster.o.d ${OBJECTDIR}/vehicle.o.d ${OBJECTDIR}/vehicle_none.o.d ${OBJECTDIR}/logging.o.d ${OBJECTDIR}/acc.o.d

# Object Files
OBJECTFILES=${OBJECTDIR}/UARTIntC.o ${OBJECTDIR}/crypt_base64.o ${OBJECTDIR}/crypt_hmac.o ${OBJECTDIR}/crypt_md5.o ${OBJECTDIR}/crypt_rc4.o ${OBJECTDIR}/led.o ${OBJECTDIR}/net.o ${OBJECTDIR}/net_msg.o ${OBJECTDIR}/net_sms.o ${OBJECTDIR}/ovms.o ${OBJECTDIR}/params.o ${OBJECTDIR}/utils.o ${OBJECTDIR}/inputs.o ${OBJECTDIR}/diag.o ${OBJECTDIR}/vehicle_teslaroadster.o ${OBJECTDIR}/vehicle.o ${OBJECTDIR}/vehicle_none.o ${OBJECTDIR}/logging.o ${OBJECTDIR}/acc.o

# Source Files
SOURCEFILES=UARTIntC.c crypt_base64.c crypt_hmac.c crypt_md5.c crypt_rc4.c led.c net.c net_msg.c net_sms.c ovms.c params.c utils.c inputs.c diag.c vehicle_teslaroadster.c vehicle.c vehicle_none.c logging.c acc.c


CFLAGS=
ASFLAGS=
LDLIBSOPTIONS=

############# Tool locations ##########################################
# If you copy a project from one host to another, the path where the  #
# compiler is installed may be different.                             #
# If you open this project with MPLAB X in the new host, this         #
# makefile will be regenerated and the paths will be corrected.       #
#######################################################################
# fixDeps replaces a bunch of sed/cat/printf statements that slow down the build
FIXDEPS=fixDeps

.build-conf:  ${BUILD_SUBPROJECTS}
ifneq ($(INFORMATION_MESSAGE), )
	@echo $(INFORMATION_MESSAGE)
endif
	${MAKE}  -f nbproject/Makefile-V2_TR_Production.mk dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}

MP_PROCESSOR_OPTION=18F2685
MP_PROCESSOR_OPTION_LD=18f2685
MP_LINKER_DEBUG_OPTION= -u_DEBUGCODESTART=0x17d30 -u_DEBUGCODELEN=0x2d0 -u_DEBUGDATASTART=0xcf4 -u_DEBUGDATALEN=0xb
# ------------------------------------------------------------------------------------
# Rules for buildStep: assemble
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compile
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${OBJECTDIR}/UARTIntC.o: UARTIntC.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/UARTIntC.o.d 
	@${RM} ${OBJECTDIR}/UARTIntC.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/UARTIntC.o   UARTIntC.c 
	@${DEP_GEN} -d ${OBJECTDIR}/UARTIntC.o 
	@${FIXDEPS} "${OBJECTDIR}/UARTIntC.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/crypt_base64.o: crypt_base64.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/crypt_base64.o.d 
	@${RM} ${OBJECTDIR}/crypt_base64.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_base64.o   crypt_base64.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_base64.o 
	@${FIXDEPS} "${OBJECTDIR}/crypt_base64.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/crypt_hmac.o: crypt_hmac.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/crypt_hmac.o.d 
	@${RM} ${OBJECTDIR}/crypt_hmac.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_hmac.o   crypt_hmac.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_hmac.o 
	@${FIXDEPS} "${OBJECTDIR}/crypt_hmac.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/crypt_md5.o: crypt_md5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/crypt_md5.o.d 
	@${RM} ${OBJECTDIR}/crypt_md5.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_md5.o   crypt_md5.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_md5.o 
	@${FIXDEPS} "${OBJECTDIR}/crypt_md5.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/crypt_rc4.o: crypt_rc4.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/crypt_rc4.o.d 
	@${RM} ${OBJECTDIR}/crypt_rc4.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_rc4.o   crypt_rc4.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_rc4.o 
	@${FIXDEPS} "${OBJECTDIR}/crypt_rc4.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/led.o: led.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/led.o.d 
	@${RM} ${OBJECTDIR}/led.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/led.o   led.c 
	@${DEP_GEN} -d ${OBJECTDIR}/led.o 
	@${FIXDEPS} "${OBJECTDIR}/led.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/net.o: net.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/net.o.d 
	@${RM} ${OBJECTDIR}/net.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net.o   net.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net.o 
	@${FIXDEPS} "${OBJECTDIR}/net.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/net_msg.o: net_msg.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/net_msg.o.d 
	@${RM} ${OBJECTDIR}/net_msg.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net_msg.o   net_msg.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net_msg.o 
	@${FIXDEPS} "${OBJECTDIR}/net_msg.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/net_sms.o: net_sms.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/net_sms.o.d 
	@${RM} ${OBJECTDIR}/net_sms.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net_sms.o   net_sms.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net_sms.o 
	@${FIXDEPS} "${OBJECTDIR}/net_sms.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/ovms.o: ovms.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/ovms.o.d 
	@${RM} ${OBJECTDIR}/ovms.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/ovms.o   ovms.c 
	@${DEP_GEN} -d ${OBJECTDIR}/ovms.o 
	@${FIXDEPS} "${OBJECTDIR}/ovms.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/params.o: params.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/params.o.d 
	@${RM} ${OBJECTDIR}/params.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/params.o   params.c 
	@${DEP_GEN} -d ${OBJECTDIR}/params.o 
	@${FIXDEPS} "${OBJECTDIR}/params.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/utils.o: utils.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/utils.o.d 
	@${RM} ${OBJECTDIR}/utils.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/utils.o   utils.c 
	@${DEP_GEN} -d ${OBJECTDIR}/utils.o 
	@${FIXDEPS} "${OBJECTDIR}/utils.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/inputs.o: inputs.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/inputs.o.d 
	@${RM} ${OBJECTDIR}/inputs.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/inputs.o   inputs.c 
	@${DEP_GEN} -d ${OBJECTDIR}/inputs.o 
	@${FIXDEPS} "${OBJECTDIR}/inputs.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/diag.o: diag.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/diag.o.d 
	@${RM} ${OBJECTDIR}/diag.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/diag.o   diag.c 
	@${DEP_GEN} -d ${OBJECTDIR}/diag.o 
	@${FIXDEPS} "${OBJECTDIR}/diag.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/vehicle_teslaroadster.o: vehicle_teslaroadster.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/vehicle_teslaroadster.o.d 
	@${RM} ${OBJECTDIR}/vehicle_teslaroadster.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/vehicle_teslaroadster.o   vehicle_teslaroadster.c 
	@${DEP_GEN} -d ${OBJECTDIR}/vehicle_teslaroadster.o 
	@${FIXDEPS} "${OBJECTDIR}/vehicle_teslaroadster.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/vehicle.o: vehicle.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/vehicle.o.d 
	@${RM} ${OBJECTDIR}/vehicle.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/vehicle.o   vehicle.c 
	@${DEP_GEN} -d ${OBJECTDIR}/vehicle.o 
	@${FIXDEPS} "${OBJECTDIR}/vehicle.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/vehicle_none.o: vehicle_none.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/vehicle_none.o.d 
	@${RM} ${OBJECTDIR}/vehicle_none.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/vehicle_none.o   vehicle_none.c 
	@${DEP_GEN} -d ${OBJECTDIR}/vehicle_none.o 
	@${FIXDEPS} "${OBJECTDIR}/vehicle_none.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/logging.o: logging.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/logging.o.d 
	@${RM} ${OBJECTDIR}/logging.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/logging.o   logging.c 
	@${DEP_GEN} -d ${OBJECTDIR}/logging.o 
	@${FIXDEPS} "${OBJECTDIR}/logging.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/acc.o: acc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/acc.o.d 
	@${RM} ${OBJECTDIR}/acc.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/acc.o   acc.c 
	@${DEP_GEN} -d ${OBJECTDIR}/acc.o 
	@${FIXDEPS} "${OBJECTDIR}/acc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
else
${OBJECTDIR}/UARTIntC.o: UARTIntC.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/UARTIntC.o.d 
	@${RM} ${OBJECTDIR}/UARTIntC.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/UARTIntC.o   UARTIntC.c 
	@${DEP_GEN} -d ${OBJECTDIR}/UARTIntC.o 
	@${FIXDEPS} "${OBJECTDIR}/UARTIntC.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/crypt_base64.o: crypt_base64.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/crypt_base64.o.d 
	@${RM} ${OBJECTDIR}/crypt_base64.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_base64.o   crypt_base64.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_base64.o 
	@${FIXDEPS} "${OBJECTDIR}/crypt_base64.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/crypt_hmac.o: crypt_hmac.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/crypt_hmac.o.d 
	@${RM} ${OBJECTDIR}/crypt_hmac.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_hmac.o   crypt_hmac.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_hmac.o 
	@${FIXDEPS} "${OBJECTDIR}/crypt_hmac.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/crypt_md5.o: crypt_md5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/crypt_md5.o.d 
	@${RM} ${OBJECTDIR}/crypt_md5.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_md5.o   crypt_md5.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_md5.o 
	@${FIXDEPS} "${OBJECTDIR}/crypt_md5.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/crypt_rc4.o: crypt_rc4.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/crypt_rc4.o.d 
	@${RM} ${OBJECTDIR}/crypt_rc4.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_rc4.o   crypt_rc4.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_rc4.o 
	@${FIXDEPS} "${OBJECTDIR}/crypt_rc4.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/led.o: led.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/led.o.d 
	@${RM} ${OBJECTDIR}/led.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/led.o   led.c 
	@${DEP_GEN} -d ${OBJECTDIR}/led.o 
	@${FIXDEPS} "${OBJECTDIR}/led.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/net.o: net.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/net.o.d 
	@${RM} ${OBJECTDIR}/net.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net.o   net.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net.o 
	@${FIXDEPS} "${OBJECTDIR}/net.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/net_msg.o: net_msg.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/net_msg.o.d 
	@${RM} ${OBJECTDIR}/net_msg.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net_msg.o   net_msg.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net_msg.o 
	@${FIXDEPS} "${OBJECTDIR}/net_msg.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/net_sms.o: net_sms.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/net_sms.o.d 
	@${RM} ${OBJECTDIR}/net_sms.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net_sms.o   net_sms.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net_sms.o 
	@${FIXDEPS} "${OBJECTDIR}/net_sms.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/ovms.o: ovms.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/ovms.o.d 
	@${RM} ${OBJECTDIR}/ovms.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/ovms.o   ovms.c 
	@${DEP_GEN} -d ${OBJECTDIR}/ovms.o 
	@${FIXDEPS} "${OBJECTDIR}/ovms.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/params.o: params.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/params.o.d 
	@${RM} ${OBJECTDIR}/params.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/params.o   params.c 
	@${DEP_GEN} -d ${OBJECTDIR}/params.o 
	@${FIXDEPS} "${OBJECTDIR}/params.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/utils.o: utils.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/utils.o.d 
	@${RM} ${OBJECTDIR}/utils.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/utils.o   utils.c 
	@${DEP_GEN} -d ${OBJECTDIR}/utils.o 
	@${FIXDEPS} "${OBJECTDIR}/utils.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/inputs.o: inputs.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/inputs.o.d 
	@${RM} ${OBJECTDIR}/inputs.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/inputs.o   inputs.c 
	@${DEP_GEN} -d ${OBJECTDIR}/inputs.o 
	@${FIXDEPS} "${OBJECTDIR}/inputs.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/diag.o: diag.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/diag.o.d 
	@${RM} ${OBJECTDIR}/diag.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/diag.o   diag.c 
	@${DEP_GEN} -d ${OBJECTDIR}/diag.o 
	@${FIXDEPS} "${OBJECTDIR}/diag.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/vehicle_teslaroadster.o: vehicle_teslaroadster.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/vehicle_teslaroadster.o.d 
	@${RM} ${OBJECTDIR}/vehicle_teslaroadster.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/vehicle_teslaroadster.o   vehicle_teslaroadster.c 
	@${DEP_GEN} -d ${OBJECTDIR}/vehicle_teslaroadster.o 
	@${FIXDEPS} "${OBJECTDIR}/vehicle_teslaroadster.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/vehicle.o: vehicle.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/vehicle.o.d 
	@${RM} ${OBJECTDIR}/vehicle.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/vehicle.o   vehicle.c 
	@${DEP_GEN} -d ${OBJECTDIR}/vehicle.o 
	@${FIXDEPS} "${OBJECTDIR}/vehicle.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/vehicle_none.o: vehicle_none.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/vehicle_none.o.d 
	@${RM} ${OBJECTDIR}/vehicle_none.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/vehicle_none.o   vehicle_none.c 
	@${DEP_GEN} -d ${OBJECTDIR}/vehicle_none.o 
	@${FIXDEPS} "${OBJECTDIR}/vehicle_none.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/logging.o: logging.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/logging.o.d 
	@${RM} ${OBJECTDIR}/logging.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/logging.o   logging.c 
	@${DEP_GEN} -d ${OBJECTDIR}/logging.o 
	@${FIXDEPS} "${OBJECTDIR}/logging.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
${OBJECTDIR}/acc.o: acc.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} "${OBJECTDIR}" 
	@${RM} ${OBJECTDIR}/acc.o.d 
	@${RM} ${OBJECTDIR}/acc.o 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -DOVMS_CAR_NONE -DOVMS_CAR_TESLAROADSTER -DOVMS_HW_V2 -DOVMS_DIAGMODULE -DOVMS_LOGGINGMODULE -DOVMS_ACCMODULE -DOVMS_BUILDCONFIG=\"TRP\" -ml --extended -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/acc.o   acc.c 
	@${DEP_GEN} -d ${OBJECTDIR}/acc.o 
	@${FIXDEPS} "${OBJECTDIR}/acc.o.d" $(SILENT) -rsi ${MP_CC_DIR}../ -c18 
	
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: link
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk    
	@${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_LD} $(MP_EXTRA_LD_PRE)   -p$(MP_PROCESSOR_OPTION_LD)  -w -x -u_DEBUG -m"build/memorymap" -u_EXTENDEDMODE -z__MPLAB_BUILD=1  -u_CRUNTIME -z__MPLAB_DEBUG=1 -z__MPLAB_DEBUGGER_PK3=1 $(MP_LINKER_DEBUG_OPTION) -l ${MP_CC_DIR}/../lib  -o dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}  ${OBJECTFILES_QUOTED_IF_SPACED}   
else
dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.${OUTPUT_SUFFIX}: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk   
	@${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_LD} $(MP_EXTRA_LD_PRE)   -p$(MP_PROCESSOR_OPTION_LD)  -w  -m"build/memorymap" -u_EXTENDEDMODE -z__MPLAB_BUILD=1  -u_CRUNTIME -l ${MP_CC_DIR}/../lib  -o dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.${DEBUGGABLE_SUFFIX}  ${OBJECTFILES_QUOTED_IF_SPACED}   
endif


# Subprojects
.build-subprojects:


# Subprojects
.clean-subprojects:

# Clean Targets
.clean-conf: ${CLEAN_SUBPROJECTS}
	${RM} -r build/V2_TR_Production
	${RM} -r dist/V2_TR_Production

# Enable dependency checking
.dep.inc: .depcheck-impl

DEPFILES=$(shell "${PATH_TO_IDE_BIN}"mplabwildcard ${POSSIBLE_DEPFILES})
ifneq (${DEPFILES},)
include ${DEPFILES}
endif
