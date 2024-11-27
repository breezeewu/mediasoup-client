#pragma once
//#include "lbframe.h"
//#include "lazylog.h"
#define ENABLE_LIBYUV_SCALE
#ifdef ENABLE_LIBYUV_SCALE
#include <libyuv.h>
using namespace libyuv;
#define HIGHT_LIGHT_Y   240
#define HIGHT_LIGHT_U   127
#define HIGHT_LIGHT_V   127

#ifndef lberror
#define lberror printf
#define lbtrace printf
#endif

#define lbmax(a, b) a > b ? a : b
#define lbmin(a, b) a > b ? b : a
#define lblimit(val, min, max) lbmin(lbmax(val, min), max)

#define RGB2Y(r, g, b) 0.299 * r + 0.587 * g + 0.114 * b + 16
#define RGB2U(r, g, b) -0.1684 * r - 0.3316 * g + 0.5 * b + 128;
#define RGB2V(r, g, b) 0.5 * r - 0.4187 * g - 0.0813 * b + 128;

// media micro
#define LAZY_NOPTS_VALUE  0x8000000000000000LL

struct pixel_argb
{
    uint8_t a;
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct pixel_yuv
{
    uint8_t y;
    uint8_t u;
    uint8_t v;
};
#ifndef DWORD_BE
#define DWORD_BE(val) (((val>>24)&0xFF) | (((val>>16)&0xFF)<<8) | (((val>>8)&0xFF)<<16) | ((val&0xFF)<<24))
#endif
static pixel_yuv rgb2yuv(uint32_t argb_color)
{
    pixel_argb argb;
    argb_color = DWORD_BE(argb_color);
    memcpy(&argb, &argb_color, sizeof(uint32_t));
    pixel_yuv yuv;
    yuv.y = RGB2Y(argb.r, argb.g, argb.b);
    yuv.u = RGB2U(argb.r, argb.g, argb.b);
    yuv.v = RGB2V(argb.r, argb.g, argb.b);

    return yuv;
}

class lazyscale
{
protected:
    FilterMode flt_mode;
    uint8_t *_i420_data;
    uint32_t _i420_data_len;
    uint8_t *_scale_data;
    uint32_t _scale_data_len;

public:
    lazyscale()
    {
        flt_mode = kFilterBilinear;
        _i420_data = NULL;
        _scale_data = NULL;
        _i420_data_len = 0;
        _scale_data_len = 0;
    }

    ~lazyscale()
    {
        if (_i420_data)
        {
            delete[] _i420_data;
            _i420_data = NULL;
        }
        _i420_data_len = 0;

        if (_scale_data)
        {
            delete[] _scale_data;
            _scale_data = NULL;
        }
        _scale_data_len = 0;
    }

