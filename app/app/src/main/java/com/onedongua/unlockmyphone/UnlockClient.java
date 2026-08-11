package com.onedongua.unlockmyphone;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;

public final class UnlockClient {

    private static final int PORT = 8765;
    private static final int TIMEOUT_MS = 3000;

    private final String host;
    private final String secret;

    public UnlockClient(String host, String secret) {
        this.host = host;
        this.secret = secret;
    }

    /**
     * 请求目标手机解锁。
     *
     * @return true 表示目标手机接受了请求
     */
    public boolean unlock() throws Exception {
        String token = generateToken(System.currentTimeMillis() / 1000L);

        String request = "unlock " + token + "\n";

        try (Socket socket = new Socket()) {
            socket.connect(
                    new InetSocketAddress(host, PORT),
                    TIMEOUT_MS
            );

            socket.setSoTimeout(TIMEOUT_MS);

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

            return "OK".equals(response);
        }
    }

    /**
     * 生成当前时间窗口的 Token。
     */
    private String generateToken(long unixTimeSeconds) throws Exception {
        long minute = unixTimeSeconds / 60;

        String input = secret + minute;

        MessageDigest digest = MessageDigest.getInstance("SHA-256");

        byte[] hash = digest.digest(
                input.getBytes(StandardCharsets.UTF_8)
        );

        return bytesToHex(hash).substring(0, 8);
    }

    public static String discoverDevice() throws IOException {
        final int PORT = 8766;
        final String MESSAGE = "DISCOVER_UNLOCKD";

        try (DatagramSocket socket = new DatagramSocket()) {
            socket.setBroadcast(true);
            socket.setSoTimeout(1000);

            byte[] data = MESSAGE.getBytes(StandardCharsets.UTF_8);

            DatagramPacket packet = new DatagramPacket(
                    data,
                    data.length,
                    InetAddress.getByName("255.255.255.255"),
                    PORT
            );

            socket.send(packet);

            byte[] buffer = new byte[128];

            DatagramPacket response = new DatagramPacket(
                    buffer,
                    buffer.length
            );

            socket.receive(response);

            String message = new String(
                    response.getData(),
                    response.getOffset(),
                    response.getLength(),
                    StandardCharsets.UTF_8
            );

            if ("UNLOCKD".equals(message)) {
                return response.getAddress().getHostAddress();
            }

            return null;
        }
    }

    private static String bytesToHex(byte[] bytes) {
        char[] hex = "0123456789abcdef".toCharArray();

        char[] result = new char[bytes.length * 2];

        for (int i = 0; i < bytes.length; i++) {
            int value = bytes[i] & 0xff;

            result[i * 2] = hex[value >>> 4];
            result[i * 2 + 1] = hex[value & 0x0f];
        }

        return new String(result);
    }
}