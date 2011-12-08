#
# Generated Makefile - do not edit!
#
# Edit the Makefile in the project folder instead (../Makefile). Each target
# has a -pre and a -post target defined where you can add customized code.
#
# This makefile implements configuration specific macros and targets.


# Include project Makefile
include Makefile

# Environment
SHELL=cmd.exe
# Adding MPLAB X bin directory to path
PATH:=C:/Program Files/Microchip/MPLABX/mplab_ide/mplab_ide/modules/../../bin/:$(PATH)
MKDIR=gnumkdir -p
RM=rm -f 
MV=mv 
CP=cp 

# Macros
CND_CONF=default
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
IMAGE_TYPE=debug
FINAL_IMAGE=dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof
else
IMAGE_TYPE=production
FINAL_IMAGE=dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof
endif

# Object Directory
OBJECTDIR=build/${CND_CONF}/${IMAGE_TYPE}

# Distribution Directory
DISTDIR=dist/${CND_CONF}/${IMAGE_TYPE}

# Object Files Quoted if spaced
OBJECTFILES_QUOTED_IF_SPACED=${OBJECTDIR}/UARTIntC.o ${OBJECTDIR}/can.o ${OBJECTDIR}/crypt_base64.o ${OBJECTDIR}/crypt_hmac.o ${OBJECTDIR}/crypt_md5.o ${OBJECTDIR}/crypt_rc4.o ${OBJECTDIR}/net.o ${OBJECTDIR}/net_msg.o ${OBJECTDIR}/net_sms.o ${OBJECTDIR}/ovms.o ${OBJECTDIR}/params.o ${OBJECTDIR}/utils.o

# Object Files
OBJECTFILES=${OBJECTDIR}/UARTIntC.o ${OBJECTDIR}/can.o ${OBJECTDIR}/crypt_base64.o ${OBJECTDIR}/crypt_hmac.o ${OBJECTDIR}/crypt_md5.o ${OBJECTDIR}/crypt_rc4.o ${OBJECTDIR}/net.o ${OBJECTDIR}/net_msg.o ${OBJECTDIR}/net_sms.o ${OBJECTDIR}/ovms.o ${OBJECTDIR}/params.o ${OBJECTDIR}/utils.o


CFLAGS=
ASFLAGS=
LDLIBSOPTIONS=

# Path to java used to run MPLAB X when this makefile was created
MP_JAVA_PATH="C:\Program Files\Java\jre6/bin/"
OS_CURRENT="$(shell uname -s)"
############# Tool locations ##########################################
# If you copy a project from one host to another, the path where the  #
# compiler is installed may be different.                             #
# If you open this project with MPLAB X in the new host, this         #
# makefile will be regenerated and the paths will be corrected.       #
#######################################################################
MP_CC="C:\Program Files\Microchip\mplabc18\v3.40\bin\mcc18.exe"
# MP_BC is not defined
MP_AS="C:\Program Files\Microchip\mplabc18\v3.40\bin\..\mpasm\MPASMWIN.exe"
MP_LD="C:\Program Files\Microchip\mplabc18\v3.40\bin\mplink.exe"
MP_AR="C:\Program Files\Microchip\mplabc18\v3.40\bin\mplib.exe"
DEP_GEN=${MP_JAVA_PATH}java -jar "C:/Program Files/Microchip/MPLABX/mplab_ide/mplab_ide/modules/../../bin/extractobjectdependencies.jar" 
# fixDeps replaces a bunch of sed/cat/printf statements that slow down the build
FIXDEPS=fixDeps
MP_CC_DIR="C:\Program Files\Microchip\mplabc18\v3.40\bin"
# MP_BC_DIR is not defined
MP_AS_DIR="C:\Program Files\Microchip\mplabc18\v3.40\bin\..\mpasm"
MP_LD_DIR="C:\Program Files\Microchip\mplabc18\v3.40\bin"
MP_AR_DIR="C:\Program Files\Microchip\mplabc18\v3.40\bin"
# MP_BC_DIR is not defined

.build-conf:  ${BUILD_SUBPROJECTS}
	${MAKE}  -f nbproject/Makefile-default.mk dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof

MP_PROCESSOR_OPTION=18F2680
MP_PROCESSOR_OPTION_LD=18f2680
MP_LINKER_DEBUG_OPTION=
# ------------------------------------------------------------------------------------
# Rules for buildStep: assemble
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compile
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${OBJECTDIR}/ovms.o: ovms.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/ovms.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/ovms.o   ovms.c 
	@${DEP_GEN} -d ${OBJECTDIR}/ovms.o 
	
${OBJECTDIR}/net_msg.o: net_msg.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/net_msg.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/net_msg.o   net_msg.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net_msg.o 
	
${OBJECTDIR}/crypt_base64.o: crypt_base64.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/crypt_base64.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/crypt_base64.o   crypt_base64.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_base64.o 
	
