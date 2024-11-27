#ifndef LAZY_FRAME_H_
#define LAZY_FRAME_H_
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "libyuv/video_common.h"
using namespace libyuv;
//#include "lazytype.h"
/*#define LAZY_FOURCC(a, b, c, d)  (((uint32_t)(a)) | ((uint32_t)(b) << 8) |  ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))
enum lazy_pixel_format
{
    // 10 Primary YUV formats: 5 planar, 2 biplanar, 2 packed.
    lazy_pixel_fmt_none = -1,
    lazy_pixel_fmt_i420 = LAZY_FOURCC('I', '4', '2', '0'), // yyyy uu vv 3p
    lazy_pixel_fmt_yv12 = LAZY_FOURCC('Y', 'V', '1', '2'), // yyyy vv uu 3p
    lazy_pixel_fmt_yuy2 = LAZY_FOURCC('Y', 'U', 'Y', '2'), // yyyy vv uu 3p
    lazy_pixel_fmt_nv12 = LAZY_FOURCC('N', 'V', '1', '2'), // yyyy uv uv 2p
    lazy_pixel_fmt_nv21 = LAZY_FOURCC('N', 'V', '2', '1'), // yyyy vu vu 2p
    lazy_pixel_fmt_bgr24 = LAZY_FOURCC('2', '4', 'B', 'G'), // b g r b g r 1p, 24位bmp像素格式
    lazy_pixel_fmt_rgb24 = LAZY_FOURCC('r', 'a', 'w', ' '), // r g b r g b 1p, 24位bmp像素格式
    lazy_pixel_fmt_rgb3 = LAZY_FOURCC('R', 'G', 'B', '3'), // r g b r g b 1p, Alias for RAW.
    lazy_pixel_fmt_bgr3 = LAZY_FOURCC('B', 'G', 'R', '3'), // b g r b g r 1p, Alias for 24BG.
    lazy_pixel_fmt_bgra = LAZY_FOURCC('B', 'G', 'R', 'A'), // b g r a b g r 1p, 32位bmp像素格式
    FOURCC_RGBA = LAZY_FOURCC('R', 'G', 'B', 'A'), // r g b a r g b a 1p, 32位bmp像素格式
    lazy_pixel_fmt_argb = LAZY_FOURCC('A', 'R', 'G', 'B'), // a r g b a r g b 1p, 32位bmp像素格式
    lazy_pixel_fmt_abgr = LAZY_FOURCC('A', 'B', 'G', 'R')  // a b g r a b g r 1p, 32位bmp像素格式
};*/

enum lazy_sample_format
{
    lazy_sample_fmt_none = -1,
    lazy_sample_fmt_u8,   // unsinged signed 8bits
    lazy_sample_fmt_s16,  // signed 16bits
    lazy_sample_fmt_s32,  // signed 32bits
    lazy_sample_fmt_flt   // float 32bits
};

enum lazy_media_type
{
    lazy_media_type_none = -1,
    lazy_media_type_video,
    lazy_media_type_audio,
    lazy_media_type_subtitle
};
class lbframe
{
public:
    int _emedia_type;  // video:0, audio:1
    uint32_t _format;           // audio:LBSAMPLE_FORMAT, video:LBPIXEL_FORMAT
    uint64_t _timestamp;

    uint32_t _width;
    uint32_t _height;

    uint32_t _channel;
    uint32_t _samplerate;

    uint32_t _nb_samples;

    bool     _bref;
    uint8_t *_data[4];          // allocate by one block
    uint32_t _linesize[4];

    lbframe(lbframe *pframe)
    {
        _emedia_type = pframe->_emedia_type;
        _format = pframe->_format;
        _timestamp = pframe->_timestamp;
        _width = pframe->_width;
        _height = pframe->_height;
        _channel = pframe->_channel;
        _samplerate = pframe->_samplerate;
        _nb_samples = pframe->_nb_samples;
        _bref = false;
        memset(_data, 0, sizeof(_data));
        memset(_linesize, 0, sizeof(_linesize));

        int size = format_size(_format, _width, _height);
        uint8_t *pdata = new uint8_t[size];
        memcpy(pdata, pframe->_data[0], size);
        fill_video(_format, pframe->_width, pframe->_height, pdata, pframe->_timestamp, false);
    }

    lbframe()
    {
        _emedia_type = lazy_media_type_video;
        _format = 0;
        _timestamp = 0;

        // video
        _width  = 0;
        _height = 0;

        // audio
        _channel = 0;
        _samplerate = 0;
        _nb_samples = 0;

        _bref       = false;

        memset(_data, 0, sizeof(_data));
        memset(_linesize, 0, sizeof(_linesize));
    }

