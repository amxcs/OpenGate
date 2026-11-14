#ifndef OPENGATE_ECC_POLICY_H
#define OPENGATE_ECC_POLICY_H
#include <stdbool.h>
#include <stdint.h>
#include "opengate_ecc_layout.h"

/* Policy is for the built-in card image (opengate_builtin_data.inc) only.
 * It is reset on firmware MCU boot; IOP reset must not re-arm it. */
typedef struct {
    bool armed;
    bool payload_seen;
} opengate_ecc_policy_t;

#define OPENGATE_ECC_POLICY_INIT { true, false }

static inline void opengate_ecc_policy_observe(opengate_ecc_policy_t *p, uint32_t sector)
{
    if (sector == OPENGATE_PAYLOAD_FIRST_PAGE) p->payload_seen = true;
    if (sector == OPENGATE_RESTORE_LAST_PAGE && p->payload_seen) p->armed = false;
}
#endif
