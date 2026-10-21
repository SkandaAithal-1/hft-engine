# M7 — TCP Protocol Specification

## 1. Overview

- **Transport:** TCP over IPv4 loopback initially.
- **Protocol:** Custom binary protocol.
- **Protocol version:** `1` (`0x01`).
- **Byte order:** Big-endian (network byte order).
- **Processing model:** Single-threaded.
- **Scope:** Incoming limit orders and cancellation requests.
- **Recovery:** Rely on TCP reliability within a viable connection. No automatic reconnection or application-level retransmission in M7.

## 2. Frame Format

Each TCP message uses a length-prefixed frame.

```text
       0             4       5       6
       +-------------+-------+-------+------------------+
Bytes  | Length (4B) | Ver.  | Type  |     Payload      |
       +-------------+-------+-------+------------------+
                     |<---- Length bytes ------------->|
```

| Field | Size | Description |
|---|---:|---|
| Length | 4 bytes | Number of bytes following the length prefix |
| Version | 1 byte | Protocol version (`0x01`) |
| Type | 1 byte | Message type |
| Payload | Variable | Message-specific fields |

- The length prefix **excludes its own 4 bytes**.
- Maximum frame length, excluding the prefix: **64 bytes**.
- TCP is a byte stream; one read may contain a partial frame or multiple frames.

## 3. Message Types

### Type `0x01` — Limit Order

**Payload size:** 25 bytes  
**Total frame size:** 31 bytes

| Field | Size | Encoding |
|---|---:|---|
| Order ID | 8 bytes | Unsigned 64-bit integer |
| Side | 1 byte | `0` = Buy, `1` = Sell |
| Price | 8 bytes | Unsigned 64-bit integer |
| Quantity | 8 bytes | Unsigned 64-bit integer |

### Type `0x02` — Cancel Order

**Payload size:** 8 bytes  
**Total frame size:** 14 bytes

| Field | Size | Encoding |
|---|---:|---|
| Order ID | 8 bytes | Unsigned 64-bit integer |

## 4. Validation Rules

### Protocol-Level Validation

- Reject unsupported protocol versions.
- Reject unknown message types.
- Require exact payload lengths for each message type.
- Reject invalid side values.
- Reject frames exceeding the configured maximum.
- Treat malformed frames as protocol errors and close the connection; do not attempt stream resynchronization.

### Application-Level Validation

- Delegate order validation to the existing event-processing and matching logic.
- Invalid prices, quantities, duplicate IDs, and other business-level failures should produce normal processing results.
- Business-level rejection does not automatically terminate the TCP connection.

## 5. Sequence Numbers

- No sequence number is included in version 1 messages.
- The feed handler assigns monotonically increasing sequence numbers to successfully decoded messages.
- Wire-level sequence numbers may be considered later for reconnects, replay, duplicate detection, or UDP recovery.

## 6. Connection and Framing Behavior

- Support frames split across multiple TCP reads.
- Support multiple frames received in a single read.
- EOF between complete frames is a normal disconnect.
- EOF partway through a frame is an incomplete-message error.
- No automatic reconnection in M7.
- TCP reliability applies while the connection remains viable; it does not guarantee successful application processing across connection failures.

## 7. Component Responsibilities

| Component | Responsibility |
|---|---|
| TCP transport | Receive and send bytes |
| Frame decoder | Reassemble complete frames from the TCP byte stream |
| Message codec | Encode/decode messages and validate protocol fields |
| Feed handler | Convert decoded messages into existing application events |
| Event processor | Process events using the existing matching engine and order book |

The protocol layer must remain independent of the order book and matching engine.

## 8. Implementation Order

1. Define protocol message types and constants.
2. Implement encoding and decoding with explicit big-endian field handling.
3. Add codec unit tests, including malformed and truncated inputs.
4. Implement TCP framing and transport.
5. Integrate the feed handler with the existing event-processing pipeline.
6. Benchmark codec performance and end-to-end TCP ingestion.

## 9. Deferred Work

- UDP transport and feed recovery.
- Multicast market-data distribution.
- Application-level retransmission and snapshot recovery.
- Automatic reconnection and cross-connection sequencing.
- Multithreaded ingestion and lock-free queues.

These may be explored after the initial TCP path is correct and benchmarked.