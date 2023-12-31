﻿#pragma once

#include <cstdint>

#define NAN_BOXING 1

#if _DEBUG
#define DEBUG_PRINT_CODE 1
#define DEBUG_TRACE_EXECUTION 1
#else
#define DEBUG_PRINT_CODE 0
#define DEBUG_TRACE_EXECUTION 0
#endif

#define DEBUG_STRESS_GC 0
#define DEBUG_LOG_GC 0

#define LOCAL_VARIABLE_COUNT (UINT8_MAX + 1)
#define UPVALUE_COUNT (UINT8_MAX)

