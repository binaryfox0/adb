# Protocol Specification
## Overview

This protocol defines a packet-based communication interface between a client and a server. It supports command execution, logging, transfer progress, and result reporting through four packet types:

- **Command Packet (`cmd`)**: Requests the server to execute a command.
- **Log Packet (`log`)**: Reports progress, diagnostic information, warnings, or errors during execution.
- **Progress Packet (`progress`)**: Reports transferred and total bytes during file transfers.
- **Result Packet (`result`)**: Returns the outcome of a completed command.

## 1. Command Packet

A Command Packet is sent by the client to request execution of a specific command. It contains the command name, optional logging configuration, and any additional parameters required by the command.

### 1.1 Generic Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | Yes | Packet type. Must be `"cmd"`. |
| `cmd` | string | Yes | Name of the command to execute on the server. |
| `log` | string | No | Logging verbosity level used by `libadb`. Defaults to `"error"`. |

### 1.2 Pairing and Connection Fields

These fields apply to pairing and connection commands.

| Field | Type | Required | Description |
|---|---|---|---|
| `key_path` | string | No | Path to the RSA-2048 private key. Defaults to `$HOME/.android/adbkey` if omitted. |

The `disconnect` command requires a `serial` field identifying the connected device, using the same serial or endpoint supplied to `connect`.

### 1.3 File Transfer Fields

The `push` command uses `remote_path` and `local_path`. The `pull` command
accepts a `files` array containing one or more remote and local path pairs. The
`stat` and `ls` commands use only `remote_path`; `ls` lists one remote directory.

| Field | Type | Required | Description |
|---|---|---|---|
| `remote_path` | string | Yes for `push`, `stat`, and `ls` | Remote file or directory path. |
| `local_path` | string | Yes for `push` | Path to the target file on the local machine. |
| `files` | array | Yes for `pull` | File objects with `remote_path` and `local_path` strings. |

The `stat` command returns the remote file's device, inode, mode, link count,
ownership, size, and timestamps in the result packet.

The `ls` command returns an `entries` array. Each entry contains its `name`,
stat fields, and `errno` (zero when the entry was inspected successfully).

### 1.4 Example: Pull Command

The following packet requests the server to pull a file from a remote device to a local path with debug-level logging enabled.

```json
{
    "type": "cmd",
    "cmd": "pull",
    "log": "debug",
    "files": [
        {
            "local_path": "/home/binaryfox0/Download/important.txt",
            "remote_path": "/home/binaryfox0/Download/important.txt"
        }
    ]
}
````

### 1.5 Default Values
* If `key_path` is omitted, the server uses `$HOME/.android/adbkey`.
* If `log` is omitted, the server uses `"error"`.

### 2\. Log Packet

A Log Packet is sent by the server to report messages generated during command execution. These messages may include progress updates, diagnostic information, warnings, or errors.

Log Packets can be transmitted throughout execution, allowing the client to receive diagnostic information before the final result is returned.

### 2.1 Fields

| Field | Type   | Required | Description                                     |
| ----- | ------ | -------- | ----------------------------------------------- |
| type  | string | Yes      | Packet type. Must be "log".                     |
| level | string | Yes      | Severity level of the log message.              |
| msg   | string | Yes      | Human-readable description of the logged event. |

### 2.2 Example

```json
{
    "type": "log",
    "level": "error",
    "msg": "Cryptography error"
}
```

### 2.3 Transfer Progress Packet

During `push` and `pull`, the server sends a progress packet at approximately one-second intervals, followed by a final progress packet before the result.

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | Yes | Packet type. Must be `"progress"`. |
| `bytes` | unsigned integer | Yes | Bytes transferred so far. |
| `total_bytes` | unsigned integer | Yes | Total file size in bytes. |

### 3\. Result Packet

A Result Packet is sent by the server when a command has completed. It indicates whether the operation succeeded or failed and contains the corresponding output or error information.

### 3.1 Generic Fields

| Field    | Type    | Required | Description                                                                                                    |
| -------- | ------- | -------- | -------------------------------------------------------------------------------------------------------------- |
| type     | string  | Yes      | Packet type. Must be "result".                                                                                 |
| is_error | boolean | Yes      | Indicates whether command execution failed. true means an error occurred; false means the operation succeeded. |
| result   | array   | Yes      | Contains command output or error details, depending on the command and its execution status.                   |

### 3.2 Device Discovery Fields

The `devices` field is used by the `devices` command to return the discovered devices.

| Field   | Type             | Required | Description                                                                                                             |
| ------- | ---------------- | -------- | ----------------------------------------------------------------------------------------------------------------------- |
| devices | array of strings | Yes      | Contains the discovered device identifiers, such as wireless addresses in host:port format or wired USB serial numbers. |

### 3.3 Example: Successful Execution

The following example illustrates a successful `devices` command.

```json
{
    "type": "result",
    "result": "success",
    "devices": [
        "192.168.0.104:44389"
    ]
}
```

In this example, the command completed successfully, and the server returned an array containing the discovered device address and port.

### 3.4 Example: Failed Execution

The following example illustrates a failure caused by insufficient memory while constructing the result payload.

```json
{
    "type": "result",
    "result": "success",
    "error": "out of memory while constructing result payload"
}
```

When an operation fails, `is_error` is set to `true`, and the response contains information describing the failure.

### 3.5 Result Handling

* The `type` field must be `"result"`.
* The `result` field indicates whether the command succeeded or failed.
* The remaining fields e.g `devices` contain the command outcome.

* Additional fields, such as `devices` or `error`, may be included depending on the command and its execution status.

### 4\. Command Execution Flow

A typical command execution follows these steps:

* Request: The client sends a Command Packet containing the command name and required arguments.
* Validation: The server validates the request and checks the required fields.
* Execution: The server executes the requested command.
* Logging and progress: During execution, the server may send Log Packets and, for file transfers, periodic Progress Packets.
* Completion: The server sends a Result Packet containing the outcome and any returned data.

### 4.1 Example Flow

```text
Client                  Server
  |                        |
  |------ Command -------->|
  |                        |
  |<------- Log -----------|
  |                        |
  |<------- Log -----------|
  |                        |
    |<---- Progress ---------|
    |                        |
  |<------ Result ---------|
  |                        |
```

### 5\. Protocol Summary

This protocol provides a consistent mechanism for command execution, real-time logging, and result reporting.

* Command Packets initiate operations and carry command-specific parameters.
* Log Packets provide progress updates and diagnostic information during execution.
* Progress Packets report transferred bytes and total bytes for `push` and `pull`.
* Result Packets communicate the final outcome and any associated data or error details.
* Default Configuration allows optional fields, such as `key_path` and `log`, to be omitted when their default values are appropriate.
* Command-Specific Fields support operations such as device discovery, pairing, connection establishment, and file transfer.

This design allows clients to execute commands, monitor their progress, and process results through a consistent packet-based interface.
