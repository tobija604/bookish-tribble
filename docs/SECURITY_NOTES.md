# Security notes — read before deploying past a bench prototype

This is a wide-breadth skeleton. A few security-relevant pieces are
deliberately left as documented TODOs rather than guessed at:

1. **Password storage.** `src/users/User.h` has a `passwordHash` field and
   `src/web/WebServer.h`'s `/api/auth/login` currently does **not** check
   it — see the `NOTE` comment right there. Wire in a salted hash (e.g.
   mbedTLS SHA-256 with a per-user random salt, stored alongside the hash)
   before this leaves a trusted bench/LAN setting. Do not store plaintext
   passwords.

2. **Transport is plain HTTP.** The device has no way to hold a publicly
   trusted TLS certificate for a LAN IP/hostname. This is normal for local
   IoT admin panels but means credentials travel in cleartext on the LAN.
   If this ever needs to cross an untrusted network, put it behind a
   reverse proxy with TLS termination rather than trying to add a
   self-signed cert story to the ESP32 itself.

3. **CSRF mitigation** is the custom-header approach
   (`X-SmartBox-Request`), which is sufficient for a same-origin SPA with
   no third-party embedding, but is not a full CSRF-token scheme. Revisit
   if this UI is ever iframed or proxied from another origin.

4. **Rate limiting** (`SecurityManager::onFailure`) counts failures and
   raises `TamperDetected` past a threshold, but does not currently impose
   an increasing lockout delay on the login endpoint itself — a
   determined attacker with LAN access can still hammer `/api/auth/login`
   at the HTTP layer. Consider adding a short per-IP backoff in
   `WebServer.h` if this box is reachable from anything less trusted than
   a home/classroom LAN.

5. **Backups exclude password hashes intentionally** (spec 32) — a
   restored backup will need `passwordHash` re-set for any non-default
   admin accounts.

6. **OTA integrity** checks a SHA-256 digest supplied in the same request
   as the firmware URL (`OtaManager::performUpdate`) — this protects
   against corruption/tampering-in-transit but is not a cryptographic
   signature. If firmware will ever be pushed automatically (not
   admin-triggered), add real signing (e.g. verify against a public key
   baked into the bootloader) rather than trusting a caller-supplied hash.

None of this blocks using the skeleton for its intended purpose (a school
project / prototype on a trusted LAN), but all six should be closed out
before the box guards anything that actually matters.
