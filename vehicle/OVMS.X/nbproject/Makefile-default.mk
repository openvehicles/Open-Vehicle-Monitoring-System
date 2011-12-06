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
MKDIR=mkdir -p
RM=rm -f 
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

# Object Files
OBJECTFILES=${OBJECTDIR}/UARTIntC.o ${OBJECTDIR}/can.o ${OBJECTDIR}/crypt_base64.o ${OBJECTDIR}/crypt_hmac.o ${OBJECTDIR}/crypt_md5.o ${OBJECTDIR}/crypt_rc4.o ${OBJECTDIR}/net.o ${OBJECTDIR}/net_msg.o ${OBJECTDIR}/net_sms.o ${OBJECTDIR}/ovms.o ${OBJECTDIR}/params.o ${OBJECTDIR}/utils.o


CFLAGS=
ASFLAGS=
LDLIBSOPTIONS=

# Path to java used to run MPLAB X when this makefile was created
MP_JAVA_PATH=/System/Library/Java/JavaVirtualMachines/1.6.0.jdk/Contents/Home/bin/
OS_CURRENT="$(shell uname -s)"
############# Tool locations ##########################################
# If you copy a project from one host to another, the path where the  #
# compiler is installed may be different.                             #
# If you open this project with MPLAB X in the new host, this         #
# makefile will be regenerated and the paths will be corrected.       #
#######################################################################
MP_CC=/Applications/microchip/mplabc18/v3.40/bin/mcc18
# MP_BC is not defined
MP_AS=/Applications/microchip/mplabc18/v3.40/bin/../mpasm/MPASMWIN
MP_LD=/Applications/microchip/mplabc18/v3.40/bin/mplink
MP_AR=/Applications/microchip/mplabc18/v3.40/bin/mplib
# MP_BC is not defined
MP_CC_DIR=/Applications/microchip/mplabc18/v3.40/bin
# MP_BC_DIR is not defined
MP_AS_DIR=/Applications/microchip/mplabc18/v3.40/bin/../mpasm
MP_LD_DIR=/Applications/microchip/mplabc18/v3.40/bin
MP_AR_DIR=/Applications/microchip/mplabc18/v3.40/bin
# MP_BC_DIR is not defined

# This makefile will use a C preprocessor to generate dependency files
MP_CPP=/Applications/microchip/mplabx/mplab_ide.app/Contents/Resources/mplab_ide/mplab_ide/modules/../../bin/mplab-cpp

.build-conf: ${BUILD_SUBPROJECTS}
	${MAKE}  -f nbproject/Makefile-default.mk dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof

MP_PROCESSOR_OPTION=18F2680
MP_PROCESSOR_OPTION_LD=18f2680
MP_LINKER_DEBUG_OPTION= -u_DEBUGCODESTART=0xfd80 -u_DEBUGCODELEN=0x280 -u_DEBUGDATASTART=0xcf4 -u_DEBUGDATALEN=0xb
# ------------------------------------------------------------------------------------
# Rules for buildStep: createRevGrep
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
__revgrep__:   nbproject/Makefile-${CND_CONF}.mk
	@echo 'grep -q $$@' > __revgrep__
	@echo 'if [ "$$?" -ne "0" ]; then' >> __revgrep__
	@echo '  exit 0' >> __revgrep__
	@echo 'else' >> __revgrep__
	@echo '  exit 1' >> __revgrep__
	@echo 'fi' >> __revgrep__
	@chmod +x __revgrep__
else
__revgrep__:   nbproject/Makefile-${CND_CONF}.mk
	@echo 'grep -q $$@' > __revgrep__
	@echo 'if [ "$$?" -ne "0" ]; then' >> __revgrep__
	@echo '  exit 0' >> __revgrep__
	@echo 'else' >> __revgrep__
	@echo '  exit 1' >> __revgrep__
	@echo 'fi' >> __revgrep__
	@chmod +x __revgrep__
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: assemble
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
else
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: compile
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
${OBJECTDIR}/ovms.o: ovms.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/ovms.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/ovms.o   ovms.c  > ${OBJECTDIR}/ovms.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/ovms.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/ovms.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/ovms.o.temp ovms.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/ovms.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/ovms.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/ovms.o.d
else
	cat ${OBJECTDIR}/ovms.o.temp >> ${OBJECTDIR}/ovms.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/net_msg.o: net_msg.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/net_msg.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net_msg.o   net_msg.c  > ${OBJECTDIR}/net_msg.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/net_msg.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/net_msg.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/net_msg.o.temp net_msg.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/net_msg.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/net_msg.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/net_msg.o.d
