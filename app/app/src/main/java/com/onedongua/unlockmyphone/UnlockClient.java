package com.onedongua.unlockmyphone;
import android.util.Log;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Inet4Address;
import java.net.InterfaceAddress;
import java.net.NetworkInterface;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.util.Enumeration;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicReference;

public final class UnlockClient {

    private static final int PORT = 8765;
    private static final String DISCOVERY_MESSAGE = "DISCOVER_UNLOCKD";
    private static final String DISCOVERY_RESPONSE = "UNLOCKD";
    private static final int DISCOVERY_TIMEOUT_MS = 250;
    private static final int DISCOVERY_SCAN_TIMEOUT_MS = 3000;
    private static final int DISCOVERY_THREADS = 32;
    private static final long MAX_DISCOVERY_HOSTS = 65534;
    private static final int UNLOCK_TIMEOUT_MS = 15000;

    private final String host;

    public UnlockClient(String host) {
        this.host = host;
    }

    /**
     * 请求目标手机解锁。
     *
     * @return 服务端返回的状态，例如 OK 或 ALREADY_UNLOCKED
     */
    public String unlock(String encryptedPin) throws Exception {
        String request = "unlock " + encryptedPin + "\n";

        try (Socket socket = new Socket()) {
            socket.connect(
                    new InetSocketAddress(host, PORT),
                    UNLOCK_TIMEOUT_MS
            );

            socket.setSoTimeout(UNLOCK_TIMEOUT_MS);

            PrintWriter writer = new PrintWriter(
                    new OutputStreamWriter(
                            socket.getOutputStream(),
                            StandardCharsets.UTF_8
                    ),
                    true
            );

            writer.print(request);
            writer.flush();

            BufferedReader reader = new BufferedReader(
                    new InputStreamReader(
                            socket.getInputStream(),
                            StandardCharsets.UTF_8
                    )
            );

            String response = reader.readLine();

            return response;
        }
    }

    public static String discoverDevice() throws IOException {
        Network network = findLocalNetwork();
        long firstHost = network.networkAddress + 1;
        long lastHost = network.broadcastAddress - 1;
        long hostCount = lastHost - firstHost + 1;

        if (hostCount <= 0 || hostCount > MAX_DISCOVERY_HOSTS) {
            throw new IOException("local network is too large to scan: /" + network.prefixLength);
        }

        byte[] data = DISCOVERY_MESSAGE.getBytes(StandardCharsets.UTF_8);
        AtomicReference<String> discoveredIp = new AtomicReference<>();
        CountDownLatch found = new CountDownLatch(1);
        int threadCount = (int) Math.min(DISCOVERY_THREADS, hostCount);
        ExecutorService executor = Executors.newFixedThreadPool(threadCount);

        for (long address = firstHost; address <= lastHost; address++) {
            final long candidate = address;
            executor.execute(() -> {
                probe(candidate, data, discoveredIp, found);
            });
        }

        try {
            found.await(DISCOVERY_SCAN_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Thread.currentThread().interrupt();
        } finally {
            executor.shutdownNow();
        }

        return discoveredIp.get();
    }

    private static void probe(
            long address,
            byte[] data,
            AtomicReference<String> discoveredIp,
            CountDownLatch found) {
        if (discoveredIp.get() != null) return;

        try (DatagramSocket socket = new DatagramSocket()) {
            socket.setSoTimeout(DISCOVERY_TIMEOUT_MS);
            InetAddress target = ipv4Address(address);
            socket.send(new DatagramPacket(data, data.length, target, PORT));

            byte[] buffer = new byte[128];
            DatagramPacket response = new DatagramPacket(buffer, buffer.length);
            socket.receive(response);

            String message = new String(
                    response.getData(),
                    response.getOffset(),
                    response.getLength(),
                    StandardCharsets.UTF_8);
            Log.i("UnlockClient", "probe: " + message);
            if (DISCOVERY_RESPONSE.equals(message)) {
                if (discoveredIp.compareAndSet(null, response.getAddress().getHostAddress())) {
                    found.countDown();
                }
            }
        } catch (IOException ignored) {
            // Most scanned addresses will not have a UDP listener.
        }
    }

    private static Network findLocalNetwork() throws IOException {
        Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
        Network fallback = null;
        if (interfaces != null) {
            while (interfaces.hasMoreElements()) {
                NetworkInterface networkInterface = interfaces.nextElement();
                try {
                    if (!networkInterface.isUp() || networkInterface.isLoopback()) continue;
                } catch (IOException e) {
                    continue;
                }

                for (InterfaceAddress interfaceAddress : networkInterface.getInterfaceAddresses()) {
                    InetAddress address = interfaceAddress.getAddress();
                    short prefixLength = interfaceAddress.getNetworkPrefixLength();
                    if (!(address instanceof Inet4Address) || prefixLength <= 0 || prefixLength >= 32) {
                        continue;
                    }

                    long ip = ipv4Number(address);
                    long mask = 0xffffffffL << (32 - prefixLength);
                    Network network = new Network(
                            ip & mask,
                            (ip & mask) | (~mask & 0xffffffffL),
                            prefixLength);
                    if (address.isSiteLocalAddress()) return network;
                    if (fallback == null) fallback = network;
                }
            }
        }
        if (fallback != null) return fallback;
        throw new IOException("no local IPv4 network found");
    }

    private static long ipv4Number(InetAddress address) {
        byte[] bytes = address.getAddress();
        return ((bytes[0] & 0xffL) << 24)
                | ((bytes[1] & 0xffL) << 16)
                | ((bytes[2] & 0xffL) << 8)
                | (bytes[3] & 0xffL);
    }

    private static InetAddress ipv4Address(long address) throws IOException {
        return InetAddress.getByAddress(new byte[] {
                (byte) (address >>> 24),
                (byte) (address >>> 16),
                (byte) (address >>> 8),
                (byte) address
        });
    }

    private static final class Network {
        private final long networkAddress;
        private final long broadcastAddress;
        private final short prefixLength;

        private Network(long networkAddress, long broadcastAddress, short prefixLength) {
            this.networkAddress = networkAddress;
            this.broadcastAddress = broadcastAddress;
            this.prefixLength = prefixLength;
        }
    }

}
