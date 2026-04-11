#pragma once

#include <QtCore/QtGlobal>

#if defined(NDI_LIBRARY)
    #define NDI_EXPORT Q_DECL_EXPORT
#else
    #define NDI_EXPORT Q_DECL_IMPORT
#endif
