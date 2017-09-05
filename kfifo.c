struct kfifo{
    uint8_t *buffer;
    uint32_t in; // 输入指针
    uint32_t out; // 输出指针
    uint32_t size; // 缓冲区大小，必须为2的次幂
}

/*判断n是否为2的幂*/
static bool is_power_of_2(unsigned int n)
{
    return (n != 0 && ((n & (n - 1)) == 0));
}

/*将数字a向上取整为2的次幂*/
static uint32_t roundup_power_of_2(uint32_t a)
{
    if (a == 0)
        return 0;

    uint32_t position = 0;
    for (int i = a; i != 0; i >>= 1)
        position++;

    return (uint32_t)(1 << position);
}


/*全局变量*/
struct kfifo fifo;

/*环形缓冲区初始化*/
void fifo_init(uint32_t size)
{
    if (!is_power_of_2(size))
        size = roundup_power_of_2(_size);

    fifo->buffer = (unsigned char *)(malloc(size * sizeof(unsigned char)));
    fifo->in = 0;
    fifo->out = 0;
    fifo->size = size;
}


/*返回实际写入缓冲区中的数据*/
uint32_t put(const uint8_t *data, uint32_t len)
{
    unsigned int l;

    /*当前缓冲区空闲空间*/
    len = min(len,fifo->size - fifo->in + fifo->out);

    /*当前in位置到buffer末尾的长度*/
    l = min(len, fifo->size - (fifo->in  & (fifo->size - 1)));

    /*首先复制数据到[in，buffer的末尾]*/
    memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), data, l);

    /*复制剩余的数据（如果有）到[buffer的起始位置,...]*/
    memcpy(fifo->buffer, data + l, len - l);

    fifo->in += len; // 直接加，不作模运算。当溢出时，从buffer的开始位置重新开始

    return len;
}


/*返回实际读取的数据长度*/
uint32_t get(uint8_t *data, uint32_t len)
{
    unsigned int l;

    /*缓冲区中的数据长度: 注意都是无符号数*/
    len = min(len, fifo->in - fifo->out);

    // 首先从[out,buffer end]读取数据
    l = min(len, fifo->size - (fifo->out & (fifo->size - 1)));
    memcpy(data, fifo->buffer + (fifo->out & (fifo->size - 1)), l);

    // 从[buffer start,...]读取数据
    memcpy(data + l, fifo->buffer, len - l);

    fifo->out += len; // 直接加，不错模运算。溢出后，从buffer的起始位置重新开始

    return len;
}













