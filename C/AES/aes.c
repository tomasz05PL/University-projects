#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "s-box.h"

// xtim is a times 2 operation in Galois Field GF(2^8)
u_int8_t xtim(u_int8_t a){
    // Multiplying number by 2
    u_int8_t result = a << 1;
    
    // Checking if initial number had MSB = 1, if yes we had overflow
    if (a & 0x80) {
        // 0x1b is special constant that we need to add to result to "remove" the overflow
        result = result ^ 0x1b;
    }
    return result;
}

// Multiplying by 9 in Galois Field
u_int8_t m9(u_int8_t a) {
    return xtim(xtim(xtim(a))) ^ a;
}

// Multiplying by 11 in Galois Field
u_int8_t mB(u_int8_t a) {
    return xtim(xtim(xtim(a))) ^ xtim(a) ^ a;
}

// Multiplying by 13 in Galois Field
u_int8_t mD(u_int8_t a) {
    return xtim(xtim(xtim(a))) ^ xtim(xtim(a)) ^ a;
}

// Multiplying by 14 in Galois Field
u_int8_t mE(u_int8_t a) {
    return xtim(xtim(xtim(a))) ^ xtim(xtim(a)) ^ xtim(a);
}

// Key expansion algorithm for AES-128
void key_expansion_128(const u_int8_t* key, u_int32_t* words){
    // Changing 128 bit key, which is composed of 16 bytes into four 32 bit words
    words[0] = (key[0]  << 24) | (key[1]  << 16) | (key[2]  << 8) | key[3];
    words[1] = (key[4]  << 24) | (key[5]  << 16) | (key[6]  << 8) | key[7];
    words[2] = (key[8]  << 24) | (key[9]  << 16) | (key[10] << 8) | key[11];
    words[3] = (key[12] << 24) | (key[13] << 16) | (key[14] << 8) | key[15];

    // Expanding key - for AES-128 we need 44 key words to have 11 128 bit round keys 

    // Temporary value to store current key word
    u_int32_t tmp = {0};
    
    // Every next word is generated based on previous word (we will call it word1) 
    // and one four places before (we will call it word2)
    // (eg. fifth word is based on fourth and first word)
    for(int i = 4; i < 44; ++i){
        // First we take word1
        tmp = words[i-1];

        // If i%4 == 0 (first word of next key) we do additional transformations
        if(i%4 == 0){
            // Rotating current tmp value by 8 bits left 
            // Bitwise rotation is operation where any bits that would go out of range are carried on the other way
            /*
            Example on 4 bits: 1101 rotate by 1 bit
            1. 1101 << 1 = 1010 (a)
            2. 1101 >> 3 = 0001 (b)
            3. a | b = 1010 | 0001 = 1011
            Here instead of shifting by 1 bit we shift by 1 byte (8 bits) but logic stays the same
            */
            tmp = (tmp << 8) | (tmp >> 24);

            // Changing bytes according to special constant substitute box defined in "s-box.h"
            // This step is called SubBytes
            // First we extract every byte of current word
            /*
            Example on 4 bits: extracting bits (not bytes in this case) from 1101
            1. 1101 >> 3 = 0001
            2. 0001 & 0001 = 1 (MSB bit)
            3. 1101 >> 2 = 0011 
            4. 0011 & 0001 = 1
            5. 1101 >> 1 = 0110
            6. 0110 & 0001 = 0
            7. 1101 & 0001 = 1 (LSB bit)
            On 32 bits it works the same but we shift by whole bytes and instead of 1 (bit max) we 0xFF = 11111111 (byte max)
            */
            u_int8_t b0 = (tmp >> 24) & 0xFF;
            u_int8_t b1 = (tmp >> 16) & 0xFF;
            u_int8_t b2 = (tmp >> 8) & 0xFF;
            u_int8_t b3 = (tmp) & 0xFF;

            // After extracting bytes we change every byte for a byte in s-box on index equal to initial byte value
            b0 = s_box[b0];
            b1 = s_box[b1];
            b2 = s_box[b2];
            b3 = s_box[b3];

            // Composing 32-bit number from bytes
            tmp = (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;

            // XOR-ing temporary word value with special constant array (round constant) which contains predefined values
            // Rcon is defined in "s-box.h"
            tmp = tmp ^ Rcon_128[i/4];
        }  
        
        // Creating final word by XOR-ing current tmp with word2
        words[i] = tmp^words[i-4];
    }
}

// Encrypting one 16 bytes data block
void aes_encrypt_128(const u_int8_t* key, u_int8_t message[16]){
    // Preparing 44 32 bit words for 11 round keys (AES-128 has 11 rounds) and variables for controlling rounds
    u_int32_t key_words[44] = {0};
    // Round counts rounds and round_key_offset tracks which word is first word for current round 
    u_int32_t round = 0, round_key_offset = 0;

    // Expanding key
    key_expansion_128(key, key_words);

    // Creating special state matrix which contains data 
    // In AES data are stored in COLUMNS which means that if first byte is on state[0][0] next one is on state[1][0]!
    u_int8_t state[4][4];

    // Copying message bytes into state matrix maintaining column order
    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            state[i][j] = message[j * 4 + i];
        }
    }

    //Round zero of encryption containing first operation AddRoundKey
    /*
    AddRoundKey is a simple operation that does XOR operation between round key and bytes of data
    Because bytes are stored in columns to do XOR operation we need to use different 32 bit key word for every column
    To do this we extract bytes from key word (just like in key_expansion) and we XOR-ing it with corresponding bytes in state matrix
    In AES first logical byte is MSB because it was written based on Big Endian system
    That's why we extract first logical byte (b0) as MSB
    */
    for(int j = 0; j < 4; ++j){
        // Using word corresponding to column
        u_int32_t key_word = key_words[round_key_offset + j];

        // Extracting word bytes
        u_int8_t key_b0 = (key_word >> 24) & 0xFF;
        u_int8_t key_b1 = (key_word >> 16) & 0xFF;
        u_int8_t key_b2 = (key_word >> 8) & 0xFF;
        u_int8_t key_b3 = (key_word) & 0xFF;

        // XOR-ing word bytes with corresponding bytes in column
        state[0][j] = state[0][j] ^ key_b0;
        state[1][j] = state[1][j] ^ key_b1;
        state[2][j] = state[2][j] ^ key_b2;
        state[3][j] = state[3][j] ^ key_b3;
    }

    // After round 0 we increment round counter and moving the key offset
    round_key_offset = ++round * 4;

    // Rounds - here we do 9 rounds of encrypting 
    /*
    Each round contains of four operations
    - SubBytes
    - ShiftRows
    - MixColumns
    - AddRoundKey
    */ 
    for(int k = 0; k < 9; ++k){
        // SubBytes works the same as SubBytes for key_expansion
        // Because state stores data as bytes we can skip extracting particular bytes
        for (int i = 0; i < 4; ++i){
            for(int j = 0;j < 4; ++j){
                state[i][j] = s_box[state[i][j]];
            }
        }

        // ShiftRows shifts bytes in row which is first step of spreading data across the matrix
        // ShiftRows makes sure that data are spread between columns 
        // ShiftRows and next operation (MixColumns) ensure that changing on bit in message changes cipher completely
        /*
        ShiftRows shifts bytes in rows (it's called shifting but works like bitwise rotation described in key_expansion)
        First row - no change
        Second row - shift 1 byte left
        Third row - shift 2 bytes left
        Fourth row - shift 3 bytes left (it's faster to shift 1 byte right but it's the same output)
        */
       // The fastest way to implement shift rows is to manually asign values
        u_int8_t temp0, temp1;

        temp0 = state[1][0];
        state[1][0] = state[1][1];
        state[1][1] = state[1][2];
        state[1][2] = state[1][3];
        state[1][3] = temp0;

        temp0 = state[2][0]; temp1 = state[2][1];
        state[2][0] = state[2][2];
        state[2][1] = state[2][3];
        state[2][2] = temp0;
        state[2][3] = temp1;

        temp0 = state[3][3];
        state[3][3] = state[3][2];
        state[3][2] = state[3][1];
        state[3][1] = state[3][0];
        state[3][0] = temp0;

        // MixCollumns is a heart of spreading and changing data in state matrix
        // It mixes data in one column 
        // With the help of shift rows it ensures that every column is dependant on every 32-bit word in initial message
        /* 
        It's a complicated mathematical matrix multiplication in Galois Field which can be represented as these equations
        s'0 = 2*s0 ^ 3*s1 ^ 1*s2 ^ 1*s3
        s'1 = 1*s0 ^ 2*s1 ^ 3*s2 ^ 1*s3
        s'2 = 1*s0 ^ 1*s1 ^ 2*s2 ^ 3*s3
        s'3 = 3*s0 ^ 1*s1 ^ 1*s2 ^ 2*s3

        s'0 - s'3 are new column bytes
        s0 - s3 are original column bytes

        We need to store original values of columns because every modified column byte is callculated based on original column bytes

        * is multiplication in Galois Field so
        1*a = a
        2*a = xtim(a)
        3*a = xtim(a)^a because 3 = 2 ^ 1
        */
        for(int j = 0; j < 4; ++j){
            // Storing original column bytes
            u_int8_t s0 = state[0][j];
            u_int8_t s1 = state[1][j];
            u_int8_t s2 = state[2][j];
            u_int8_t s3 = state[3][j];

            // Performing MixColumns
            state[0][j] = xtim(s0) ^ (xtim(s1) ^ s1) ^ s2 ^ s3;
            state[1][j] = s0 ^ xtim(s1) ^ (xtim(s2) ^ s2) ^ s3;
            state[2][j] = s0 ^ s1 ^ xtim(s2) ^ (xtim(s3) ^ s3);
            state[3][j] = (xtim(s0) ^ s0) ^ s1 ^ s2 ^ xtim(s3);
        }

        //AddRoundKey - the same as round zero
        for(int j = 0; j < 4; ++j){
            u_int32_t key_word = key_words[round_key_offset + j];

            u_int8_t key_b0 = (key_word >> 24) & 0xFF;
            u_int8_t key_b1 = (key_word >> 16) & 0xFF;
            u_int8_t key_b2 = (key_word >> 8) & 0xFF;
            u_int8_t key_b3 = (key_word) & 0xFF;
            
            state[0][j] = state[0][j] ^ key_b0;
            state[1][j] = state[1][j] ^ key_b1;
            state[2][j] = state[2][j] ^ key_b2;
            state[3][j] = state[3][j] ^ key_b3;
        }

        // Incrementing round counter and moving key offset
        round_key_offset = ++round * 4;
    }

    // Final round (11 round) performs every operation from previous round but not MixColumns

    // SubBytes
    for (int i = 0; i < 4; ++i){
        for(int j = 0;j < 4; ++j){
            state[i][j] = s_box[state[i][j]];
        }
    }

    // ShiftRows
    u_int8_t temp0, temp1;

    temp0 = state[1][0];
    state[1][0] = state[1][1];
    state[1][1] = state[1][2];
    state[1][2] = state[1][3];
    state[1][3] = temp0;

    temp0 = state[2][0]; temp1 = state[2][1];
    state[2][0] = state[2][2];
    state[2][1] = state[2][3];
    state[2][2] = temp0;
    state[2][3] = temp1;

    temp0 = state[3][3];
    state[3][3] = state[3][2];
    state[3][2] = state[3][1];
    state[3][1] = state[3][0];
    state[3][0] = temp0;

    //AddRoundKey
    for(int j = 0; j < 4; ++j){
        u_int32_t key_word = key_words[round_key_offset + j];

        u_int8_t key_b0 = (key_word >> 24) & 0xFF;
        u_int8_t key_b1 = (key_word >> 16) & 0xFF;
        u_int8_t key_b2 = (key_word >> 8) & 0xFF;
        u_int8_t key_b3 = (key_word) & 0xFF;
        
        state[0][j] = state[0][j] ^ key_b0;
        state[1][j] = state[1][j] ^ key_b1;
        state[2][j] = state[2][j] ^ key_b2;
        state[3][j] = state[3][j] ^ key_b3;
    }

    // After all rounds of encrypting we copy bytes from state matrix into message
    // Because message is a pointer now data in original message are changed as well, we don't need to return anything
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            message[j * 4 + i] = state[i][j];
        }
    }
}

