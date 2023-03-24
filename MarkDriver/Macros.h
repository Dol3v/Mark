#pragma once
#include "DriverConfiguration.h"


#define LOG_SUCCESS(msg) KdPrint((DRIVER_PREFIX "[+] " msg "\n"));
#define LOG_SUCCESS_VA(msg, ...) KdPrint((DRIVER_PREFIX "[+] " msg "\n", __VA_ARGS__));

#define LOG_FAIL(msg) KdPrint((DRIVER_PREFIX __FILE__ " [-] " msg "\n"));
#define LOG_FAIL_VA(msg, ...) KdPrint((DRIVER_PREFIX __FILE__ " [-] " msg "\n", __VA_ARGS__));
#define LOG_FAIL_WITH_STATUS(msg, status) KdPrint((DRIVER_PREFIX __FILE__ " [-] " msg " (status=0x%08x)\n", status));

#define BREAK_ON_ERROR(status, msg) \
	if (!NT_SUCCESS((status))) {	\
		LOG_FAIL_WITH_STATUS(msg, status);			\
		break;						\
	}

#define RETURN_IF_FAILED(status) \
	if (!NT_SUCCESS((status))) {	\
		return status;						\
	}

#define PRINT_AND_RETURN_IF_FAILED(status, msg) \
	if (!NT_SUCCESS((status))) {	\
		LOG_FAIL_WITH_STATUS(msg, status);				\
		return status;				\
	}


#define STRINGIFY(x) #x


#define DECLARE_SYSTEM_FUNCTION(rettype, name, ...)      \
    typedef rettype(* name ## _t)(				\
        __VA_ARGS__                             \
    );											\
    rettype name(__VA_ARGS__);		
