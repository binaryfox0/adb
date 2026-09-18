# Pairing
## Formats
### Pairing Packet

| Offset | Size | Field | Type | Description |
|-------:|-----:|-------|------|-------------|
| `0x00` | 1 | `version` | `u8` | Packet format version. Currently `1`. |
| `0x01` | 1 | `type` | [`pairing_type`](#pairing-packet-types) | Type of pairing packet. |
| `0x02` | 4 | `size` | `u32be` | Payload size in bytes. Must not exceed `8192` bytes. |
| `0x06` | N | `payload` | `bytes` | Packet payload. |

### Pairing Packet Types

| Value | Name | Description |
|------:|------|-------------|
| `0x00` | `SPAKE2_MSG` | SPAKE2 message. |
| `0x01` | `PEER_INFO` | Peer information. |

### Android Public Key

The Android public key uses the following binary format:
| Offset | Size | Field | Type | Description |
|-------:|-----:|-------|------|-------------|
| `0x0000` | 4 | `modulus_size_words` | `u32le` | RSA-2048 modulus size in 32-bit words. Always `64`. |
| `0x0004` | 4 | `n0inv` | `u32le` | `-N⁻¹ mod 2³²`, where `N` is the RSA modulus. |
| `0x0008` | 256 | `modulus` | `u8[256]` | RSA-2048 modulus `N`. |
| `0x0108` | 256 | `rr` | `u8[256]` | `R² mod N`, where `R = 2²⁰⁴⁸`. |
| `0x0208` | 4 | `exponent` | `u32le` | RSA public exponent. Always `65537`. |

The total size of the binary public key is `528` bytes.

### ADB Public Key
The ADB public key is transmitted as a Base64-encoded representation of the Android public key, followed by the host identity:

```text
<base64-encoded-android-public-key> <hostname>@<username>
```

### SPAKE2 Password

The SPAKE2 password is constructed from the pairing code and key material exported from the TLS connection:

| Offset | Size | Field | Type | Description |
|-------:|-----:|-------|------|-------------|
| `0x00` | 6 | `code` |`char[6]`|`Six-digit pairing code.|
| `0x06` | 64 | `key_material` |`u8[64]`|Key material exported from the TLS connection using the label "adb-label\0".|

The resulting password is `70` bytes long.

## Pairing Process

The entire pairing process is performed over an encrypted TLS 1.3 connection using a self-signed X.509 certificate generated from an RSA-2048 private key.

1. TLS Connection
The client establishes a TLS 1.3 connection with the device.
The connection uses a self-signed X.509 certificate generated from an RSA-2048 private key.

2. SPAKE2 Key Exchange
The client and server exchange SPAKE2 messages using the SPAKE2 password.
The SPAKE2 roles are:
- `my_role`: "adb pair client"
- `their_role`: "adb pair server"

After processing the peer's SPAKE2 message, both sides derive the same shared key material.

3. AES-128 Key Derivation
The SPAKE2-derived key material is passed through HKDF-SHA256.
The first 16 bytes of the derived output are used as the AES-128 encryption key for the peer information exchange.

4. Peer Information Exchange
The client encodes its ADB public key and encrypts it using the derived AES-128 key.
The encrypted public key is sent to the device in a PEER_INFO pairing packet.
The device processes the public key and responds with a null-terminated mDNS instance name for subsequent discovery