// Decrypting one 16 bytes data block
void aes_decrypt_128(const u_int8_t* key, u_int8_t cipher[16]){
    // Preparing 44 32 bit words for round keys and controll variables
    u_int32_t key_words[44] = {0};
    // This time round is equal to 10 and round_key_offset equal to 40 because we are inverting encrypting process
    u_int32_t round = 10, round_key_offset = 40;

    // Expanding AES key
    key_expansion_128(key, key_words);

    // Preparing state matrix
    u_int8_t state[4][4];

    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            state[i][j] = cipher[j * 4 + i];
        }
    }

    // Round eleven
    // AddRoundKey
    /*
    Becuase XOR operation is it's own inversion then to invert AddRoundKey we need to add key again
    a^b = c => a = c^b - here b is our round key
    */
    for(int j = 0; j < 4; ++j){
        u_int32_t key_word = key_words[round_key_offset+j];

        u_int8_t b0 = (key_word >> 24) & 0xFF;
        u_int8_t b1 = (key_word >> 16) & 0xFF;
        u_int8_t b2 = (key_word >> 8) & 0xFF;
        u_int8_t b3 = (key_word) & 0xFF;

        state[0][j] = state[0][j] ^ b0;
        state[1][j] = state[1][j] ^ b1;
        state[2][j] = state[2][j] ^ b2;
        state[3][j] = state[3][j] ^ b3;
    }

    // InvShiftRows
    // To invert shift rows we need to shift rows right not left 
    u_int8_t temp0, temp1;

    temp0 = state[1][3];
    state[1][3] = state[1][2];
    state[1][2] = state[1][1];
    state[1][1] = state[1][0];
    state[1][0] = temp0;

    temp0 = state[2][0]; temp1 = state[2][1];
    state[2][0] = state[2][2];
    state[2][1] = state[2][3];
    state[2][2] = temp0;
    state[2][3] = temp1;

    temp0 = state[3][0];
    state[3][0] = state[3][1];
    state[3][1] = state[3][2];
    state[3][2] = state[3][3];
    state[3][3] = temp0;

    // InvSubBytes
    // To invert byte subtitution we need to do the same steps as for SubBytes but using inverse_s_box
    // Inverse S-box works the same as S-box but allows to invert SubBytes process
    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            state[i][j] = inverse_s_box[state[i][j]];
        }
    }

    round_key_offset = --round * 4;
    //Rounds
    for(int i = 0; i < 9; ++i){
        // AddRoundKey
        for(int j = 0; j < 4; ++j){
            u_int32_t key_word = key_words[round_key_offset+j];

            u_int8_t b0 = (key_word >> 24) & 0xFF;
            u_int8_t b1 = (key_word >> 16) & 0xFF;
            u_int8_t b2 = (key_word >> 8) & 0xFF;
            u_int8_t b3 = (key_word) & 0xFF;

            state[0][j] = state[0][j] ^ b0;
            state[1][j] = state[1][j] ^ b1;
            state[2][j] = state[2][j] ^ b2;
            state[3][j] = state[3][j] ^ b3;
        }   
        // InvMixCollumns
        /*
        To invert MixCollumns we need to multiply our collumn by inverted matrix
        It's again a complicated mathematical transformation that can be represented as theese equations:
        s'0 = 14*s0 ^ 11*s1 ^ 13*s2 ^ 9*s3
        s'1 = 9*s0 ^ 14*s1 ^ 11*s2 ^ 13*s3
        s'2 = 13*s0 ^ 9*s1 ^ 14*s2 ^ 11*s3
        s'3 = 11*s0 ^ 13*s1 ^ 9*s2 ^ 14*s3
        */
        for(int j = 0; j < 4; ++j){
            u_int8_t s0 = state[0][j];
            u_int8_t s1 = state[1][j];
            u_int8_t s2 = state[2][j];
            u_int8_t s3 = state[3][j];

            
            state[0][j] = mE(s0) ^ mB(s1) ^ mD(s2) ^ m9(s3);
            state[1][j] = m9(s0) ^ mE(s1) ^ mB(s2) ^ mD(s3);
            state[2][j] = mD(s0) ^ m9(s1) ^ mE(s2) ^ mB(s3);
            state[3][j] = mB(s0) ^ mD(s1) ^ m9(s2) ^ mE(s3);
        }

        // InvShiftRows
        temp0 = state[1][3];
        state[1][3] = state[1][2];
        state[1][2] = state[1][1];
        state[1][1] = state[1][0];
        state[1][0] = temp0;

        temp0 = state[2][0]; temp1 = state[2][1];
        state[2][0] = state[2][2];
        state[2][1] = state[2][3];
        state[2][2] = temp0;
        state[2][3] = temp1;

        temp0 = state[3][0];
        state[3][0] = state[3][1];
        state[3][1] = state[3][2];
        state[3][2] = state[3][3];
        state[3][3] = temp0;

        // InvSubBytes
        for(int i = 0; i < 4; ++i){
            for(int j = 0; j < 4; ++j){
                state[i][j] = inverse_s_box[state[i][j]];
            }
        }

        round_key_offset = --round * 4;
    }

    // AddRoundKey
    for(int j = 0; j < 4; ++j){
        u_int32_t key_word = key_words[round_key_offset+j];

        u_int8_t b0 = (key_word >> 24) & 0xFF;
        u_int8_t b1 = (key_word >> 16) & 0xFF;
        u_int8_t b2 = (key_word >> 8) & 0xFF;
        u_int8_t b3 = (key_word) & 0xFF;

        state[0][j] = state[0][j] ^ b0;
        state[1][j] = state[1][j] ^ b1;
        state[2][j] = state[2][j] ^ b2;
        state[3][j] = state[3][j] ^ b3;
    }

    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            cipher[j * 4 + i] = state[i][j];
        }
    }
}

