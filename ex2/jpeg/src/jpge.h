// jpge.h - C++ class for JPEG compression.
// Public domain, Rich Geldreich <richgel99@gmail.com>
// Alex Evans: Added RGBA support, linear memory allocator.
#ifndef JPEG_ENCODER_H
#define JPEG_ENCODER_H

namespace jpge {
typedef unsigned char  uint8;
typedef signed short   int16;
typedef signed int     int32;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef unsigned int   uint;

struct rgb {
    uint8 r,g,b;
};

struct rgba {
    uint8 r,g,b,a;
};

struct ycbcr {
    float y,cb,cr;
};
typedef double dct_t;
typedef int16 dctq_t; // quantized

// JPEG chroma subsampling factors. Y_ONLY (grayscale images) and H2V2 (color images) are the most common.
enum subsampling_t { Y_ONLY = 0, H1V1 = 1, H2V1 = 2, H2V2 = 3 };

// JPEG compression parameters structure.
struct params {
    inline params() : m_quality(85), m_subsampling(H2V2), m_no_chroma_discrim_flag(false), m_two_pass_flag(false) { }

    inline bool check() const {
        if ((m_quality < 1) || (m_quality > 100)) return false;
        if ((uint)m_subsampling > (uint)H2V2) return false;
        return true;
    }

    // Quality: 1-100, higher is better. Typical values are around 50-95.
    int m_quality;

    // m_subsampling:
    // 0 = Y (grayscale) only
    // 1 = YCbCr, no subsampling (H1V1, YCbCr 1x1x1, 3 blocks per MCU)
    // 2 = YCbCr, H2V1 subsampling (YCbCr 2x1x1, 4 blocks per MCU)
    // 3 = YCbCr, H2V2 subsampling (YCbCr 4x1x1, 6 blocks per MCU-- very common)
    subsampling_t m_subsampling;

    // Disables CbCr discrimination - only intended for testing.
    // If true, the Y quantization table is also used for the CbCr channels.
    bool m_no_chroma_discrim_flag;

    bool m_two_pass_flag;
};

// Writes JPEG image to a file.
// num_channels must be 1 (Y) or 3 (RGB), image pitch must be width*num_channels.
bool compress_image_to_jpeg_file(const char *pFilename, int width, int height, int num_channels, const uint8 *pImage_data, const params &comp_params = params());

// Writes JPEG image to memory buffer.
// On entry, buf_size is the size of the output buffer pointed at by pBuf, which should be at least ~1024 bytes.
// If return value is true, buf_size will be set to the size of the compressed data.
bool compress_image_to_jpeg_file_in_memory(void *pBuf, int &buf_size, int width, int height, int num_channels, const uint8 *pImage_data, const params &comp_params = params());

// Output stream abstract class - used by the jpeg_encoder class to write to the output stream.
// put_buf() is generally called with len==JPGE_OUT_BUF_SIZE bytes, but for headers it'll be called with smaller amounts.
class output_stream {
public:
    virtual ~output_stream() { };
    virtual bool put_buf(const void *Pbuf, int len) = 0;
    template<class T> inline bool put_obj(const T &obj) {
        return put_buf(&obj, sizeof(T));
    }
};

bool compress_image_to_stream(output_stream &dst_stream, int width, int height, int num_channels, const uint8 *pImage_data, const params &comp_params);

class huffman_table {
public:
    uint m_codes[256];
    uint8 m_code_sizes[256];
    uint8 m_bits[17];
    uint8 m_val[256];
    uint32 m_count[256];

    void optimize(int table_len);
    void compute();
};

class component {
public:
    uint8 m_h_samp, m_v_samp;
    int m_last_dc_val;
};

struct huffman_dcac {
    int32 m_quantization_table[64];
    huffman_table dc,ac;
};

class image {
public:
    int m_x, m_y, m_bpp;
    int m_x_mcu, m_y_mcu;
    int m_mcus_per_row;
    int m_mcu_w, m_mcu_h;
    float *m_mcu_lines[3];
    dctq_t *m_dctqs[3]; // quantized dcts

