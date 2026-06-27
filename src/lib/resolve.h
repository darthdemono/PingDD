/**
 * @file resolve.h
 * @brief Reverse-DNS and ASN annotation for resolved addresses.
 *
 * These helpers enrich a @ref host_t with human-meaningful context: the PTR
 * (reverse DNS) name of its primary address and the origin Autonomous System,
 * looked up via the Team Cymru IP-to-ASN DNS service
 * (https://team-cymru.com/community-services/ip-asn-mapping/).
 *
 * The ASN lookup uses a tiny self-contained DNS TXT client (no resolver
 * library), so it stays portable across the same platforms as the rest of
 * PingDD. It needs outbound UDP/53 and is therefore opt-in (`--resolve`).
 */
#ifndef PINGDD_RESOLVE_H
#define PINGDD_RESOLVE_H

#include "standard.h"

/**
 * @brief Annotate a resolved host with reverse DNS and origin-ASN info.
 *
 * Fills @ref host_t::ReverseName, @ref host_t::Asn and @ref host_t::AsnOrg for
 * the host's primary address. Best-effort: fields that cannot be determined are
 * left empty and no error is returned.
 *
 * @param[in,out] host Resolved host to annotate (must have AddrCount > 0).
 */
void Resolve_Annotate(host_t *const host);

#endif /* PINGDD_RESOLVE_H */
