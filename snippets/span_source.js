class SpanSource
{
    constructor(data)
    {
        this.data = data;
        this.cur = 0;
        this.la = 0;
        this.tokStart = 0;
        this.bytesPos = 0;
        this.laBytes = 0;
    }

    peek()
    {
        if (this.la >= this.data.length)
        {
            return 0;
        }

        const ch = this.data[this.la++];
        this.laBytes++;

        return ch;
    }

    accept()
    {
        this.cur = this.la;
        this.bytesPos = this.laBytes;
    }

    backtrack()
    {
        this.la = this.cur;
        this.laBytes = this.bytesPos;
    }

    startToken() { this.tokStart = this.cur; }

    text() { return Buffer.from(this.data.slice(this.tokStart, this.cur)).toString("utf8"); }

    bytes() { return this.bytesPos; }
}