    int scale(uint8_t *psrc, uint32_t srcfoucc, uint32_t srcw, uint32_t srch, uint8_t *pdst, int len, uint32_t dstfoucc, uint32_t dstw = 0, uint32_t dsth = 0)
    {
        int ret = -1;
        uint8_t *psrc_data = psrc;
        uint8_t *pdst_data = pdst;
        if (0 == dstw || 0 == dsth)
        {
            dstw = srcw;
            dsth = srch;
        }

        if (srcfoucc != dstfoucc)
        {
            uint32_t dst_data_len = srcw * srch * 3 / 2;
            // convert to i420
            if (FOURCC_I420 != srcfoucc)
            {
                if (srcw == dstw && srch == dsth && FOURCC_I420 == dstfoucc)
                {
                    pdst_data = pdst;
                }
                else
                {
                    if (_i420_data_len < dst_data_len)
                    {
                        if (_i420_data)
                        {
                            delete[] _i420_data;
                        }
                        _i420_data = new uint8_t[dst_data_len];
                    }
                    _i420_data_len = dst_data_len;
                    pdst_data = _i420_data;
                }

                ret = toi420(psrc_data, srcfoucc, srcw, srch, pdst_data, (int)dst_data_len);
                if (ret < 0)
                {
                    lberror("ret:%d = toi420(psrc_data:%p, srcfoucc:%0x, srcw:%u, srch:%u, pdst_data:%p, dst_data_len:%u)\n", ret, psrc_data, srcfoucc, srcw, srch, pdst_data, dst_data_len);
                    return ret;
                }
                assert(ret >= 0);
                psrc_data = pdst_data;
            }
        }
        else if (FOURCC_BGRA == srcfoucc || FOURCC_ARGB == srcfoucc || FOURCC_RGBA == srcfoucc || FOURCC_ABGR == srcfoucc)
        {
            if (srcw == dstw && srch == dsth)
            {
                int pic_size = srcw * 4 * srch;
                assert(len >= pic_size);
                memcpy(pdst, psrc, pic_size);
                return 0;
            }
            ret = ARGBScale(psrc_data, srcw * 4, srcw, srch, pdst, dstw * 4, dstw, dsth, flt_mode);
            return ret;
        }

        // need scale ?
        if (srcw != dstw || srch != dsth)
        {
            uint32_t dst_data_len = dstw * dsth * 3 / 2;
            if (FOURCC_I420 == dstfoucc)
            {
                pdst_data = pdst;
            }
            else
            {
                if (_scale_data_len < dst_data_len)
                {
                    if (_scale_data)
                    {
                        delete[] _scale_data;
                        _scale_data = NULL;
                    }
                    _scale_data = new uint8_t[dst_data_len];
                }
                _scale_data_len = dst_data_len;
                pdst_data = _scale_data;
            }
            ret = scalei420(psrc_data, srcw, srch, pdst_data, dstw, dsth);
            if (ret < 0)
            {
                lberror("ret:%d = scalei420(psrc_data:%p, srcw:%d, srch:%d, pdst_data:%p, dstw:%d, dsth:%d)\n", ret, psrc_data, srcw, srch, pdst_data, dstw, dsth);
                return ret;
            }
            //assert(ret > 0);
            psrc_data = pdst_data;
        }

        // convert to dstfoucc
        if (FOURCC_I420 != dstfoucc)
        {
            pdst_data = pdst;
            ret = fromi420(psrc_data, dstw, dsth, pdst_data, len, dstfoucc);
            if (ret < 0)
            {
                lberror("ret:%d = fromi420(psrc_data:%p, dstw:%d, dsth:%d, pdst_data:%p, len:%d, dstfoucc:%0x)\n", ret, psrc_data, dstw, dsth, pdst_data, len, dstfoucc);
                return ret;
            }
            assert(ret >= 0);
        }
        //lbtrace("%s() end, ret:%d\n", __FUNCTION__, ret);
        return ret;
    }

