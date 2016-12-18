//==============================================================================
//                                                                              
//            Copyright (C) 2003-2007, Coolsand Technologies, Inc.              
//                            All Rights Reserved                               
//                                                                              
//      This source code is the property of Coolsand Technologies and is        
//      confidential.  Any  modification, distribution,  reproduction or        
//      exploitation  of  any content of this file is totally forbidden,        
//      except  with the  written permission  of  Coolsand Technologies.        
//                                                                              
//==============================================================================
//                                                                              
//    THIS FILE WAS GENERATED AUTOMATICALLY BY THE MAKE PROCESS.
//                                                                              
//                       !!! PLEASE DO NOT EDIT !!!                             
//                                                                              
//==============================================================================

#ifndef _YUNBA_VERSION_H_
#define _YUNBA_VERSION_H_

// =============================================================================
//  MACROS
// =============================================================================

#define YUNBA_VERSION_REVISION                     (-1)

// =============================================================================
//  TYPES
// =============================================================================

#ifndef YUNBA_VERSION_NUMBER
#define YUNBA_VERSION_NUMBER                       (0)
#endif

#ifndef YUNBA_VERSION_DATE
#define YUNBA_VERSION_DATE                         (BUILD_DATE)
#endif

#ifndef YUNBA_VERSION_STRING
#define YUNBA_VERSION_STRING                       "YUNBA version string not defined"
#endif

#ifndef YUNBA_VERSION_STRING_WITH_BRANCH
#define YUNBA_VERSION_STRING_WITH_BRANCH           YUNBA_VERSION_STRING " Branch: " "none"
#endif

#define YUNBA_VERSION_STRUCT                       {YUNBA_VERSION_REVISION, \
                                                  YUNBA_VERSION_NUMBER, \
                                                  YUNBA_VERSION_DATE, \
                                                  YUNBA_VERSION_STRING_WITH_BRANCH}

#endif // _YUNBA_VERSION_H_
