final class Source {
    private final byte[] data;
    private int cur, la, tokStart;
    private long bytesPos = 0;
    private long laBytes = 0;

    Source(byte[] data) {
        this.data = data;
    }

    int peek() {
        if (la >= data.length) {
            return 0;
        }

        int ch = data[la++] & 0xFF;
        laBytes++;

        return ch;
    }

    void accept() {
        cur = la;
        bytesPos = laBytes;
    }

    void backtrack() {
        la = cur;
        laBytes = bytesPos;
    }

    void startToken() {
        tokStart = cur;
    }

    String text() {
        return new String(data, tokStart, cur - tokStart, java.nio.charset.StandardCharsets.UTF_8);
    }

    long bytes() {
        return bytesPos;
    }
}