    int toi420(const uint8_t *psrc, uint32_t srcfoucc, uint32_t srcw, uint32_t srch, uint8_t *pdst, int len)
    {
        int ret = -1;
        uint8_t *y = pdst;
        uint8_t *u = pdst + srcw * srch;
        uint8_t *v = pdst + (srcw * srch) * 5 / 4;
        assert((uint32_t)len >= srcw * srch * 3 / 2);
        switch (srcfoucc)
        {
        case FOURCC_I420:
        {
            /**************** I420 format ****************
             * Y Y Y Y          stride: w
             * U U
             * V V
             ******************************************/
            memcpy(pdst, psrc, len);
            ret = 1;
            break;
        }
        case FOURCC_YV12:
        {
            /**************** YV12 format ****************
             * Y Y Y Y          stride: w
             * V V
             * U U
             ********************************************/
            int len = srcw * srch;
            memcpy(y, psrc, len);
            memcpy(v, psrc + len, len / 4);
            memcpy(u, psrc + len * 5 / 4, len / 4);
            break;
        }
        case FOURCC_NV12:
        {
            /**************** NV12 format ****************
             * Y Y Y Y          stride: w
             * U V U V
             ********************************************/
            ret = NV12ToI420(psrc, srcw, psrc + srcw * srch, srcw, y, srcw, u, srcw / 2, v, srcw / 2, srcw, srch);
            //lbtrace("ret:%d = NV12ToI420", ret);
            break;
        }
        case FOURCC_NV21:
        {
            /**************** NV21 format ****************
             * Y Y Y Y          stride: w
             * V U V U
             ********************************************/
            ret = NV21ToI420(psrc, srcw, psrc + srcw * srch, srcw, y, srcw, u, srcw / 2, v, srcw / 2, srcw, srch);
            //lbtrace("ret:%d = NV21ToI420", ret);
            break;
        }
        case FOURCC_YUY2:
        {
            /****************YUY2 format****************
             * Y U Y V          stride: w*2
             * Y U Y V
             ******************************************/
            ret = YUY2ToI420(psrc, srcw * 2, y, srcw, u, srcw / 2, v, srcw / 2, srcw, srch);
            //lbtrace("ret:%d = YUY2ToI420", ret);
            break;
        }
        case FOURCC_ARGB:
        {
            /**************** ARGB format ****************
             * A R G B          stride: w*4
             * A R G B
             ******************************************/
            ret = ARGBToI420(psrc, srcw * 4, y, srcw, u, srcw / 2, v, srcw / 2, srcw, srch);
            //lbtrace("ret:%d = ARGBToI420", ret);
            break;
        }
        case FOURCC_BGRA:
        {
            /**************** BGRA format ****************
             * B G R A          stride: w*4
             * B G R A
             ******************************************/
            ret = ARGBToI420(psrc, srcw * 4, y, srcw, u, srcw / 2, v, srcw / 2, srcw, srch);// ARGB little endian (bgra in memory) to I420.
            //lbtrace("ret:%d = BGRAToI420", ret);
            break;
        }
        case FOURCC_BGR3:
        {
            ret = RGB24ToI420(psrc, srcw * 3, y, srcw, u, srcw / 2, v, srcw / 2, srcw, srch);// RGB big endian (rgb in memory) to I420.
            break;
        }
        case FOURCC_RGB3:
        {
            /**************** RGB24 format ****************
             * R G B          stride: w*3
             * R G B
             ******************************************/
            ret = RAWToI420(psrc, srcw * 3, y, srcw, u, srcw / 2, v, srcw / 2, srcw, srch);// RGB big endian (rgb in memory) to I420.
            //lbtrace("ret:%d = RGB24ToI420", ret);
            break;
        }
        case FOURCC_RGBA:
        {
            /**************** RGBA format ****************
             * R G B A         stride: w*4
             * R G B A
             ******************************************/
            ret = ABGRToI420(psrc, srcw * 4, y, srcw, u, srcw / 2, v, srcw / 2, srcw, srch);// RGBA little endian (abgr in memory) to I420.
            //lbtrace("ret:%d = RGBAToI420", ret);
            break;
        }
        case FOURCC_ABGR:
        {
            /**************** ABGR format ****************
             * A B G R         stride: w*4
             * A B G R
             ******************************************/
            ret = RGBAToI420(psrc, srcw * 4, y, srcw, u, srcw / 2, v, srcw / 2, srcw, srch);
            break;
        }
        default:
        {
            lberror("Invalid forcc:%0x", srcfoucc);
            break;
        }

        }

        return ret;
    }

