/* core/operatorgate.h — requirePlatformOperator gate (split from w4api).
 * PLATFORM_OPERATOR_EMAILS/IDS allowlist vs the Supabase identity.
 * 0 allow; -401 no user; -1403 not configured; -403 not an operator. */
#ifndef JO_OPERATORGATE_H
#define JO_OPERATORGATE_H
#include "auth.h"
int opgate_check(const auth_user *u);
#endif