// Function to print data as hex
void print_hex(unsigned char* data, int len) {
    for (int i = 0; i < len; ++i) {
        printf("%02x", data[i]);
    }
    putchar('\n');
}

void generate_iv(u_int8_t iv_vector[16]){
    FILE* fd = fopen("/dev/urandom", "rb");
    if(fd == NULL){
        perror("Error while opening file");
        exit(0);
    }

    if(fread(iv_vector, 1, 16, fd) != 16){
        perror("Error while reading data");
        exit(0);
    }

    fclose(fd);
}

// CBC encryption
u_int8_t* cbc_encryption_128(const u_int8_t* key, u_int8_t* mes, u_int32_t mes_bytes){
    // Creating variables for blocks used in cbc encrypting algorithm
    u_int8_t previous_block[16], data_block[16];
    // Generating IV vector and treating it as 0 block
    generate_iv(previous_block);

    // Creating variables for amount of data blocks and counter for output variable
    u_int32_t data_blocks = (mes_bytes/16) + 1;
    u_int32_t output_counter = 0;

    // Creating variable for complete cipher an allocating memory
    u_int8_t* output = malloc((data_blocks + 1) * 16);

    if(output == NULL){
        perror("Error while allocating memory");
        return NULL;
    }

    // Copying 16 bytes of data from previous block to output and incrementing output_counter
    memcpy(output, previous_block, 16);
    ++output_counter;

    // Encrypting blocks with following algorithm
    /*
    - Take current block
    - XOR current block with previous block (previous block in the 0 round is a IV vector)
    We do not encrypt IV vector - it has to be random but it's not private
    - Encrypt XORed block of data

    If it's the last block we have to handle padding (PKCF#7 standard)
    We fill last block of data with bytes containing number of filled bytes
    Eg. if we are filling 3 bytes then we add 3 bytes each with value 0x03
    
    Because CBC requires padding even if original message is multiple of 16 bytes we need to add padding
    In this case we add whole 16 bytes block with each byte set to 0x10
    */
    for(int i = 0; i < data_blocks; ++i){
        // If it's the last block - handle padding
        if(i == data_blocks - 1){
            // We calculate size of last message block
            u_int32_t last_block_size = mes_bytes % 16;
            // Variable that stores padding
            u_int8_t padding_value;

            // If message is multiple of 16 bytes - padding is equal to 16
            if(last_block_size == 0 && mes_bytes > 0){
                padding_value = 16;
            }
            // Else we calculate padding as 16 - size of last block
            else {
                padding_value = 16 - last_block_size;
            }
            // We copy what is left of original message (if last block size is zero it means there's nothing more to copy)
            memcpy(data_block, &mes[i * 16], last_block_size);
            
            // We add padding bytes
            for(int p = last_block_size; p < 16; ++p){
                data_block[p] = padding_value;
            }
        
        } 
        // If it's not last block of data - we regulary copy 16 bytes from message
        else {
            memcpy(data_block, &mes[i * 16], 16);
        }

        // Performing XOR with previous data block
        for(int j = 0; j < 16; ++j){
            data_block[j] = data_block[j] ^ previous_block[j];
        }

        // Encrypting the XORed value
        aes_encrypt_128(key, data_block);

        // Copying encrypted block data into output (starting from the right place) and into previous block
        memcpy(&output[output_counter * 16], data_block, 16);
        memcpy(previous_block, data_block, 16);

        // Incrementing output counter
        ++output_counter;
    }

    return output;
}

