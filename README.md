# UnlockMyPhone

UnlockMyPhone 由两个部分组成：

- **A：`unlockd`**：运行在需要解锁的 Android 设备上，提供 TCP 解锁服务和 UDP 局域网发现服务。
- **B：Android App**：运行在控制设备上，扫描局域网并向 A 发送解锁请求。

## 工作流程

1. B 获取自己的局域网 IPv4 地址和子网前缀。
2. B 推算局域网网段，并发扫描网段内主机的 UDP `8765` 端口。
3. A 收到 `DISCOVER_UNLOCKD` 后记录来源 IP，并返回 `UNLOCKD`。
4. B 使用返回的 IP 连接 A 的 TCP `8765` 端口。
5. B 发送加密 PIN，A 校验后执行解锁并返回结果。

UDP 和 TCP 使用相同的端口号是允许的，因为两者属于独立的传输协议：

- UDP `8765`：设备发现
- TCP `8765`：解锁请求

## 编译 A

环境要求：Android NDK、CMake 和 Ninja。Windows 下可直接执行：

```bat
cd unlockd
build.bat
```

脚本默认构建 `arm64-v8a`，Android API 为 23。生成的可执行文件位于：

```text
unlockd/build/unlockd
```

将它推送到 A 后运行，例如：

```bash
adb push unlockd/build/unlockd /data/local/tmp/unlockd
adb shell chmod 700 /data/local/tmp/unlockd
adb shell /data/local/tmp/unlockd
```

A 需要具备执行输入事件和读取锁屏状态所需的权限。程序启动后应看到：

```text
discovery listening on UDP port 8765
unlockd listening on TCP port 8765
```

## 编译 B

在 Android Studio 中打开 `App` 目录，或在 Windows 命令行执行：

```bat
cd App
gradlew.bat assembleDebug
```

APK 通常生成于：

```text
App/app/build/outputs/apk/debug/app-debug.apk
```

安装：

```bash
adb install -r App/app/build/outputs/apk/debug/app-debug.apk
```

B 需要和 A 连接到同一个局域网，并拥有 `INTERNET` 权限。点击“重新扫描”可以重新执行设备发现。

## 网络协议

UDP 发现请求：

```text
DISCOVER_UNLOCKD
```

UDP 响应：

```text
UNLOCKD
```

TCP 解锁请求格式：

```text
unlock <encrypted_pin>\n
```

成功或已解锁时，A 返回：

```text
OK
ALREADY_UNLOCKED
```

## 注意事项

- UDP 发现没有身份认证，局域网内其他设备可以伪造 `UNLOCKD` 响应；B 后续仍会通过 TCP 连接并发送加密 PIN。
- A 和 B 必须位于可互通的同一 IPv4 局域网。访客网络、AP 隔离或防火墙可能阻止设备间通信。
- B 当前会根据网卡子网前缀扫描主机；过大的网段会被拒绝扫描。
- 不要将 A 的 TCP `8765` 端口暴露到公网。