else
	cat ${OBJECTDIR}/net_msg.o.temp >> ${OBJECTDIR}/net_msg.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/crypt_base64.o: crypt_base64.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/crypt_base64.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_base64.o   crypt_base64.c  > ${OBJECTDIR}/crypt_base64.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/crypt_base64.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/crypt_base64.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/crypt_base64.o.temp crypt_base64.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/crypt_base64.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/crypt_base64.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/crypt_base64.o.d
else
	cat ${OBJECTDIR}/crypt_base64.o.temp >> ${OBJECTDIR}/crypt_base64.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/crypt_hmac.o: crypt_hmac.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/crypt_hmac.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_hmac.o   crypt_hmac.c  > ${OBJECTDIR}/crypt_hmac.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/crypt_hmac.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/crypt_hmac.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/crypt_hmac.o.temp crypt_hmac.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/crypt_hmac.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/crypt_hmac.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/crypt_hmac.o.d
else
	cat ${OBJECTDIR}/crypt_hmac.o.temp >> ${OBJECTDIR}/crypt_hmac.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/params.o: params.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/params.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/params.o   params.c  > ${OBJECTDIR}/params.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/params.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/params.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/params.o.temp params.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/params.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/params.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/params.o.d
else
	cat ${OBJECTDIR}/params.o.temp >> ${OBJECTDIR}/params.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/net_sms.o: net_sms.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/net_sms.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net_sms.o   net_sms.c  > ${OBJECTDIR}/net_sms.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/net_sms.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/net_sms.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/net_sms.o.temp net_sms.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/net_sms.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/net_sms.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/net_sms.o.d
else
	cat ${OBJECTDIR}/net_sms.o.temp >> ${OBJECTDIR}/net_sms.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/net.o: net.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/net.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net.o   net.c  > ${OBJECTDIR}/net.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/net.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/net.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/net.o.temp net.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/net.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/net.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/net.o.d
else
	cat ${OBJECTDIR}/net.o.temp >> ${OBJECTDIR}/net.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/crypt_md5.o: crypt_md5.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/crypt_md5.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_md5.o   crypt_md5.c  > ${OBJECTDIR}/crypt_md5.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/crypt_md5.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/crypt_md5.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/crypt_md5.o.temp crypt_md5.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/crypt_md5.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/crypt_md5.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/crypt_md5.o.d
else
	cat ${OBJECTDIR}/crypt_md5.o.temp >> ${OBJECTDIR}/crypt_md5.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/crypt_rc4.o: crypt_rc4.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/crypt_rc4.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_rc4.o   crypt_rc4.c  > ${OBJECTDIR}/crypt_rc4.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/crypt_rc4.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/crypt_rc4.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/crypt_rc4.o.temp crypt_rc4.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/crypt_rc4.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/crypt_rc4.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/crypt_rc4.o.d
else
	cat ${OBJECTDIR}/crypt_rc4.o.temp >> ${OBJECTDIR}/crypt_rc4.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/utils.o: utils.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/utils.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/utils.o   utils.c  > ${OBJECTDIR}/utils.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/utils.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/utils.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/utils.o.temp utils.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/utils.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/utils.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/utils.o.d
else
	cat ${OBJECTDIR}/utils.o.temp >> ${OBJECTDIR}/utils.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/UARTIntC.o: UARTIntC.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/UARTIntC.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/UARTIntC.o   UARTIntC.c  > ${OBJECTDIR}/UARTIntC.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/UARTIntC.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/UARTIntC.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/UARTIntC.o.temp UARTIntC.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/UARTIntC.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/UARTIntC.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/UARTIntC.o.d