${OBJECTDIR}/crypt_hmac.o: crypt_hmac.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/crypt_hmac.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/crypt_hmac.o   crypt_hmac.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_hmac.o 
	
${OBJECTDIR}/params.o: params.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/params.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/params.o   params.c 
	@${DEP_GEN} -d ${OBJECTDIR}/params.o 
	
${OBJECTDIR}/net_sms.o: net_sms.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/net_sms.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/net_sms.o   net_sms.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net_sms.o 
	
${OBJECTDIR}/net.o: net.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/net.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/net.o   net.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net.o 
	
${OBJECTDIR}/crypt_md5.o: crypt_md5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/crypt_md5.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/crypt_md5.o   crypt_md5.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_md5.o 
	
${OBJECTDIR}/crypt_rc4.o: crypt_rc4.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/crypt_rc4.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/crypt_rc4.o   crypt_rc4.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_rc4.o 
	
${OBJECTDIR}/utils.o: utils.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/utils.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/utils.o   utils.c 
	@${DEP_GEN} -d ${OBJECTDIR}/utils.o 
	
${OBJECTDIR}/UARTIntC.o: UARTIntC.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/UARTIntC.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/UARTIntC.o   UARTIntC.c 
	@${DEP_GEN} -d ${OBJECTDIR}/UARTIntC.o 
	
${OBJECTDIR}/can.o: can.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/can.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG  -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/can.o   can.c 
	@${DEP_GEN} -d ${OBJECTDIR}/can.o 
	
else
${OBJECTDIR}/ovms.o: ovms.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/ovms.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/ovms.o   ovms.c 
	@${DEP_GEN} -d ${OBJECTDIR}/ovms.o 
	
${OBJECTDIR}/net_msg.o: net_msg.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/net_msg.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/net_msg.o   net_msg.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net_msg.o 
	
${OBJECTDIR}/crypt_base64.o: crypt_base64.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/crypt_base64.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/crypt_base64.o   crypt_base64.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_base64.o 
	
${OBJECTDIR}/crypt_hmac.o: crypt_hmac.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/crypt_hmac.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/crypt_hmac.o   crypt_hmac.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_hmac.o 
	
${OBJECTDIR}/params.o: params.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/params.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/params.o   params.c 
	@${DEP_GEN} -d ${OBJECTDIR}/params.o 
	
${OBJECTDIR}/net_sms.o: net_sms.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/net_sms.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/net_sms.o   net_sms.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net_sms.o 
	
${OBJECTDIR}/net.o: net.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/net.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/net.o   net.c 
	@${DEP_GEN} -d ${OBJECTDIR}/net.o 
	
${OBJECTDIR}/crypt_md5.o: crypt_md5.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/crypt_md5.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/crypt_md5.o   crypt_md5.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_md5.o 
	
${OBJECTDIR}/crypt_rc4.o: crypt_rc4.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/crypt_rc4.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/crypt_rc4.o   crypt_rc4.c 
	@${DEP_GEN} -d ${OBJECTDIR}/crypt_rc4.o 
	
${OBJECTDIR}/utils.o: utils.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/utils.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/utils.o   utils.c 
	@${DEP_GEN} -d ${OBJECTDIR}/utils.o 
	
${OBJECTDIR}/UARTIntC.o: UARTIntC.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/UARTIntC.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/UARTIntC.o   UARTIntC.c 
	@${DEP_GEN} -d ${OBJECTDIR}/UARTIntC.o 
	
${OBJECTDIR}/can.o: can.c  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} ${OBJECTDIR} 
	@${RM} ${OBJECTDIR}/can.o.d 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k  -I ${MP_CC_DIR}\\..\\h  -fo ${OBJECTDIR}/can.o   can.c 
	@${DEP_GEN} -d ${OBJECTDIR}/can.o 
	
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: link
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_LD} $(MP_EXTRA_LD_PRE)   -p$(MP_PROCESSOR_OPTION_LD)  -w -x -u_DEBUG   -z__MPLAB_BUILD=1  -u_CRUNTIME -z__MPLAB_DEBUG=1 $(MP_LINKER_DEBUG_OPTION) -l ${MP_CC_DIR}\\..\\lib  -odist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof  ${OBJECTFILES_QUOTED_IF_SPACED}   
else
dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk
	@${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_LD} $(MP_EXTRA_LD_PRE)   -p$(MP_PROCESSOR_OPTION_LD)  -w    -z__MPLAB_BUILD=1  -u_CRUNTIME -l ${MP_CC_DIR}\\..\\lib  -odist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof  ${OBJECTFILES_QUOTED_IF_SPACED}   
endif


# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf:
	${RM} -r build/default
	${RM} -r dist/default

# Enable dependency checking
.dep.inc: .depcheck-impl

DEPFILES=$(wildcard $(addsuffix .d, ${OBJECTFILES}))
ifneq (${DEPFILES},)
include ${DEPFILES}
endif