// CBC decryption
u_int8_t* cbc_decrypt_128(const u_int8_t* key, u_int8_t* cipher, u_int32_t cipher_bytes, u_int32_t* mes_bytes){
    // Just like in CBC encryption - creating control variables
    u_int8_t previous_block_encrypted[16], current_block[16];
    u_int32_t cipher_blocks = cipher_bytes/16, mes_blocks = cipher_blocks - 1;
    u_int8_t* output = malloc((cipher_blocks - 1) * 16);
    if(output == NULL){
        perror("Error while allocating memory");
        return NULL;
    }

    // Extract IV as a previous block - IV is not encrypted
    memcpy(previous_block_encrypted, cipher, 16);

    // Decrypting message
    for(int i = 0; i < mes_blocks; ++i) {
        // Copying next cipher block as current block
        memcpy(current_block, &cipher[(i + 1) * 16], 16);

        // Decrypting current block
        aes_decrypt_128(key, current_block);
        
        // XORing current block with encrypted previous block (on firts iteration it's unencrypted IV)
        for(int j = 0; j < 16; ++j) {
            current_block[j] = current_block[j] ^ previous_block_encrypted[j];
        }

        // Copying decrypted block into output
        memcpy(&output[i * 16], current_block, 16);

        // Copying next cipher block (current block but encrypted) into previous block encrypted
        memcpy(previous_block_encrypted, &cipher[(i + 1) * 16], 16);
    }

    // Handling padding
    u_int8_t padding = output[mes_blocks * 16 - 1];

    if(padding == 0 || padding > 16){
        fprintf(stderr, "Err: Incorrect padding value\n");
        free(output);
        return NULL;
    }

    u_int32_t out_encrypted_bytes = mes_blocks * 16 - padding;
    u_int8_t* tmp_output;
    tmp_output = realloc(output, out_encrypted_bytes);
    if(tmp_output == NULL && out_encrypted_bytes > 0){
        perror("Error while reallocating memory");
        free(output);
        return NULL;
    }
    else {
        output = tmp_output;
    }
    if(mes_bytes != NULL){
        *mes_bytes = out_encrypted_bytes;
    }

    return output;
}

int main(){
    unsigned char mes[16] = {
        'a', 'a', 'b', 'b', 'c', 'c', 'd', 'd',
        'e', 'e', 'f', 'f', 'g', 'g', 'h', 'h'
    };
    unsigned char key[16] = {
        'a', 'a', 'b', 'b', 'c', 'c', 'd', 'd',
        'e', 'e', 'f', 'f', 'g', 'g', 'h', 'h'
    };

    printf("Klucz (hex):      ");
    print_hex(key, 16);
    printf("Wiadomość (hex):  ");
    print_hex(mes, 16);

    unsigned char* output = cbc_encryption_128(key, mes, sizeof(mes));
    
    printf("Wektor IV (hex):  ");
    print_hex(output, 16);
    
    printf("Szyfrogram (hex): ");
    print_hex(&output[16], 32);

    aes_decrypt_128(key, mes);
    printf("Wiadomość (hex):  ");
    print_hex(mes, 16);

    unsigned char* output_decrypted = cbc_decrypt_128(key, output, 48, NULL);

    printf("Wiadomość (hex):  ");
    print_hex(output_decrypted, 16);

    free(output);
    free(output_decrypted);

    return 0;
}