    virtual ~lbframe()
    {
        reset();
    }

    void reset()
    {
        _emedia_type = lazy_media_type_none;
        _format = 0;
        _timestamp = 0;

        // video
        _width  = 0;
        _height = 0;

        // audio
        _channel = 0;
        _samplerate = 0;
        _nb_samples = 0;

        if(!_bref)
        {
            delete[] _data[0];
        }
        _bref       = false;
        memset(_data, 0, sizeof(_data));
        memset(_linesize, 0, sizeof(_linesize));
    }

    int init_video(int format, int width, int height, uint64_t pts, uint8_t *data = NULL, bool bref = false)
    {
        _emedia_type = lazy_media_type_video;

        if (!data || !bref)
        {
            int img_size = format_size(format, width, height);
            uint8_t *ptmp = new uint8_t[img_size];
            if (format == FOURCC_I420)
            {
                memset(ptmp, 0x80, img_size);
            }
            if (data && !bref)
            {
                memcpy(ptmp, data, img_size);
            }
            data = ptmp;
            bref = false;
        }

        return fill_video(format, width, height, data, pts, bref);
    }

    int init_audio(int format, int channel, int samplerate, uint64_t pts, int nbsample, uint8_t *data = NULL, bool bref = false)
    {
        _emedia_type = lazy_media_type_audio;
        _format = format;
        _channel = channel;
        _samplerate = samplerate;
        _nb_samples = nbsample;
        _timestamp = pts;

        int frame_size = bytes_per_sample(format) * channel * nbsample;
        if (!data)
        {
            if (_data[0])
            {
                delete[] _data[0];
                _data[0] = 0;
            }

            data = new uint8_t[frame_size];
            bref = false;
        }
        _bref = bref;
        _data[0] = data;
        _linesize[0] = frame_size;
        return 0;
    }

    int fill_video(uint32_t format, int width, int height, uint8_t *data, uint64_t pts, bool bref = true)
    {
        reset();
        _emedia_type = lazy_media_type_video;
        _width = width;
        _height = height;
        _format = format;
        _timestamp = pts;
        _bref = bref;
        switch(format)
        {
        case FOURCC_I420:
        case FOURCC_YV12:
        {
            _data[0] = data;
            _linesize[0] = width;

            uint8_t *u = data + width * height;
            uint8_t *v = data + width * height * 5 / 4;
            if(FOURCC_I420 == format)
            {
                _data[1] = u;
                _data[2] = v;
            }
            else
            {
                _data[1] = v;
                _data[2] = u;
            }
            _linesize[1] = width / 2;
            _linesize[2] = width / 2;
            break;
        }
        case FOURCC_NV12:
        case FOURCC_NV21:
        {
            _data[0] = data;
            _linesize[0] = width;
            _data[1] = data + width * height;
            _linesize[1] = width / 2;
            break;
        }
        case FOURCC_RGB3:
        case FOURCC_BGR3:
        case FOURCC_BGRA:
        case FOURCC_RGBA:
        case FOURCC_ARGB:
        case FOURCC_ABGR:
        {
            _data[0] = data;
            if(FOURCC_RGB3 == format || FOURCC_BGR3 == format)
            {
                _linesize[0] = width * 3;
            }
            else
            {
                _linesize[0] = width * 4;
            }
            break;
        }
        default:
        {
            //lberror("Invalid pixel format:%0x\n", format);
            assert(0);
            break;
        }
        }

        return 0;
    }

    int fill_plane(int width, int height, int format, uint8_t **ppdata, int *linesize, uint64_t pts, bool bref = true)
    {
        reset();
        _bref = bref;
        int plane_cnt = plane_count(format);
        if (bref)
        {
            _emedia_type = lazy_media_type_video;
            _format = format;
            _width = width;
            _height = height;
            _timestamp = pts;
            for (int i = 0; i < plane_cnt; i++)
            {
                _data[i] = ppdata[i];
                _linesize[i] = linesize[i];
            }
        }
        else
        {
            uint8_t *img_data = new uint8_t[frame_size()];
            fill_video(format, width, height, img_data, pts, false);
            for (int i = 0; i < plane_cnt; i++)
            {
                memcpy(_data[i], ppdata[i], plane_size(width, height, format, i));
                _linesize[i] = linesize[i];
            }
        }

        return 0;
    }