    int fromi420(uint8_t *psrc, uint32_t srcw, uint32_t srch, uint8_t *pdst, int len, uint32_t dstfoucc)
    {
        int ret = -1;
        uint8_t *y = psrc;
        uint8_t *u = psrc + srcw * srch;
        uint8_t *v = psrc + srcw * srch * 5 / 4;
        switch (dstfoucc)
        {
        case FOURCC_I420:
        {
            int cp_len = srcw * srch * 3 / 2;
            assert(len >= cp_len);
            memcpy(psrc, pdst, cp_len);
            return 1;
        }
        case FOURCC_YV12:
        {
            int cp_len = srcw * srch;
            assert(len >= cp_len * 3 / 2);
            memcpy(pdst, y, cp_len);
            memcpy(pdst + cp_len, v, cp_len / 4);
            memcpy(pdst + cp_len * 5 / 4, u, cp_len / 4);
            return 1;
        }
        case FOURCC_NV21:
        {
            assert((uint32_t)len >= srcw * srch * 2);
            ret = I420ToNV21(y, srcw, u, srcw / 2, v, srcw / 2, pdst, srcw, pdst + srcw * srch, srcw, srcw, srch);
            lbtrace("ret:%d = I420ToNV21", ret);
            break;
        }
        case FOURCC_NV12:
        {
            assert((uint32_t)len >= srcw * srch * 2);
            ret = I420ToNV12(y, srcw, u, srcw / 2, v, srcw / 2, pdst, srcw, pdst + srcw * srch, srcw, srcw, srch);
            lbtrace("ret:%d = I420ToNV12", ret);
            break;
        }
        case FOURCC_YUY2:
        {
            /****************YUY2 format****************
             * Y U Y V          stride: w*2
             * Y U Y V
             ******************************************/
            assert((uint32_t)len >= srcw * srch * 2);
            ret = I420ToYUY2(y, srcw, u, srcw / 2, v, srcw / 2, pdst, srcw * 2, srcw, srch);
            lbtrace("ret:%d = I420ToYUY2", ret);
            break;
        }
        case FOURCC_ARGB:
        {
            /**************** ARGB format ****************
             * A R G B          stride: w*4
             * A R G B
             ******************************************/
            assert((uint32_t)len >= srcw * srch * 4);
            ret = I420ToBGRA(y, srcw, u, srcw / 2, v, srcw / 2, pdst, srcw * 4, srcw, srch);
            lbtrace("ret:%d = I420ToARGB", ret);
            break;
        }
        case FOURCC_BGRA:
        {
            /**************** BGRA format ****************
             * B G R A          stride: w*4
             * B G R A
             ******************************************/
            assert((uint32_t)len >= srcw * srch * 4);
            ret = I420ToARGB(y, srcw, u, srcw / 2, v, srcw / 2, pdst, srcw * 4, srcw, srch);
            lbtrace("ret:%d = I420ToBGRA", ret);
            break;
        }
        case FOURCC_BGR3:
        {
            assert((uint32_t)len >= srcw * srch * 3);
            ret = I420ToRGB24(y, srcw, u, srcw / 2, v, srcw / 2, pdst, srcw * 3, srcw, srch);
            lbtrace("ret:%d = I420ToRGB24", ret);
            break;
        }
        case FOURCC_RGB3:
        {
            /**************** RGB24 format ****************
             * R G B          stride: w*3
             * R G B
             ******************************************/
            assert((uint32_t)len >= srcw * srch * 3);
            ret = I420ToRAW(y, srcw, u, srcw / 2, v, srcw / 2, pdst, srcw * 3, srcw, srch);
            lbtrace("ret:%d = I420ToRGB24", ret);
            break;
        }
        case FOURCC_RGBA:
        {
            /**************** RGBA format ****************
             * R G B A         stride: w*4
             * R G B A
             ******************************************/
            assert((uint32_t)len >= srcw * srch * 4);
            ret = I420ToABGR(y, srcw, u, srcw / 2, v, srcw / 2, pdst, srcw * 4, srcw, srch);
            lbtrace("ret:%d = I420ToRGBA", ret);
            break;
        }
        default:
        {
            lberror("Invalid forcc:%0x", dstfoucc);
            break;
        }

        }

        return ret;
    }