    float get_px(int x, int y, int c);
    ycbcr get_px(int x, int y);
    void set_px(float px, int x, int y, int c);
    void set_px(ycbcr px, int x, int y);

    dctq_t *get_dctq(int x, int y, int c);
};

// Lower level jpeg_encoder class - useful if more control is needed than the above helper functions.
class jpeg_encoder {
public:
    jpeg_encoder();
    ~jpeg_encoder();

    // Initializes the compressor.
    // pStream: The stream object to use for writing compressed data.
    // params - Compression parameters structure, defined above.
    // width, height  - Image dimensions.
    // channels - May be 1, or 3. 1 indicates grayscale, 3 indicates RGB source data.
    // Returns false on out of memory or if a stream write fails.
    bool init(output_stream *pStream, int width, int height, int src_channels, const params &comp_params = params());

    const params &get_params() const {
        return m_params;
    }

    // Deinitializes the compressor, freeing any allocated memory. May be called at any time.
    void deinit();

    // Call this method with each source scanline.
    // width * src_channels bytes per scanline is expected (RGB or Y format).
    // Returns false on out of memory or if a stream write fails.
    bool read_image(const uint8 *data);
    bool process_scanline2(const uint8 *pScanline, int y);

    // You must call after all scanlines are processed to finish compression.
    bool process_end_of_image();
    void load_mcu_Y(const uint8 *pSrc, int y);
    void load_mcu_YCC(const uint8 *pSrc, int y);

private:
    jpeg_encoder(const jpeg_encoder &);
    jpeg_encoder &operator =(const jpeg_encoder &);

    output_stream *m_pStream;
    params m_params;
    uint8 m_num_components;
    component m_comp[3];
    int32 m_mcu_y_ofs;

    struct huffman_dcac m_huff[2];
    enum { JPGE_OUT_BUF_SIZE = 2048 };
    uint8 m_out_buf[JPGE_OUT_BUF_SIZE];
    uint8 *m_pOut_buf;
    uint m_out_buf_left;
    uint32 m_bit_buffer;
    uint m_bits_in;
    uint8 m_pass_num;
    bool m_all_stream_writes_succeeded;
    image m_image;

    void emit_byte(uint8 i);
    void emit_word(uint i);
    void emit_marker(int marker);
    void emit_jfif_app0();
    void emit_dqt();
    void emit_sof();
    void emit_dht(uint8 *bits, uint8 *val, int index, bool ac_flag);
    void emit_dhts();
    void emit_sos();
    void emit_markers();
    void compute_quant_table(int32 *dst, int16 *src);
    void adjust_quant_table(int32 *dst, int32 *src);
    void first_pass_init();
    bool second_pass_init();
    bool jpg_open(int p_x_res, int p_y_res, int src_channels);
    void load_block_8_8_grey(dct_t *, int x, int y);
    void load_block_8_8(dct_t *, int x, int y, int c);
    void load_block_16_8(dct_t *, int x, int y, int c);
    void load_block_16_8_8(dct_t *, int x, int y, int c);
    void quantize_pixels(dct_t *pSrc, int16 *pDst, const int32 *q);
    void flush_output_buffer();
    void put_bits(uint bits, uint len);
    void code_coefficients_pass_one(int16 *pSrc, huffman_dcac *huff, component *);
    void code_coefficients_pass_two(int16 *pSrc, huffman_dcac *huff, component *);
    void code_block(dct_t *src, dctq_t *coefficients, huffman_dcac *huff, int component_num);
    void process_mcu_row(int y);
    bool terminate_pass_one();
    bool terminate_pass_two();
    void clear();
    void init();
    dct_t blend_dual(int x, int y, int c);
    dct_t blend_quad(int x, int y, int c);
};

} // namespace jpge

#endif // JPEG_ENCODER