    int clone_video(uint32_t format, int width , int height, uint8_t *data, uint64_t pts)
    {
        int size = format_size(format, width, height);
        uint8_t *pdata = new uint8_t[size];
        if(data)
        {
            memcpy(pdata, data, size);
        }
        else
        {
            memset(pdata, 0, size);
        }
        return fill_video(format, width, height, pdata, pts, false);
    }

    static int format_size(uint32_t format, int width, int height)
    {
        switch(format)
        {
        case FOURCC_I420:
        case FOURCC_YV12:
        {
            return width * height * 3 / 2;
        }
        case FOURCC_NV12:
        case FOURCC_NV21:
        {
            return width * height * 3 / 2;
        }
        case FOURCC_RGB3:
        case FOURCC_BGR3:
        {
            return width * height * 3;
        }
        case FOURCC_BGRA:
        case FOURCC_RGBA:
        case FOURCC_ARGB:
        case FOURCC_ABGR:
        {
            return width * height * 4;
        }
        default:
        {
            assert(0);
            return 0;
        }
        }
    }

    int frame_size()
    {
        if (lazy_media_type_video == _emedia_type)
        {
            return format_size(_format, _width, _height);
        }
        else
        {
            return _nb_samples * _channel * bytes_per_sample(_format);
        }
    }

    static int plane_count(int format)
    {
        switch (format)
        {
        case FOURCC_I420:
        case FOURCC_YV12:
        {
            return 3;
        }
        case FOURCC_NV12:
        case FOURCC_NV21:
        {
            return 2;
        }
        case FOURCC_RGB3:
        case FOURCC_BGR3:
        case FOURCC_BGRA:
        case FOURCC_RGBA:
        case FOURCC_ARGB:
        case FOURCC_ABGR:
        {
            return 1;
        }
        default:
        {
            assert(0);
            return -1;
        }
        }
    }
    static int plane_size(int width, int height, int format, int index)
    {
        switch (format)
        {
        case FOURCC_I420:
        case FOURCC_YV12:
        {
            if (0 == index)
            {
                return width * height;
            }
            else if(1 == index || 2 == index)
            {
                return width * height / 4;
            }
            return 0;
        }
        case FOURCC_NV12:
        case FOURCC_NV21:
        {
            if (0 == index)
            {
                return width * height;
            }
            else if (1 == index || 2 == index)
            {
                return width * height / 2;
            }
            return 0;
        }
        case FOURCC_RGB3:
        case FOURCC_BGR3:
        {
            if (0 == index)
            {
                return width * height * 3;
            }
            return 0;
        }
        case FOURCC_BGRA:
        case FOURCC_RGBA:
        case FOURCC_ARGB:
        case FOURCC_ABGR:
        {
            if (0 == index)
            {
                return width * height * 4;
            }
            return 0;
        }
        default:
        {
            assert(0);
            return -1;
        }
        }
    }

    int clone(lbframe *pframe)
    {
        if(lazy_media_type_video == pframe->_emedia_type)
        {
            if(_format != pframe->_format || _width != pframe->_width || _height != pframe->_height || !_data[0])
            {
                reset();
                clone_video(pframe->_format, pframe->_width, pframe->_height, NULL, pframe->_timestamp);
            }

            return copy(pframe);
        }
        else
        {
            if (_channel != pframe->_channel || _samplerate != pframe->_samplerate || _nb_samples != pframe->_nb_samples)
            {
                reset();
                _channel = pframe->_channel;
                _samplerate = pframe->_samplerate;
                _format = pframe->_format;
                _nb_samples = pframe->_nb_samples;
                assert((int)pframe->_linesize[0] == frame_size());
                _linesize[0] = pframe->_linesize[0];
                _data[0] = new uint8_t[_linesize[0]];
            }

            memcpy(_data[0], pframe->_data[0], pframe->_linesize[0]);
            return 0;
        }
    }