    int scalei420(uint8_t *psrc, uint32_t srcw, uint32_t srch, uint8_t *pdst, uint32_t dstw, uint32_t dsth)
    {
        uint8_t *src_y = psrc;
        uint8_t *src_u = psrc + srcw * srch;
        uint8_t *src_v = psrc + srcw * srch * 5 / 4;

        uint8_t *dst_y = pdst;
        uint8_t *dst_u = pdst + dstw * dsth;
        uint8_t *dst_v = pdst + dstw * dsth * 5 / 4;
        return I420Scale(src_y, srcw, src_u, srcw / 2, src_v, srcw / 2, srcw, srch, dst_y, dstw, dst_u, dstw / 2, dst_v, dstw / 2, dstw, dsth, flt_mode);
    }
#ifdef LAZY_FRAME_H_
    static int crop(lbframe *pframe, int crop_x, int crop_y, int crop_w, int crop_h)
    {
        crop_x = crop_x - crop_x % 2;
        crop_y = crop_y - crop_y % 2;
        crop_w = crop_w - crop_w % 2;
        crop_h = crop_h - crop_h % 2;
        int cw = pframe->_width - crop_x;
        int ch = pframe->_height - crop_y;
        cw = cw > crop_w ? crop_w : cw;
        ch = ch > crop_h ? crop_h : ch;
        cw = cw - cw % 2;
        ch = ch - ch % 2;
        switch (pframe->_format)
        {
        case FOURCC_I420:
        case FOURCC_YV12:
        {
            uint8_t *old_data = pframe->_data[0];
            uint8_t *pnew_data = new uint8_t[cw * ch * 3 / 2];
            uint8_t  *pdst = pnew_data;

            uint8_t *psrc = pframe->_data[0] + crop_y * pframe->_width + crop_x;
            // crop y
            for (int i = 0; i < ch; i++)
            {
                memcpy(pdst, psrc, cw);
                psrc += pframe->_linesize[0];
                pdst += cw;
            }

            psrc = pframe->_data[1] + crop_y * pframe->_width / 4 + crop_x / 2;
            pdst = pnew_data + cw * ch;
            // crop u/v plane
            for (int j = 0; j < ch / 2; j++)
            {
                memcpy(pdst, psrc, cw / 2);
                psrc += pframe->_linesize[1];
                pdst += cw / 2;
            }

            psrc = pframe->_data[2] + crop_y * pframe->_width / 4 + crop_x / 2;
            pdst = pnew_data + cw * ch * 5 / 4;
            // crop v/u plane
            for (int k = 0; k < ch / 2; k++)
            {
                memcpy(pdst, psrc, cw / 2);
                psrc += pframe->_linesize[2];
                pdst += cw / 2;
            }

            pframe->_data[0] = pnew_data;
            if (FOURCC_I420 == pframe->_format)
            {
                pframe->_data[1] = pnew_data + cw * ch;
                pframe->_data[2] = pnew_data + cw * ch * 5 / 4;
            }
            else
            {
                pframe->_data[2] = pnew_data + cw * ch;
                pframe->_data[1] = pnew_data + cw * ch * 5 / 4;
            }

            pframe->_linesize[0] = cw;
            pframe->_linesize[1] = cw / 2;
            pframe->_linesize[2] = cw / 2;
            if (!pframe->_bref)
            {
                delete[] old_data;
            }
            pframe->_width = cw;
            pframe->_height = ch;
            pframe->_bref = false;
            break;
        }
        case FOURCC_NV12:
        case FOURCC_NV21:
        {
            uint8_t *pnew_data = new uint8_t[cw * ch * 3 / 2];
            uint8_t *old_data = pframe->_data[0];
            uint8_t *psrc = old_data + crop_y * pframe->_width + crop_x;
            uint8_t *pdst = pnew_data;
            for (int i = 0; i < ch; i++)
            {
                memcpy(pdst, psrc, cw);
                psrc += pframe->_linesize[0];
                pdst += cw;
            }

            for (int j = 0; j < ch / 2; j++)
            {
                memcpy(pdst, psrc, cw);
                psrc += pframe->_linesize[1];
                pdst += cw / 2;
            }

            if (!pframe->_bref)
            {
                delete[] old_data;
            }
            pframe->_bref = false;
            pframe->_data[0] = pnew_data;
            pframe->_linesize[0] = cw;
            pframe->_data[1] = pnew_data + cw * ch;
            pframe->_linesize[1] = cw;
            pframe->_width = cw;
            pframe->_height = ch;
            break;
        }
        case FOURCC_BGR3:
        case FOURCC_RGB3:
        case FOURCC_BGRA:
        case FOURCC_RGBA:
        case FOURCC_ARGB:
        case FOURCC_ABGR:
        {
            int size = lbframe::format_size(pframe->_format, cw, ch);
            int bytes_per_pixel = 4;
            if (FOURCC_BGR3 == pframe->_format || FOURCC_RGB3 == pframe->_format)
            {
                bytes_per_pixel = 3;
            }
            uint8_t *data = new uint8_t[size];
            uint8_t *psrc = pframe->_data[0] + (crop_y * pframe->_width + crop_x) * bytes_per_pixel;
            uint8_t *pdst = data;
            for (int i = 0; i < ch; i++)
            {
                memcpy(pdst, psrc, cw * bytes_per_pixel);
                psrc += pframe->_linesize[0];
                pdst += cw * bytes_per_pixel;
            }

            if (!pframe->_bref)
            {
                delete[] pframe->_data[0];
            }

            pframe->_data[0] = data;
            pframe->_linesize[0] = cw * bytes_per_pixel;
            pframe->_width = cw;
            pframe->_height = ch;
            pframe->_bref = false;
            break;
        }
        default:
        {
            assert(0);
            break;
        }
        }

        return 0;
    }

