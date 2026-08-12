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

环境要求：Android NDK、CMake 和 Ninja。Windows 下可参考 build.bat **修改目录**后执行：

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
adb shell chmod 755 /data/local/tmp/unlockd
adb shell /data/local/tmp/unlockd
```

## 封装为 Magisk 模块

`unlockd` 可以通过 Magisk 在开机后自动启动。先编译 unlockd ，再执行：

```bat
cd unlockd
build-module.bat
```

生成的模块位于 `unlockd/build/unlockd-magisk.zip`，可在 Magisk 或兼容的平台安装。模块会将 unlockd 挂载到 `/system/bin/unlockd` ，等待 Android 完成开机后启动，并将输出记录到 `/data/adb/unlockd.log`。

当前模块内的 daemon 架构为 `arm64-v8a`；其他 ABI 需要分别编译并替换模块中的二进制。

程序启动后应看到：

```text
discovery listening on UDP port 8765
unlockd listening on TCP port 8765
```

## 编译 B

在 Android Studio 中打开 `App` 目录，或配置好环境后在 Windows 命令行执行：

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

## 快捷解锁

App 提供三种快捷入口。它们都会在后台发送解锁请求，不会打开主界面；只有未配置 PIN 时才会跳转到主页提示配置 PIN。

### 外部 Intent 调用

后台 Service 的信息如下：

```text
包名：com.onedongua.unlockmyphone
Service：com.onedongua.unlockmyphone.UnlockRequestService
Action：com.onedongua.unlockmyphone.action.UNLOCK_REQUEST
```

推荐使用显式 Intent 调用 Service。Java 示例：

```java
Intent intent = new Intent();
intent.setComponent(new ComponentName(
        "com.onedongua.unlockmyphone",
        "com.onedongua.unlockmyphone.UnlockRequestService"));
intent.setAction("com.onedongua.unlockmyphone.action.UNLOCK_REQUEST");
context.startService(intent);
```

Kotlin 示例：

```kotlin
val intent = Intent().apply {
    component = ComponentName(
        "com.onedongua.unlockmyphone",
        "com.onedongua.unlockmyphone.UnlockRequestService"
    )
    action = "com.onedongua.unlockmyphone.action.UNLOCK_REQUEST"
}
startService(intent)
```

也可以使用 ADB 测试：

```bash
adb shell am startservice \
  -n com.onedongua.unlockmyphone/.UnlockRequestService \
  -a com.onedongua.unlockmyphone.action.UNLOCK_REQUEST
```

不需要传递 PIN、IP 或其他 Extra。App 会读取已保存的配置；如果没有保存 IP，会尝试自动发现设备。Android 8.0 及以上对后台 Service 有限制，外部 App 应在用户操作期间发起调用；如果调用方处于后台，需根据自身任务改用前台 Service 或其他系统允许的后台执行方式。

### 应用快捷方式

将 App 的静态快捷方式添加到桌面或启动器时，填写以下字段：

```text
类型：Activity
Package：com.onedongua.unlockmyphone
Class：com.onedongua.unlockmyphone.MainActivity
Action：com.onedongua.unlockmyphone.action.UNLOCK_REQUEST
Extras：无
```

通过此方法启动的 Activity 启动后会自动退出到后台。

### Quick Settings 磁贴

安装 App 后，在系统的“编辑快捷设置”中添加“发送解锁请求”磁贴。点击磁贴会直接启动后台 Service，不会展开或打开 App 主界面。

部分系统会限制磁贴启动后台任务；如果点击后没有执行，请确认 App 未被系统限制后台运行，并检查 A 设备的 TCP `8765` 端口是否可达。

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
