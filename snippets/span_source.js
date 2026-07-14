class SpanSource
{
    constructor(data)
    {
        this.data = data;
        this.cur = 0;
        this.la = 0;
        this.tokStart = 0;
        this.bytesPos = 0;
        this.colPos = 1;
        this.linePos = 1;
        this.laBytes = 0;
        this.laCol = 1;
        this.laLine = 1;
    }

    peek()
    {
        if (this.la >= this.data.length)
        {
            return 0;
        }

        const ch = this.data[this.la++];
        this.laBytes++;
        this.laCol++;
        if (ch === 10)
        {
            this.laLine++;
            this.laCol = 1;
        }

        return ch;
    }

    accept()
    {
        this.cur = this.la;
        this.bytesPos = this.laBytes;
        this.colPos = this.laCol;
        this.linePos = this.laLine;
    }

    backtrack()
    {
        this.la = this.cur;
        this.laBytes = this.bytesPos;
        this.laCol = this.colPos;
        this.laLine = this.linePos;
    }

    startToken() { this.tokStart = this.cur; }

    text() { return Buffer.from(this.data.slice(this.tokStart, this.cur)).toString("utf8"); }

    bytes() { return this.bytesPos; }
    col() { return this.colPos; }
    line() { return this.linePos; }
}