    int scale(lbframe *pframe, int width, int height, uint32_t format = -1)
    {
        if ((uint32_t)-1 == format)
        {
            format = pframe->_format;
        }
        if (pframe->_width == (uint32_t)width && pframe->_height == (uint32_t)height && format == pframe->_format)
        {
            return height;
        }
        uint64_t pts = pframe->_timestamp;
        uint32_t size = lbframe::format_size(format, width, height);
        uint8_t *new_data = new uint8_t[size];
        int ret = scale(pframe->_data[0], pframe->_format, pframe->_width, pframe->_height, new_data, size, format, width, height);

        pframe->reset();
        pframe->fill_video(format, width, height, new_data, pts, false);

        return ret;
    }
    static bool is_rgba_format(uint32_t format)
    {
        switch (format)
        {
        case FOURCC_BGRA:
        case FOURCC_RGBA:
        case FOURCC_ARGB:
        case FOURCC_ABGR:
        {
            return true;
        }
        default:
        {
            return false;
        }
        }
    }
    static int overlay_argb_to_i420(lbframe *psrcframe, lbframe *pdstframe, int x, int y)
    {
        if (NULL == psrcframe || NULL == pdstframe)
        {
            lberror("Invalid param, psrcframe:%p, pdstframe:%p\n", psrcframe, pdstframe);
            return -1;
        }

        if (!is_rgba_format(psrcframe->_format) || pdstframe->_format != FOURCC_I420)
        {
            lberror("Invalid frame format, psrcframe->_format:%0x  is not rgba format or pdstframe->_format:%0x != FOURCC_I420\n", psrcframe->_format, pdstframe->_format);
            return -1;
        }
        int pixel_size = 4;
        // argb: 0:b, 1:g, 2:r, 3:a
        int a_idx = 3;
        int r_idx = 2;
        int g_idx = 1;
        int b_idx = 0;
        switch (psrcframe->_format)
        {
        case FOURCC_BGRA:
        {
            b_idx = 0;
            g_idx = 1;
            r_idx = 2;
            a_idx = 3;
            break;
        }
        case FOURCC_ARGB:
        {
            a_idx = 0;
            r_idx = 1;
            g_idx = 2;
            b_idx = 3;
            break;
        }
        case FOURCC_RGBA:
        {
            r_idx = 0;
            g_idx = 1;
            b_idx = 2;
            a_idx = 3;
            break;
        }
        case FOURCC_ABGR:
        {
            a_idx = 0;
            b_idx = 1;
            g_idx = 2;
            r_idx = 3;
            break;
        }
        default:
        {
            lberror("Invalid rgb format for verlay:%0x\n", psrcframe->_format);
            break;
        }
        }
        uint8_t *src_pixel = NULL, *pdy = NULL, *pdu = NULL, *pdv = NULL;
        //int src_pitch = (int)psrcframe->_width * pixel_size;
        int sw_max = x + (int)psrcframe->_width;
        int sh_max = y + (int)psrcframe->_height;
        for (int j = y, q = 0; j < sh_max; j++, q += 4)
        {
            src_pixel = psrcframe->_data[0] + q * psrcframe->_width;
            pdy = pdstframe->_data[0] + j * pdstframe->_width + x;
            for (int i = x, p = 0; i < sw_max; i++, p++, src_pixel += 4, pdy++)
            {
                if (src_pixel[a_idx])
                {
                    //*pdy = RGB2Y(src_pixel[3], src_pixel[2], src_pixel[1]);
                    int y = RGB2Y(src_pixel[r_idx], src_pixel[g_idx], src_pixel[b_idx]);
                    *pdy = lbmin(y, 240);
                }
            }
        }

        for (int j = y / 2, q = 0; j < sh_max / 2; j++, q += 2)
        {
            src_pixel = psrcframe->_data[0] + q * psrcframe->_width * pixel_size;
            pdu = pdstframe->_data[1] + j * pdstframe->_width / 2 + x / 2;
            pdv = pdstframe->_data[2] + j * pdstframe->_width / 2 + x / 2;
            for (int i = x / 2, p = 0; i < sw_max / 2; i++, p += 2, pdu++, pdv++, src_pixel += 8)
            {
                if (src_pixel[a_idx])
                {
                    int u = RGB2U(src_pixel[r_idx], src_pixel[g_idx], src_pixel[b_idx]);
                    u = lblimit(u, -127, 127);
                    *pdu = u;
                    int v = RGB2V(src_pixel[r_idx], src_pixel[g_idx], src_pixel[b_idx]);
                    v = lblimit(v, -127, 127);
                    *pdv = v;
                }
            }
        }
        lbtrace("y size:%d, u size:%d, v size:%d\n", pdy - pdstframe->_data[0], pdu - pdstframe->_data[1], pdv - pdstframe->_data[2]);
        /*FILE* pfile = fopen("overlay.i420", "wb");
        if (pfile)
        {
            fwrite(pdstframe->_data[0], 1, pdstframe->frame_size(), pfile);
            fclose(pfile);
        }*/
        return 0;
    }
    static int overlay(lbframe *psrcframe, lbframe *pdstframe, int x, int y)
    {
        assert(x + psrcframe->_width <= pdstframe->_width);
        assert(y + psrcframe->_height <= pdstframe->_height);
        if (FOURCC_I420 == pdstframe->_format && is_rgba_format(psrcframe->_format))
        {
            return overlay_argb_to_i420(psrcframe, pdstframe, x, y);
        }
        else if (pdstframe->_format != psrcframe->_format)
        {
            return -1;
        }
        switch (pdstframe->_format)
        {
        case FOURCC_I420:
        case FOURCC_YV12:
        {
            uint8_t *psrc = psrcframe->_data[0];
            uint8_t *pdst = pdstframe->_data[0] + y * pdstframe->_width + x;
            for (uint32_t i = 0; i < psrcframe->_height; i++)
            {
                memcpy(pdst, psrc, psrcframe->_width);
                psrc += psrcframe->_linesize[0];
                pdst += pdstframe->_linesize[0];
            }
            psrc = psrcframe->_data[1];
            pdst = pdstframe->_data[1] + y * pdstframe->_width / 4 + x / 2;
            for (uint32_t j = 0; j < psrcframe->_height / 2; j++)
            {
                memcpy(pdst, psrc, psrcframe->_width / 2);
                psrc += psrcframe->_linesize[1];
                pdst += pdstframe->_linesize[1];
            }
            psrc = psrcframe->_data[2];
            pdst = pdstframe->_data[2] + y * pdstframe->_width / 4 + x / 2;
            for (uint32_t k = 0; k < psrcframe->_height / 2; k++)
            {
                memcpy(pdst, psrc, psrcframe->_width / 2);
                psrc += psrcframe->_linesize[2];
                pdst += pdstframe->_linesize[2];
            }
            break;
        }
        case FOURCC_NV12:
        case FOURCC_NV21:
        {
            uint8_t *psrc = psrcframe->_data[0];
            uint8_t *pdst = pdstframe->_data[0] + y * pdstframe->_width + x;
            for (uint32_t i = 0; i < psrcframe->_height; i++)
            {
                memcpy(pdst, psrc, psrcframe->_width);
                psrc += psrcframe->_linesize[0];
                pdst += pdstframe->_linesize[0];
            }

            psrc = psrcframe->_data[1];
            pdst = pdstframe->_data[1] + y / 2 * pdstframe->_width + x;
            for (uint32_t j = 0; j < psrcframe->_height / 2; j++)
            {
                memcpy(pdst, psrc, psrcframe->_width);
                psrc += psrcframe->_linesize[0];
                pdst += pdstframe->_linesize[1];
            }
            break;
        }
        case FOURCC_RGB3:
        case FOURCC_BGR3:
        case FOURCC_BGRA:
        case FOURCC_RGBA:
            //case FOURCC_ARGB:
        {
            int bytes_per_pixel = 4;
            if (FOURCC_RGB3 == pdstframe->_format || FOURCC_BGR3 == pdstframe->_format)
            {
                bytes_per_pixel = 3;
            }
            uint8_t *psrc = psrcframe->_data[0];
            uint8_t *pdst = psrcframe->_data[0] + (y * pdstframe->_width + x) * bytes_per_pixel;
            for (uint32_t i = 0; i < psrcframe->_height; i++)
            {
                memcpy(pdst, psrc, psrcframe->_width * bytes_per_pixel);
                psrc += psrcframe->_linesize[0];
                pdst += pdstframe->_linesize[0];
            }
            break;
        }
        case FOURCC_ARGB:
        {
            int bytes_per_pixel = 4;
            uint8_t *psrc = psrcframe->_data[0];
            uint8_t *pdst = psrcframe->_data[0] + (y * pdstframe->_width + x) * bytes_per_pixel;
            for (uint32_t i = 0; i < psrcframe->_height; i++)
            {
                for (uint32_t j = 0; j < psrcframe->_width; j+=4)
                {
                    if (psrc[j])
                    {
                        memcpy(psrc + j, pdst + j, bytes_per_pixel);
                    }
                }
                //memcpy(pdst, psrc, psrcframe->_width * bytes_per_pixel);
                psrc += psrcframe->_linesize[0];
                pdst += pdstframe->_linesize[0];
            }
            break;
        }
        default:
        {
            assert(0);
            break;
        }
        }

        return 0;
    }

