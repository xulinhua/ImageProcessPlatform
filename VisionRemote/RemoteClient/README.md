# RemoteClient

Ubuntu 22.04 上的被控端。源码在本仓库的 `RemoteClient/`，改完后整份拷到测试机 `/home/hs/testCode/RemoteControl/RemoteClient` 再编译。和 Windows 中控台 `RemoteMaster` 用同一套 VRMT 协议：TLS 1.2、Hello、心跳、JPEG 画面、键鼠。

这一版面向已经登录的 X11 桌面。Ubuntu 22.04 默认的 Wayland 会话、登录界面和锁屏不在范围内。

## 目录

```
RemoteClient/
  CMakeLists.txt
  README.md
  src/                 被控端代码
    protocol/          从 Windows 工程原样拿来的帧和消息，保证两边组包一致
  build/RemoteClient   编译结果
```

设备号和证书不在这个目录里，在 `~/.config/RemoteClient/`：

| 文件 | 内容 |
| --- | --- |
| `device-id` | 第一次启动生成的设备号，之后不变 |
| `cert.pem` | 自签证书，带服务器认证和客户端认证 |
| `key.pem` | 私钥，权限 600 |

指纹是证书 DER 的 SHA-256，64 位小写十六进制，算法和 Windows 端一致。

## 编译

系统是 Ubuntu 22.04，已有这些开发包：`build-essential`、`cmake`、`libssl-dev`、`libx11-dev`、`libxext-dev`、`libxtst-dev`、`libjpeg-dev`。

```bash
cd /home/hs/testCode/RemoteControl/RemoteClient
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j 4
```

## 运行

在已经登录的图形会话下启动。会弹出被控端窗口，显示设备号和状态。程序自己会找 `DISPLAY`，没有的话再用 `/run/user/1000/gdm/Xauthority` 试 `:1` 和 `:0`。启动时如果同一份程序已经在跑，会先结束它，再听 `19102`，并在 UDP `19101` 上应答中控台的局域网扫描。

只等中控台来连：

```bash
./build/RemoteClient
```

同时反连一台中控台（中控台监听端口默认 19100）：

```bash
./build/RemoteClient 192.168.16.10
./build/RemoteClient 192.168.16.10 19100
```

启动后标准错误会打印桌面尺寸、设备号、指纹和监听地址。同一时间只保持一路会话，断开后继续听。

## 从中控台连接

中控台直连端口固定是 **19102**。添加设备时地址填这台机器的 IP，例如 `192.168.16.33`，不要带端口。

连上之后：

1. 被控端先发 Hello。
2. 中控台订阅主码流或缩略图之后才开始发 JPEG。
3. 操作申请会直接同意，不弹本机确认，也不核对验证码。
4. 取得操作权之后，键鼠按画面坐标注入当前桌面。

## 这一版有的和没有的

有：

- 直连监听 `0.0.0.0:19102`
- 局域网发现：应答中控台发到 UDP `19101` 的扫描
- 被控端窗口：设备号、指纹、桌面、连接状态
- 可选反连中控台
- TLS 1.2 客户端握手（中控台始终是 TLS 服务端）
- 整屏 JPEG，长边、质量和帧率跟中控台的订阅
- 鼠标移动、左右中键、滚轮，以及常用 Windows 虚拟键
- 光标位置

没有：

- 识别码通道（`remote.subapi.fun`）
- H.264 和原图 BGRA
- 验证码、来访确认、无人值守密码
- 文件、剪贴板、聊天、关机
- Wayland、登录界面、锁屏
- 现场有人动鼠标时暂停注入

帧率上限按订阅来，但最高每秒 30 帧，避免软编码把机器占满。
