class SpanSource:
    def __init__(self, data: bytes):
        self.data = data
        self.cur = 0
        self.la = 0
        self.tok_start = 0
        self.bytes_pos = 0
        self.col_pos = 1
        self.line_pos = 1
        self.la_bytes = 0
        self.la_col = 1
        self.la_line = 1

    def peek(self):
        if self.la >= len(self.data):
            return 0

        ch = self.data[self.la]
        self.la += 1
        self.la_bytes += 1
        self.la_col += 1
        if ch == 10:
            self.la_line += 1
            self.la_col = 1

        return ch

    def accept(self):
        self.cur = self.la
        self.bytes_pos = self.la_bytes
        self.col_pos = self.la_col
        self.line_pos = self.la_line

    def backtrack(self):
        self.la = self.cur
        self.la_bytes = self.bytes_pos
        self.la_col = self.col_pos
        self.la_line = self.line_pos

    def start_token(self):
        self.tok_start = self.cur

    def text(self):
        return self.data[self.tok_start : self.cur].decode("utf-8", errors="replace")

    def bytes(self):
        return self.bytes_pos

    def col(self):
        return self.col_pos

    def line(self):
        return self.line_pos