    int copy(lbframe *pframe)
    {
        if(lazy_media_type_video == pframe->_emedia_type)
        {
            if(_format != pframe->_format || _width != pframe->_width || _height != pframe->_height || !_data[0])
            {
                //lberror("Invalid video property found, _format:%d != pframe->_format:%d || _width:%d != pframe->_width:%d || _height:%d != pframe->_height:%d || !_data[0]:%p\n", _format, pframe->_format, _width, pframe->_width, _height, pframe->_height, _data[0]);
                assert(0);
                return -1;
            }
            switch(_format)
            {
            case FOURCC_I420:
            case FOURCC_YV12:
            {
                uint8_t *psrc = pframe->_data[0];
                uint8_t *pdst = _data[0];
                    for(uint32_t i = 0; i < _height; i++)
                {
                    memcpy(pdst, psrc, _width);
                    psrc += pframe->_linesize[0];
                    pdst += _linesize[0];
                }

                psrc = pframe->_data[1];
                pdst = _data[1];
                    for(uint32_t j = 0; j < _height/2; j++)
                {
                    memcpy(pdst, psrc, _width / 2);
                    psrc += pframe->_linesize[1];
                    pdst += _linesize[1];
                }

                psrc = pframe->_data[2];
                pdst = _data[2];
                    for(uint32_t k = 0; k < _height/2; k++)
                {
                    memcpy(pdst, psrc, _width / 2);
                    psrc += pframe->_linesize[2];
                    pdst += _linesize[2];
                }
                return 0;
            }
            case FOURCC_NV12:
            case FOURCC_NV21:
            {
                uint8_t *psrc = pframe->_data[0];
                uint8_t *pdst = _data[0];
                    for(uint32_t i = 0; i < _height; i++)
                {
                    memcpy(pdst, psrc, _width);
                    psrc += pframe->_linesize[0];
                    pdst += _linesize[0];
                }

                psrc = pframe->_data[1];
                pdst = _data[1];
                    for(uint32_t j = 0; j < _height/2; j++)
                {
                    memcpy(pdst, psrc, _width);
                    psrc += pframe->_linesize[1];
                    pdst += _linesize[1];
                }
            }
            case FOURCC_RGB3:
            case FOURCC_BGR3:
            case FOURCC_BGRA:
            case FOURCC_RGBA:
            case FOURCC_ARGB:
            case FOURCC_ABGR:
            {
                uint8_t *psrc = pframe->_data[0];
                uint8_t *pdst = _data[0];
                int pitch = _linesize[0] > pframe->_linesize[0] ? pframe->_linesize[0] : _linesize[0];
                    for(uint32_t i = 0; i < _height; i++)
                {
                    memcpy(pdst, psrc, pitch);
                    psrc += pframe->_linesize[0];
                    pdst += _linesize[0];
                }
            }
            default:
            {
                assert(0);
                return -1;
            }
            }

            return 0;
        }
        else
        {
            if(_format != pframe->_format || _channel != pframe->_channel || _nb_samples != pframe->_nb_samples || !_data[0])
            {
                //lberror("Invalid audio property found, _format:%d != pframe->_format:%d || _channel:%d != pframe->_channel:%d || _nb_samples:%d != pframe->_nb_samples:%d || !_data[0]:%p\n", _format, pframe->_format, _channel, pframe->_channel, _nb_samples, pframe->_nb_samples, _data[0]);
                assert(0);
                return -1;
            }

            for(int i = 0; i < 4 && pframe->_data[i]; i++)
            {
                memcpy(_data[i], pframe->_data[i], pframe->_linesize[i]);
                _linesize[i] = pframe->_linesize[i];
            }
            return 0;
        }
    }

    int allocate()
    {
        if (lazy_media_type_video == _emedia_type && _width > 0 && _height > 0)
        {
            int size = format_size(_format, _width, _height);
            if (NULL == _data[0])
            {
                uint8_t *ptmp = new uint8_t[size];
                fill_video(_format, _width, _height, ptmp, _timestamp, false);
            }
            return 0;
        }
        else if(lazy_media_type_audio == _emedia_type && _channel > 0 && _format > 0 && _nb_samples > 0)
        {
            int sample_size = _channel * bytes_per_sample(_format) * _nb_samples;
            if (_linesize[0] != (uint32_t)sample_size)
            {
                if (_data[0])
                {
                    delete[] _data[0];
                    _data[0] = NULL;
                }
                _data[0] = new uint8_t[sample_size];
                _linesize[0] = sample_size;
            }
            return 0;
        }
        else
        {
            assert(0);
            //lberror("Invalid frame property, _emedia_type:%d, _format:%d, _width:%d, _height:%d, _channel:%d, _nb_samples:%d, allocate memory failed\n", _emedia_type, _format, _width, _height, _channel, _nb_samples);
            return -1;
        }
    }
    static int bytes_per_sample(int format)
    {
        switch(format)
        {
        case lazy_sample_fmt_u8:
            return 1;
        case lazy_sample_fmt_s16:
            return 2;
        case lazy_sample_fmt_s32:
            return 4;
        case lazy_sample_fmt_flt:
            return 4;
        default:
        {
            return -1;
        }
        }
    }
};

#endif