/* core/maintenanceapi.h — GET /api/admin/maintenance?hours= digest
 * (self-healing/repair report; split from w4api). */
#ifndef JO_MAINTENANCEAPI_H
#define JO_MAINTENANCEAPI_H
#include "db.h"
char *maintenance_digest(db_handle *db, int hours);
#endif