else
	cat ${OBJECTDIR}/UARTIntC.o.temp >> ${OBJECTDIR}/UARTIntC.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/can.o: can.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/can.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -D__DEBUG -D__MPLAB_DEBUGGER_PK3=1 -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/can.o   can.c  > ${OBJECTDIR}/can.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/can.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/can.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/can.o.temp can.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/can.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/can.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/can.o.d
else
	cat ${OBJECTDIR}/can.o.temp >> ${OBJECTDIR}/can.o.d
endif
	${RM} __temp_cpp_output__
else
${OBJECTDIR}/ovms.o: ovms.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/ovms.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/ovms.o   ovms.c  > ${OBJECTDIR}/ovms.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/ovms.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/ovms.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/ovms.o.temp ovms.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/ovms.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/ovms.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/ovms.o.d
else
	cat ${OBJECTDIR}/ovms.o.temp >> ${OBJECTDIR}/ovms.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/net_msg.o: net_msg.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/net_msg.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net_msg.o   net_msg.c  > ${OBJECTDIR}/net_msg.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/net_msg.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/net_msg.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/net_msg.o.temp net_msg.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/net_msg.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/net_msg.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/net_msg.o.d
else
	cat ${OBJECTDIR}/net_msg.o.temp >> ${OBJECTDIR}/net_msg.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/crypt_base64.o: crypt_base64.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/crypt_base64.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_base64.o   crypt_base64.c  > ${OBJECTDIR}/crypt_base64.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/crypt_base64.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/crypt_base64.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/crypt_base64.o.temp crypt_base64.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/crypt_base64.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/crypt_base64.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/crypt_base64.o.d
else
	cat ${OBJECTDIR}/crypt_base64.o.temp >> ${OBJECTDIR}/crypt_base64.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/crypt_hmac.o: crypt_hmac.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/crypt_hmac.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_hmac.o   crypt_hmac.c  > ${OBJECTDIR}/crypt_hmac.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/crypt_hmac.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/crypt_hmac.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/crypt_hmac.o.temp crypt_hmac.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/crypt_hmac.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/crypt_hmac.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/crypt_hmac.o.d
else
	cat ${OBJECTDIR}/crypt_hmac.o.temp >> ${OBJECTDIR}/crypt_hmac.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/params.o: params.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/params.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/params.o   params.c  > ${OBJECTDIR}/params.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/params.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/params.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/params.o.temp params.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/params.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/params.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/params.o.d
else
	cat ${OBJECTDIR}/params.o.temp >> ${OBJECTDIR}/params.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/net_sms.o: net_sms.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/net_sms.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net_sms.o   net_sms.c  > ${OBJECTDIR}/net_sms.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/net_sms.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/net_sms.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/net_sms.o.temp net_sms.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/net_sms.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/net_sms.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/net_sms.o.d
else
	cat ${OBJECTDIR}/net_sms.o.temp >> ${OBJECTDIR}/net_sms.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/net.o: net.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/net.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/net.o   net.c  > ${OBJECTDIR}/net.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/net.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/net.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/net.o.temp net.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/net.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/net.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/net.o.d
else
	cat ${OBJECTDIR}/net.o.temp >> ${OBJECTDIR}/net.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/crypt_md5.o: crypt_md5.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/crypt_md5.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_md5.o   crypt_md5.c  > ${OBJECTDIR}/crypt_md5.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/crypt_md5.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/crypt_md5.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/crypt_md5.o.temp crypt_md5.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/crypt_md5.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/crypt_md5.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/crypt_md5.o.d
else
	cat ${OBJECTDIR}/crypt_md5.o.temp >> ${OBJECTDIR}/crypt_md5.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/crypt_rc4.o: crypt_rc4.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/crypt_rc4.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/crypt_rc4.o   crypt_rc4.c  > ${OBJECTDIR}/crypt_rc4.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/crypt_rc4.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/crypt_rc4.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/crypt_rc4.o.temp crypt_rc4.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/crypt_rc4.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/crypt_rc4.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/crypt_rc4.o.d
else
	cat ${OBJECTDIR}/crypt_rc4.o.temp >> ${OBJECTDIR}/crypt_rc4.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/utils.o: utils.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/utils.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/utils.o   utils.c  > ${OBJECTDIR}/utils.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/utils.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/utils.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/utils.o.temp utils.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/utils.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/utils.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/utils.o.d
