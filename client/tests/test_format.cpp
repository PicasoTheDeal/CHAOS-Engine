#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include "chaos_format.h"

TEST_CASE("chaos_file_t is exactly 512 bytes with the documented offsets") {
    STATIC_REQUIRE(sizeof(chaos_file_t) == 512);
    REQUIRE(offsetof(chaos_file_t, seed_full) == 8);
    REQUIRE(offsetof(chaos_file_t, instance_id) == 40);
    REQUIRE(offsetof(chaos_file_t, client_random) == 56);
    REQUIRE(offsetof(chaos_file_t, ticket_id) == 88);
    REQUIRE(offsetof(chaos_file_t, ticket_user_id) == 104);
    REQUIRE(offsetof(chaos_file_t, ticket_issued_ms) == 112);
    REQUIRE(offsetof(chaos_file_t, ticket_commit) == 120);
    REQUIRE(offsetof(chaos_file_t, ticket_server_random) == 152);
    REQUIRE(offsetof(chaos_file_t, ticket_sig) == 184);
    REQUIRE(offsetof(chaos_file_t, family) == 248);
    REQUIRE(offsetof(chaos_file_t, mutation_seed) == 252);
    REQUIRE(offsetof(chaos_file_t, claimed_at_ms) == 256);
    REQUIRE(offsetof(chaos_file_t, owner_id) == 264);
    REQUIRE(offsetof(chaos_file_t, nonce) == 272);
    REQUIRE(offsetof(chaos_file_t, cert_sig) == 288);
    REQUIRE(offsetof(chaos_file_t, preview_hash) == 352);
    REQUIRE(offsetof(chaos_file_t, seed_display) == 384);
    REQUIRE(offsetof(chaos_file_t, display_len) == 428);
    REQUIRE(CHAOS_TICKET_OFFSET + CHAOS_TICKET_WIRE_SIZE == offsetof(chaos_file_t, family));
    REQUIRE(CHAOS_TICKET_SIGNED_OFFSET + CHAOS_TICKET_SIGNED_LEN == offsetof(chaos_file_t, ticket_sig));
    REQUIRE(CHAOS_CERT_SIGNED_OFFSET + CHAOS_CERT_SIGNED_LEN == offsetof(chaos_file_t, cert_sig));
}
