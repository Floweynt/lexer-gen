final class Source {
    private final byte[] data;
    private int cur, la, tokStart;
    private long bytesPos = 0, colPos = 1, linePos = 1;
    private long laBytes = 0, laCol = 1, laLine = 1;

    Source(byte[] data) {
        this.data = data;
    }

    int peek() {
        if (la >= data.length) {
            return 0;
        }

        int ch = data[la++] & 0xFF;
        laBytes++;
        laCol++;
        if (ch == '\n') {
            laLine++;
            laCol = 1;
        }

        return ch;
    }

    void accept() {
        cur = la;
        bytesPos = laBytes;
        colPos = laCol;
        linePos = laLine;
    }

    void backtrack() {
        la = cur;
        laBytes = bytesPos;
        laCol = colPos;
        laLine = linePos;
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

    long col() {
        return colPos;
    }

    long line() {
        return linePos;
    }
}
