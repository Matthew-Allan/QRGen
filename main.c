#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

typedef struct QRCodeInfo {
    uint16_t mods;  // Size in bits of a data segment.
    uint16_t size;  // Size in bytes of a data segment.
    uint8_t  vers;  // Version number.
    uint8_t  side;  // Length of the sides of the QR code.
} QRCodeInfo;

#define QRData(qr_info) ((uint8_t *) ((QRCodeInfo *) (qr_info) + 1))
#define QRMask(qr_info) (QRData(qr_info) + (qr_info)->size);

#define QRIndex(qr_info, x, y) (((uint16_t) (y) * ((QRCodeInfo *) (qr_info))->side) + (x))

#define biToBy(bits) ((bits + 7) / 8)

QRCodeInfo *createQRInfo(int version) {
    if(version < 0 || version > 40) {
        return NULL;
    }

    uint8_t side_len = 4 * version + 17;
    uint16_t bit_size = (uint16_t) side_len * side_len;
    uint16_t byte_size = biToBy(bit_size);

    QRCodeInfo *qr_info = (QRCodeInfo *) malloc(sizeof(QRCodeInfo) + (byte_size * 2));
    if(qr_info == NULL) {
        return NULL;
    }

    memset(QRData(qr_info), 0, byte_size * 2);

    qr_info->vers = version;
    qr_info->side = side_len;
    qr_info->size = byte_size;
    qr_info->mods = bit_size;

    return qr_info;
}

void drawLineAt(QRCodeInfo *qr_info, uint8_t line, uint8_t line_mask, uint8_t x, uint8_t y) {
    uint8_t *data = QRData(qr_info), *mask = QRMask(qr_info);
    uint16_t index = QRIndex(qr_info, x, y);

    uint8_t data_mask = line_mask << index % 8;
    if(data_mask) {
        data[index / 8] = (~data_mask & data[index / 8]) | line << index % 8;   
        mask[index / 8] |= data_mask;
    }
    
    data_mask = line_mask >> (8 - (index % 8));
    if(data_mask) {
        data[index / 8 + 1] = (~data_mask & data[index / 8 + 1]) | line >> (8 - (index % 8));
        mask[index / 8 + 1] |= data_mask;
    }
}

#define RGT 0b01
#define BOT 0b10

#define TOP_LFT 0
#define TOP_RGT RGT
#define BOT_LFT BOT

void drawPosition(QRCodeInfo *qr_info, uint8_t position) {
    const uint8_t mark[] = {0b01111111, 0b01000001, 0b01011101, 0b01011101, 0b01011101, 0b01000001, 0b01111111};

    uint8_t pos_x = position & RGT ? qr_info->side - 8 : 0;
    uint8_t pos_y = position & BOT ? qr_info->side - 7 : 0;

    drawLineAt(qr_info, 0, 0xff, pos_x, pos_y + (position & BOT ? -1 : 7));
    for(int y = 0; y < 7; y++) {
        drawLineAt(qr_info, mark[y] << (position & RGT), 0xff, pos_x, pos_y + y);
    }
}

void drawAlignment(QRCodeInfo *qr_info, uint8_t pos_x, uint8_t pos_y) {
    const uint8_t mark[] = {0b00011111, 0b00010001, 0b00010101, 0b00010001, 0b00011111};
    for(int y = 0; y < 5; y++) {
        drawLineAt(qr_info, mark[y], 0x1F, pos_x - 2, pos_y + y - 2);
    }
}

void drawAlignments(QRCodeInfo *qr_info) {
    if(qr_info->vers == 1) {
        return;
    }

    uint8_t cols = ((qr_info->vers + 7) / 7) + 1;
    uint8_t len = qr_info->side - 13;
    uint8_t step = (uint8_t) roundf((float) len / (cols - 1));
    step += step & 1;

    uint8_t locs[cols];
    locs[0] = 6;
    for(uint8_t i = 0; i < cols - 1; i++) {
        locs[cols - i - 1] = len + 6 - (i * step);
    }

    for(uint8_t x = 0; x < cols; x++) {
        uint8_t pos_x = locs[x];
        for(uint8_t y = 0; y < cols; y++) {
            if((x == 0 && y == 0) || (x == 0 && y == cols - 1) || (x == cols - 1 && y == 0)) {
                continue;
            }
            drawAlignment(qr_info, pos_x, locs[y]);
        }
    }
}

void drawDots(QRCodeInfo *qr_info) {
    const uint8_t line = 0b01010101;
    uint8_t length = qr_info->side - 16;
    uint8_t end = length % 8;

    for(int i = 0; i < length / 8; i++) {
        drawLineAt(qr_info, line, 0xff, 8 + (i * 8), 6);
    }
    drawLineAt(qr_info, line, (1 << end) - 1, 8 + length - end, 6);

    uint8_t *data = QRData(qr_info), *mask = QRMask(qr_info);
    for(int i = 0; i < length; i++) {
        uint16_t index = QRIndex(qr_info, 6, 8 + i);
        uint8_t line_mask = 1 << (index % 8);
        data[index / 8] = (~line_mask & data[index / 8]) | (i + 1 % 2) << index % 8;
        mask[index / 8] |= line_mask;
    }
}

void drawPositions(QRCodeInfo *qr_info) {
    drawPosition(qr_info, TOP_RGT);
    drawPosition(qr_info, TOP_LFT);
    drawPosition(qr_info, BOT_LFT);
}

void printQR(QRCodeInfo *qr_info) {
    uint8_t *data = QRData(qr_info), *mask = QRMask(qr_info);
    uint8_t data_val, mask_val;
    for(int i = 0; i < qr_info->mods; i++) {
        if(i % 8 == 0) {
            data_val = data[i / 8], mask_val = mask[i / 8];
        }
        printf("%s%s", mask_val & 1 ? data_val & 1 ? "##" : "  " : "~~", i % qr_info->side == qr_info->side - 1 ? "\n" : "");
        mask_val >>= 1; data_val >>= 1;
    }
}

int main(int argc, char const *argv[]) {
    QRCodeInfo *info = createQRInfo(15);
    drawPositions(info);
    drawDots(info);
    drawAlignments(info);
    printQR(info);
    free(info);
    return 0;
}