    static void high_light(lbframe *pframe, int x, int y, int width, int height, uint32_t argb_color, int pixel_size)
    {
        if (NULL == pframe)
        {
            return ;
        }
        assert(width > 2 * pixel_size);
        pixel_size = pixel_size - pixel_size % 2;
        //int x_max = x + width;
        //int y_max = y + height;
        pixel_yuv yuv = rgb2yuv(argb_color);
        switch (pframe->_format)
        {
        case FOURCC_I420:
        case FOURCC_YV12:
        {
            for (int j = 0; j < height; j++)
            {
                uint8_t *py = pframe->_data[0] + (j + y) * pframe->_width + x;
                uint8_t *pu = pframe->_data[1] + (j + y) * pframe->_width / 4 + x / 2;
                uint8_t *pv = pframe->_data[2] + (j + y) * pframe->_width / 4 + x / 2;
                if (FOURCC_YV12 == pframe->_format)
                {
                    uint8_t *ptmp = pu;
                    pu = pv;
                    pv = ptmp;
                }

                for (int i = 0; i < width; i++, py++)
                {
                    if (i < pixel_size || j < pixel_size || i >= width - pixel_size || j >= height - pixel_size)
                    {
                        *py = yuv.y;
                        if (i % 2 == 0)
                        {

                            if (j % 2 == 0)
                            {
                                *pu = yuv.u;
                                *pv = yuv.v;
                                pu++;
                                pv++;
                            }
                        }
                    }
                    else if (i == pixel_size)
                    {
                        py += width - 2 * pixel_size - 1;
                        i += width - 2 * pixel_size - 1;
                        if (j % 2 == 0)
                        {
                            pu += (width - 2 * pixel_size) / 2;
                            pv += (width - 2 * pixel_size) / 2;
                            long u_off = pu - pframe->_data[1];
                            long v_off = pv - pframe->_data[2];
                            assert(u_off <= pframe->_width * pframe->_height / 2);
                            assert(v_off <= pframe->_width * pframe->_height / 2);
                        }

                    }
                }
            }
        }
        }
    }
#endif
};
#endif