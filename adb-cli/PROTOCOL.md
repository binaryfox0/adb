# Protocol Specification

## Packet Types

The protocol defines three types of packets:
- **Command Packet**: Requests the server to execute a command.
- **Log Packet**: Reports log messages generated during command execution.
- **Result Packet**: Returns the outcome of a command execution.

## 1. Command Packet

A Command Packet is sent by the client to request that the server execute a specific command. The packet contains the command name, optional logging configuration, and any additional arguments required by the command.

### Fields

| Field | Type | Required | Description |
|---|---|---|---|
| `type` | string | Yes | Packet type. Must be `"cmd"`. |
| `cmd` | string | Yes | Name of the command to execute on the server. |
| `log` | string | No | Logging verbosity level used by `libadb` during command execution. |
| Additional fields | varies | Depends on command | Command-specific arguments required for execution. |

### Example

The following packet requests the server to pull a file from a remote device to a local path, with debug-level logging enabled.

```json
{
    "type": "cmd",
    "cmd": "pull",
    "log": "debug",
    "local_path": "/home/binaryfox0/Download/important.txt",
    "remote_path": "/home/binaryfox0/Download/important.txt"
}
````

The arguments required by a command depend on the selected command. For example, the `pull` command requires both `local_path` and `remote_path`.

If the `log` field is omitted, the server uses `error` by default.

### 2\. Log Packet

A Log Packet is sent by the server to report messages generated during command execution. These messages may describe progress, diagnostic information, warnings, or errors.

### Fields

| Field | Type   | Required | Description                                     |
| ----- | ------ | -------- | ----------------------------------------------- |
| type  | string | Yes      | Packet type. Must be "log".                     |
| level | string | Yes      | Severity level of the log message.              |
| msg   | string | Yes      | Human-readable description of the logged event. |

### Example

```json
{
    "type": "log",
    "level": "error",
    "msg": "Cryptography error"
}
```

Log Packets may be transmitted throughout command execution, allowing the client to receive diagnostic information before the final result is returned.

### 3\. Result Packet

A Result Packet is sent by the server when a command has completed. It indicates whether the operation succeeded or failed and contains the corresponding output or error information.

### Fields

| Field    | Type    | Required | Description                                                                                                        |
| -------- | ------- | -------- | ------------------------------------------------------------------------------------------------------------------ |
| type     | string  | Yes      | Packet type. Must be "result".                                                                                     |
| is_error | boolean | Yes      | Indicates whether the command execution failed. true means an error occurred; false means the operation succeeded. |
| result   | array   | Yes      | Contains the command output or error details, depending on the command and its execution status.                   |

### Example: Successful Execution

```json
{
    "type": "result",
    "is_error": false,
    "result": [
        "192.168.0.104:44389"
    ]
}
```

In this example, the command completed successfully, and the server returned an array containing the resulting device address and port.

### Example: Failed Execution

```json
{
    "type": "result",
    "is_error": true,
    "result": [
        "Failed to execute command"
    ]
}
```

When an operation fails, `is_error` is set to `true`, and the `result` array contains information describing the failure.

### 4\. Command Execution Flow

A typical command execution follows this sequence:

* The client sends a Command Packet containing the command name and its required arguments.

* The server validates the request and begins executing the requested command.

* During execution, the server may send one or more Log Packets to report progress or diagnostic information.

* Once execution finishes, the server sends a Result Packet containing the outcome and any returned data.

### Example Flow

```text
Client                  Server
  |                        |
  |------ Command -------->|
  |                        |
  |<------- Log -----------|
  |                        |
  |<------- Log -----------|
  |                        |
  |<------ Result ---------|
  |                        |
```

This protocol allows the client to request operations, receive real-time logging information, and obtain the final result through a consistent packet structure.
