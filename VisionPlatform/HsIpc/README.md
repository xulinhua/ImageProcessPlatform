# HsIpc

本机进程通讯库。视觉平台和同机的其它软件（例如运控）链接同一份 `HsIpc.dll`，用同一套报文交换 OK/NG、统计、图像和文件。库不包含工站、流程、Qt 或 Halcon。

协议字段以 [`../../docs/本机进程通讯库方案.md`](../../docs/本机进程通讯库方案.md) 为准。这里只写怎么编译和怎么调用。

## 编译

在 Visual Studio 2022 里打开 `VisionPlatform.sln`，工程在 **ClassLib / Port** 下，名为 `HsIpc`。源码在 `VisionPlatform/HsIpc`。配置用 **Debug|x64** 或 **Release|x64**，也有对应的 Win32。`VisionIpc` 的 `Global\Vision.Ipc` 映射和两个事件通过 `HsIpc::Region`、`HsIpc::Notify` 创建，对象名不加 `HsIpc.` 前缀。

产出是 `bin/<平台>/<配置>/HsIpc.dll`，以及同目录的导入库。人工对通道时用同目录下的 `HsIpcDebug.exe`，说明见 `HsIpcDebug/README.md`。设备串口和网口用 `Vision.PortDebug.exe`。

只依赖 Windows API 和 `ws2_32`。调用方包含 `include/hs_ipc.h`，链接 `HsIpc.lib`，运行时能找到 `HsIpc.dll`。

## 调用

C++ 调用方用头文件里的 `HsIpc::Session`、`HsIpc::Region`、`HsIpc::Notify`。这三个类不从 DLL 导出，在调用方编译，只转调 C 接口。DLL 仍导出 C 函数，这样视觉平台和运控可以用不同版本的 Visual Studio 链接同一份 DLL，虚表不跨过边界。

一条通道在 `Session::open` 时选定一种传输，之后不要换。谁先打开谁创建对象，后打开的连上已有对象。通道短名两边必须一致，只允许字母、数字、`.`、`_`、`-`。

```cpp
HsIpcConfig cfg;
std::memset(&cfg, 0, sizeof(cfg));
cfg.transport = HS_IPC_SHARED_MEMORY;
cfg.name = "Hs.VisionMotion";

std::string err;
HsIpc::Session session = HsIpc::Session::open(cfg, &err);
if (!session.opened()) {
    return;
}

HsIpcMessage msg;
std::memset(&msg, 0, sizeof(msg));
msg.type = HS_IPC_SIGNAL;
msg.name = "station-1";
msg.result = HS_IPC_RESULT_OK;
session.send(msg);

HsIpcMessage got;
if (session.poll(&got, 1000) == HS_IPC_OK) {
    /* got.name、got.text、got.data 保持到下一次 poll 或析构，不要 free */
}
```

自己排版的共享内存用 `HsIpc::Region::open`，叫醒对方用 `HsIpc::Notify::open`。析构时各自关闭。

`Session::send` / `Session::poll` 的返回值：

| 值 | 含义 |
|---|---|
| `HS_IPC_OK` | 成功 |
| `HS_IPC_TIMEOUT` | poll 在时限内没有报文。`timeoutMs < 0` 会一直等 |
| `HS_IPC_ERR_FULL` | 信号环、消息队列或三个图像/文件槽都满。不覆盖还没取走的 |
| `HS_IPC_ERR_SIZE` | 共享内存的图像或文件超过槽大小，或单帧超过分片上限 |
| `HS_IPC_ERR_PROTOCOL` | 魔数、版本或帧体对不上 |
| `HS_IPC_ERR_IO` | 管道、套接字或内核对象失败 |
| `HS_IPC_ERR_ARG` | 参数不完整，例如图像有长度但没有数据指针 |

心跳是否超时由调用方看 `Session::lastRecvMs`。库只记下上次成功 poll 的本机毫秒时间，不把通道标成离线。

## 四种传输

| `transport` | 什么时候用 | 图像和文件怎么过去 |
|---|---|---|
| `HS_IPC_SHARED_MEMORY` | 同机、传图多 | 内容进槽，环里只有槽号。poll 把字节拷出来。统计只留最新一包 |
| `HS_IPC_NAMED_PIPE` | 同机、按顺序取每一条 | 帧按顺序写进 `\\.\pipe\HsIpc.<名字>`。超过 1 MB 分片 |
| `HS_IPC_MESSAGE_QUEUE` | 同机、不能丢中间那条 OK/NG | 库内队列，不安装 MSMQ。满了返回 `HS_IPC_ERR_FULL` |
| `HS_IPC_SOCKET` | 跨机器，或对方只用套接字 | TCP。`host` 空则 `127.0.0.1`，`port` 必填。帧格式与管道相同 |

信号量和命名事件只用来叫醒对方，不是单独的传输。UDP、邮槽、窗口消息、COM 不在这个库里。

共享内存先创建在 `Global\`，这样会话 0 里的服务和已登录用户能打开同一块。当前用户没有创建全局段的权限时，整组对象退回 `Local\`，同一登录会话里的进程仍然互通。要跨会话时，由有权限的一方先打开。

## 和 VisionIpc 的关系

`VisionIpc` 仍只给 `VisionApp`、`Vision.Station`、`Vision.Link` 用，名字是 `Global\Vision.Ipc`。不要把运控报文写进那块内存。`HsIpc` 用调用方传入的通道名，另开一块。

以后三个内部进程可以改走 `HsIpc`，但不是把 `SharedFrames` 换成 `hs_ipc_send` 就结束。替换前要补的命令报文和直接写槽，写在方案第 1 节。
