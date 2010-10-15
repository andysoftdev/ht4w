#include "sigar.h"

static sigar_version_t sigar_version = {
    "07/09/2009 04:06 AM",
    "80016c6",
    "1.6.3.82",
    "x86-winnt",
    "sigar-x86-winnt.lib",
    "sigar-x86-winnt",
    "SIGAR-1.6.3.82, "
    "SCM revision 80016c6, "
    "built 07/09/2009 04:06 AM as sigar-x86-winnt.lib",
    1,
    6,
    3,
    82
};

SIGAR_DECLARE(sigar_version_t *) sigar_version_get(void)
{
    return &sigar_version;
}
