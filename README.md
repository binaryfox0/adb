# ADB
> [!IMPORTANT]
> This project is not owned by or affiliated with Google. It is an independent attempt to re-implement the protocol.
> The name adb is used solely for consistency with the original project and terminology.

## Implementation Status
All the implementations show below was based on Google's ADB source code commit [1cf2f017d312f73b3dc53bda85ef2610e35a80e9](https://android.googlesource.com/platform/packages/modules/adb/+/1cf2f017d312f73b3dc53bda85ef2610e35a80e9)
| Feature | Description | Status | Remarks |
|-|-|-|-|
| shell_v2 | Shell protocol version 2 | ❌ | |
| cmd | Execute a shell command | ❌ | |
| stat_v2 | File status protocol version 2 | ❌ | |
| libusb | USB device support | ⚠️ | Untested against a real device |
| push_sync | Push synchronization support | ❌ | |
| apex | APEX package support | ❌ | |
| ls_v2 | File listing protocol version 2 | ❌ | |
| sendrecv_v2 | File transfer protocol version 2 | ⚠️ | Only receive was fully usable |
| sendrecv_v2_brotli | File transfer v2 with Brotli compression | ⚠️ | Only receive was fully usable |
| sendrecv_v2_lz4 | File transfer v2 with LZ4 compression | ⚠️ | Only receive was fully usable |
| sendrecv_v2_zstd | File transfer v2 with Zstandard compression | ⚠️ | Only receive was fully usable |
| fixed_push_mkdir | Fixed directory creation during push | ❌ | |
| abb | Android Bug/Debug Bridge (ABB) support | ❌ | |
| fixed_push_symlink_timestamp | Preserve symlink timestamps during push | ❌ | |
| abb_exec | Execute commands through ABB | ❌ | |
| remount_shell | Remount support through the shell | ❌ | |
| track_app | Track application/package changes | ❌ | |
| sendrecv_v2_dry_run_send | Dry-run mode for send/receive v2 transfers | ❌ | |
| delayed_ack | Delayed acknowledgement support | ❌ | |
| openscreen_mdns | OpenScreen mDNS device discovery | ❌ | |
| devicetracker_proto_format | Device Tracker protocol format | ❌ | |
| devraw | Raw device communication support | ❌ | |
| app_info | Additional application information for tracking | ❌ | |
| server_status | Server status reporting | ❌ | |

## Research
[My research on the protocol](./docs)