else
	cat ${OBJECTDIR}/utils.o.temp >> ${OBJECTDIR}/utils.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/UARTIntC.o: UARTIntC.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/UARTIntC.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/UARTIntC.o   UARTIntC.c  > ${OBJECTDIR}/UARTIntC.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/UARTIntC.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/UARTIntC.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/UARTIntC.o.temp UARTIntC.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/UARTIntC.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/UARTIntC.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/UARTIntC.o.d
else
	cat ${OBJECTDIR}/UARTIntC.o.temp >> ${OBJECTDIR}/UARTIntC.o.d
endif
	${RM} __temp_cpp_output__
${OBJECTDIR}/can.o: can.c  nbproject/Makefile-${CND_CONF}.mk
	${RM} ${OBJECTDIR}/can.o.d 
	${MKDIR} ${OBJECTDIR} 
	${MP_CC} $(MP_EXTRA_CC_PRE) -p$(MP_PROCESSOR_OPTION) -k -Ou- -Ot- -Ob- -Op- -Or- -Od- -Opa-  -I ${MP_CC_DIR}/../h  -fo ${OBJECTDIR}/can.o   can.c  > ${OBJECTDIR}/can.err 2>&1 ; if [ $$? -eq 0 ] ; then cat ${OBJECTDIR}/can.err | sed 's/\(^.*:.*:\)\(Warning\)\(.*$$\)/\1 \2:\3/g' ; else cat ${OBJECTDIR}/can.err | sed 's/\(^.*:.*:\)\(Error\)\(.*$$\)/\1 \2:\3/g' ; exit 1 ; fi
	${MP_CPP}  -MMD ${OBJECTDIR}/can.o.temp can.c __temp_cpp_output__ -D __18F2680 -D __18CXX -I /Applications/microchip/mplabc18/v3.40/bin/../h  -D__18F2680
	printf "%s/" ${OBJECTDIR} > ${OBJECTDIR}/can.o.d
ifneq (,$(findstring MINGW32,$(OS_CURRENT)))
	cat ${OBJECTDIR}/can.o.temp | sed -e 's/\\\ /__SPACES__/g' -e's/\\$$/__EOL__/g' -e 's/\\/\//g' -e 's/__SPACES__/\\\ /g' -e 's/__EOL__/\\/g' >> ${OBJECTDIR}/can.o.d
else
	cat ${OBJECTDIR}/can.o.temp >> ${OBJECTDIR}/can.o.d
endif
	${RM} __temp_cpp_output__
endif

# ------------------------------------------------------------------------------------
# Rules for buildStep: link
ifeq ($(TYPE_IMAGE), DEBUG_RUN)
dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk
	${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_LD} $(MP_EXTRA_LD_PRE) /Applications/microchip/mplabx/mpasmx/LKR/18f2680_g.lkr  -p$(MP_PROCESSOR_OPTION_LD)  -w -x -u_DEBUG   -z__MPLAB_BUILD=1  -u_CRUNTIME -z__MPLAB_DEBUG=1 -z__MPLAB_DEBUGGER_PK3=1 $(MP_LINKER_DEBUG_OPTION) -l ${MP_CC_DIR}/../lib  -odist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof ${OBJECTFILES}      
else
dist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof: ${OBJECTFILES}  nbproject/Makefile-${CND_CONF}.mk
	${MKDIR} dist/${CND_CONF}/${IMAGE_TYPE} 
	${MP_LD} $(MP_EXTRA_LD_PRE) /Applications/microchip/mplabx/mpasmx/LKR/18f2680_g.lkr  -p$(MP_PROCESSOR_OPTION_LD)  -w    -z__MPLAB_BUILD=1  -u_CRUNTIME -l ${MP_CC_DIR}/../lib  -odist/${CND_CONF}/${IMAGE_TYPE}/OVMS.X.${IMAGE_TYPE}.cof ${OBJECTFILES}      
endif


# Subprojects
.build-subprojects:

# Clean Targets
.clean-conf:
	${RM} -r build/default
	${RM} -r dist/default

# Enable dependency checking
.dep.inc: .depcheck-impl

include .dep.inc
