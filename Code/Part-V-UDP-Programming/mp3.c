#include "mp3.h"

int parse_mp3_header(unsigned char header[4], struct mp3_frame *frame)
{
    uint32_t h;

    h = ((uint32_t)header[0] << 24) | ((uint32_t)header[1] << 16) |
        ((uint32_t)header[2] << 8) | header[3];

    /* MPEG audio sync */
    if (((h >> 21) & 0x7ff) != 0x7ff) return -1;
    /* MPEG-1 */
    if (((h >> 19) & 0x3) != 3) return -1;
    /* Layer III */
    if (((h >> 17) & 0x3) != 1) return -1;

    int bitrate_index = (h >> 12) & 0xf;
    int samplerate_index = (h >> 10) & 0x3;
    if (bitrate_index == 0 || bitrate_index == 15) return -1;
    if (samplerate_index == 3) return -1;

    frame->bitrate = bitrate_table[bitrate_index] * 1000;
    frame->sample_rate = samplerate_table[samplerate_index];
    frame->padding = (h >> 9) & 1;

    /* MPEG-1 Layer III frame size:
     *     144 * bitrate / sample_rate + padding */
    frame->frame_size = (144 * frame->bitrate) / frame->sample_rate + frame->padding;
    return 0;
}

int read_mp3_frame(int fd, unsigned char *buffer, size_t buffer_size, struct mp3_frame *frame)
{
    unsigned char header[4];
    ssize_t n;

    n = read(fd, header, sizeof(header));
    if (n == 0) 
    if (n != sizeof(header)) return -1;
    if (parse_mp3_header(header, frame) == -1) return -1;
    if ((size_t)frame->frame_size > buffer_size) return -1;

    memcpy(buffer, header, sizeof(header));
    n = read(fd, buffer + sizeof(header), frame->frame_size - sizeof(header));
    if (n != frame->frame_size - sizeof(header))
        return -1;

    return frame->frame_size;
}

int find_first_mp3_frame(int fd)
{
    unsigned char header[4];

    if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
        return -1;

    unsigned char id3[10];
    if (read(fd, id3, sizeof(id3)) != sizeof(id3))
        return -1;

    if (id3[0] == 'I' && id3[1] == 'D' && id3[2] == '3') {
        unsigned int size;

        size = ((id3[6] & 0x7f) << 21) | ((id3[7] & 0x7f) << 14) |
               ((id3[8] & 0x7f) << 7)  | (id3[9] & 0x7f);

        off_t offset = 10 + size;
        if (id3[5] & 0x10) offset += 10;
        if (lseek(fd, offset, SEEK_SET) == (off_t)-1) return -1;
    } else {
        if (lseek(fd, 0, SEEK_SET) == (off_t)-1)
            return -1;
    }

    /* Verify that the current position contains
     * an MPEG-1 Layer III frame */
    if (read(fd, header, sizeof(header)) != sizeof(header))
        return -1;

    struct mp3_frame frame;
    if (parse_mp3_header(header, &frame) == -1)
        return -1;

    /* Return to the beginning of the frame */
    if (lseek(fd, -4, SEEK_CUR) == (off_t)-1)
        return -1;

    return 0;
}   
