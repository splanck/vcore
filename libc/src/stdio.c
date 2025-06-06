#include <stdio.h>
#include <lib.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct FILE {
    int fd;
    long pos;
};

static int udecimal_to_string(char *buffer, int position, unsigned long long d)
{
    char digits_map[10] = "0123456789";
    char digits_buffer[25];
    int size = 0;
    do {
        digits_buffer[size++] = digits_map[d % 10];
        d /= 10;
    } while (d != 0);
    for (int i = size - 1; i >= 0; i--)
        buffer[position++] = digits_buffer[i];
    return size;
}

static int udecimal_to_string_n(char *buffer, size_t limit, int position, unsigned long long d)
{
    char digits_map[10] = "0123456789";
    char digits_buffer[25];
    int size = 0;
    do {
        digits_buffer[size++] = digits_map[d % 10];
        d /= 10;
    } while (d != 0);
    for (int i = size - 1; i >= 0; i--) {
        if (position + 1 < (int)limit)
            buffer[position] = digits_buffer[i];
        position++;
    }
    return size;
}

static int decimal_to_string(char *buffer, int position, long long d)
{
    int size = 0;
    if (d < 0) {
        d = -d;
        buffer[position++] = '-';
        size = 1;
    }
    size += udecimal_to_string(buffer, position, (unsigned long long)d);
    return size;
}

static int decimal_to_string_n(char *buffer, size_t limit, int position, long long d)
{
    int size = 0;
    if (d < 0) {
        d = -d;
        if (position + 1 < (int)limit)
            buffer[position] = '-';
        position++;
        size = 1;
    }
    size += udecimal_to_string_n(buffer, limit, position, (unsigned long long)d);
    return size;
}

static int hex_to_string(char *buffer, int position, unsigned long long d)
{
    char digits_buffer[25];
    char digits_map[16] = "0123456789ABCDEF";
    int size = 0;
    do {
        digits_buffer[size++] = digits_map[d % 16];
        d /= 16;
    } while (d != 0);
    for (int i = size - 1; i >= 0; i--)
        buffer[position++] = digits_buffer[i];
    buffer[position++] = 'H';
    return size + 1;
}

static int hex_to_string_n(char *buffer, size_t limit, int position, unsigned long long d)
{
    char digits_buffer[25];
    char digits_map[16] = "0123456789ABCDEF";
    int size = 0;
    do {
        digits_buffer[size++] = digits_map[d % 16];
        d /= 16;
    } while (d != 0);
    for (int i = size - 1; i >= 0; i--) {
        if (position + 1 < (int)limit)
            buffer[position] = digits_buffer[i];
        position++;
    }
    if (position + 1 < (int)limit)
        buffer[position] = 'H';
    position++;
    return size + 1;
}

static int format_buffer(char *buffer, const char *fmt, va_list args)
{
    int pos = 0;
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            buffer[pos++] = fmt[i];
        } else {
            switch (fmt[++i]) {
            case 'x':
                pos += hex_to_string(buffer, pos, va_arg(args, unsigned long long));
                break;
            case 'u':
                pos += udecimal_to_string(buffer, pos, va_arg(args, unsigned long long));
                break;
            case 'd':
                pos += decimal_to_string(buffer, pos, va_arg(args, long long));
                break;
            case 's': {
                char *s = va_arg(args, char*);
                while (*s)
                    buffer[pos++] = *s++;
                break; }
            default:
                buffer[pos++] = '%';
                i--;
            }
        }
    }
    return pos;
}

static int format_buffer_n(char *buffer, size_t limit, const char *fmt, va_list args)
{
    size_t pos = 0;
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            if (pos + 1 < limit)
                buffer[pos] = fmt[i];
            pos++;
        } else {
            switch (fmt[++i]) {
            case 'x':
                pos += hex_to_string(buffer, pos, va_arg(args, unsigned long long));
                break;
            case 'u':
                pos += udecimal_to_string(buffer, pos, va_arg(args, unsigned long long));
                break;
            case 'd':
                pos += decimal_to_string(buffer, pos, va_arg(args, long long));
                break;
            case 's': {
                char *s = va_arg(args, char*);
                while (*s) {
                    if (pos + 1 < limit)
                        buffer[pos] = *s;
                    pos++; s++; }
                break; }
            default:
                if (pos + 1 < limit)
                    buffer[pos] = '%';
                pos++; i--;
            }
        }
    }
    if (limit)
        buffer[(pos < limit ? pos : limit - 1)] = '\0';
    return pos;
}

int sprintf(char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = format_buffer(str, format, args);
    va_end(args);
    str[len] = '\0';
    return len;
}

int snprintf(char *str, size_t size, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int len = format_buffer_n(str, size, format, args);
    va_end(args);
    return len;
}

FILE *fopen(const char *path, const char *mode)
{
    (void)mode;
    int fd = open_file((char*)path);
    if (fd == -1)
        return NULL;
    FILE *f = malloc(sizeof(FILE));
    if (!f)
        return NULL;
    f->fd = fd;
    f->pos = 0;
    return f;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int r = read_file(stream->fd, ptr, size * nmemb);
    if (r < 0)
        return 0;
    stream->pos += r;
    return (size_t)r / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream)
{
    int r = write_file(stream->fd, (void*)ptr, size * nmemb);
    if (r < 0)
        return 0;
    stream->pos += r;
    return (size_t)r / size;
}

int fseek(FILE *stream, long offset, int whence)
{
    if (!stream) {
        errno = EINVAL;
        return -1;
    }

    long size = get_file_size(stream->fd);
    if (size < 0) {
        errno = EIO;
        return -1;
    }

    long target;
    switch (whence) {
    case SEEK_SET:
        target = offset;
        break;
    case SEEK_CUR:
        target = stream->pos + offset;
        break;
    case SEEK_END:
        target = size + offset;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    if (target < stream->pos || target < 0 || target > size) {
        errno = EINVAL;
        return -1;
    }

    char buf[64];
    while (stream->pos < target) {
        long chunk = target - stream->pos;
        if (chunk > (long)sizeof(buf))
            chunk = sizeof(buf);
        int r = read_file(stream->fd, buf, (int)chunk);
        if (r != chunk) {
            errno = EIO;
            return -1;
        }
        stream->pos += r;
    }

    return 0;
}

long ftell(FILE *stream)
{
    if (!stream) {
        errno = EINVAL;
        return -1L;
    }
    return stream->pos;
}

int fclose(FILE *stream)
{
    if (!stream)
        return -1;
    close_file(stream->fd);
    free(stream);
    return 0;
}

