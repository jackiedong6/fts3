/* Copyright @ Members of the EMI Collaboration, 2010.
See www.eu-emi.eu for details on the copyright holders.

Licensed under the Apache License, Version 2.0 (the "License"); 
you may not use this file except in compliance with the License. 
You may obtain a copy of the License at 

    http://www.apache.org/licenses/LICENSE-2.0 

Unless required by applicable law or agreed to in writing, software 
distributed under the License is distributed on an "AS IS" BASIS, 
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. 
See the License for the specific language governing permissions and 
limitations under the License. */

/** \file dev.h Common FTS3 coding constructs. Each FTS3 source files must include it. */

#ifndef FTS3_COMMON_DEV_H
#define FTS3_COMMON_DEV_H

#ifndef FTS3_NAMESPACE_START
  /** Start FTS3 namespace. Example:
      \code
      FTS3_NAMESPACE_START
          code....
          ...
      FTS3_NAMESPACE_END
      \endcode
  */ 
  #define FTS3_NAMESPACE_START namespace fts3 {
#endif

#ifndef FTS3_NAMESPACE_END
  /** End FTS3 namespace. Example:
      \code
      FTS3_NAMESPACE_START
          code....
          ...
      FTS3_NAMESPACE_END
      \endcode
  */ 
  #define FTS3_NAMESPACE_END } 
#endif

#ifndef FTS3_NAMESPACE_NAME
  /** Defines FTS3 namespace name. */
  #define FTS3_NAMESPACE_NAME fts3
#endif

/** FTS 3 application label, can be found in different compile-time generated constructs. */
#define FTS3_APPLICATION_LABEL "FTS3_Server"

#ifdef NDEBUG
  /** Assert macro for FTS 3, in no-debug case. Usage: same than system assert macro/function. */
  #define FTS3_DEBUG_ASSERT(x) 
#else
  #include <assert.h>
  #include <iostream>
  /** Assert macro for FTS 3, in debug case. Usage: same than system assert macro/function. */
  #define FTS3_DEBUG_ASSERT(x) assert(x)
#endif

#endif // FTS3_COMMON_DEV_H

