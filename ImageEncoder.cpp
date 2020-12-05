#include "ImageEncoder.h"

#include <sstream>
#include <iostream>
#include <opencv2/core/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <iomanip>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <assert.h>
#include <png.h>

#include "MutableVideoFrame.h"

ImageEncoder::ImageEncoder(DeviceProber* deviceProber) :
	m_refCount(1),
	m_deviceProber(deviceProber)
{
	m_frameConverter = CreateVideoConversionInstance();
}

ULONG ImageEncoder::AddRef(void)
{
	return __sync_add_and_fetch(&m_refCount, 1);
}

ULONG ImageEncoder::Release(void)
{
	int32_t newRefValue = __sync_sub_and_fetch(&m_refCount, 1);
	if (newRefValue == 0)
	{
		m_frameConverter->Release(); // does not assert to 0

		delete this;
		return 0;
	}
	return newRefValue;
}

void ImageEncoder::RGBImage(unsigned int iteration) {
	IDeckLinkVideoFrame* frame = m_deviceProber->GetLastFrame();

	frame->AddRef();

	frame = convertFrameIfReqired(frame);
	decodeRawVideoFrame(frame, iteration);

	frame->Release();
}

bool ImageEncoder::convertFrameToOpenCV(IDeckLinkVideoFrame* in, cv::Mat &frame)
{
	switch (in->GetPixelFormat()) {
    case bmdFormat8BitYUV:
    {
        void* data;
        if (FAILED(in->GetBytes(&data)))
            return false;

        cv::Mat mat = cv::Mat(in->GetHeight(), in->GetWidth(), CV_8UC2, data,
            in->GetRowBytes());

		// std::lock_guard<std::mutex> g( mtxFrameBytes);
        cv::cvtColor(mat, frame, cv::COLOR_YUV2BGR_UYVY);
        return true;
    }
    case bmdFormat8BitBGRA:
    {
        void* data;
        if (FAILED(in->GetBytes(&data)))
            return false;

		cv::Mat mat = cv::Mat(in->GetHeight(), in->GetWidth(), CV_8UC4, data);

		cv::cvtColor(mat, frame, cv::COLOR_BGRA2BGR);
        return true;
    }
    default:
    {
		/*
        ComPtr<IDeckLinkVideoConversion> deckLinkVideoConversion =
            CreateVideoConversionInstance();
        if (! deckLinkVideoConversion)
            return false;
        CvMatDeckLinkVideoFrame cvMatWrapper(in->GetHeight(), in->GetWidth());
        if (FAILED(deckLinkVideoConversion->ConvertFrame(in.get(), &cvMatWrapper)))
            return false;
        
		lock_guard<mutex> g( mtxFrameBytes);
		cv::cvtColor(cvMatWrapper.mat, out, CV_BGRA2BGR);
        return true;*/
		return false;
    }}
}

void ImageEncoder::decodeRawVideoFrame(IDeckLinkVideoFrame* videoFrame, unsigned int iteration)
{
	cv::Mat mFrame;
    if(videoFrame)
    {
        if (videoFrame->GetFlags() & bmdFrameHasNoInputSource)
        {
			std::cerr << "Frame received - No input signal detected\n" << std::endl;
        }
        else
        {
			convertFrameToOpenCV( videoFrame, mFrame );
			if (!mFrame.empty())
			{
				std::stringstream ss;
				int n_digits = 3;
				std::string prefix = "file";
				std::string ext(".png");
				ss << prefix << std::setfill('0') << std::setw(n_digits) << iteration << ext;
				cv::imwrite(ss.str(), mFrame);
			}
        }
	}
}

std::string ImageEncoder::EncodeImage() {
	IDeckLinkVideoFrame* frame = m_deviceProber->GetLastFrame();
	if(frame == NULL) {
		return "";
	}

	frame->AddRef();

	frame = convertFrameIfReqired(frame);
	std::string encodedImage = encodeToPng(frame);

	frame->Release();

	return encodedImage;
}

void pngWriteCallback(png_structp  png_ptr, png_bytep data, png_size_t length)
{
	std::stringstream* stream = (std::stringstream*)png_get_io_ptr(png_ptr);
	std::string str = std::string((const char*)data, length);
	(*stream) << str;
}

std::string ImageEncoder::encodeToPng(IDeckLinkVideoFrame* frame)
{
	png_structp png_ptr = png_create_write_struct(
		PNG_LIBPNG_VER_STRING,
		/* user_error_ptr */  NULL,
		/* user_error_fn */   NULL,
		/* user_warning_fn */ NULL);

	if (!png_ptr) {
		std::cerr << "Unable to allocate png_structp" << std::endl;
		exit(1);
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		std::cerr << "Unable to allocate png_infop" << std::endl;
		exit(1);
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		std::cerr << "Unable to setup png_jmpbuf" << std::endl;
		exit(1);
	}

	png_set_IHDR(
		png_ptr,
		info_ptr,
		frame->GetWidth(),
		frame->GetHeight(),
		/* bit_depth */ 8,
		PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT,
		PNG_FILTER_TYPE_DEFAULT);


	png_byte* frameBytes;
	frame->GetBytes((void**) &frameBytes);
	
	png_bytepp row_pointers = new png_bytep[frame->GetHeight()];
	for(long row = 0; row < frame->GetHeight(); row++)
	{
		row_pointers[row] = frameBytes + (row * frame->GetRowBytes());
	}

	png_set_rows(
		png_ptr,
		info_ptr,
		row_pointers);

	std::stringstream pngBody;

	png_set_write_fn(
		png_ptr,
		&pngBody,
		pngWriteCallback,
		NULL);

	png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_BGR, NULL);

	if (info_ptr != NULL)
	{
		png_free_data(png_ptr, info_ptr, PNG_FREE_ALL, -1);
	}

	if (png_ptr != NULL)
	{
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
	}

	delete row_pointers;

	return pngBody.str();
}

IDeckLinkVideoFrame* ImageEncoder::convertFrameIfReqired(IDeckLinkVideoFrame* frame)
{
	HRESULT result;

	if(frame->GetPixelFormat() == bmdFormat8BitBGRA)
	{
		return frame;
	}

	IDeckLinkVideoFrame* convertedFrame = new MutableVideoFrame(
		frame->GetWidth(),
		frame->GetHeight(),
		bmdFormat8BitBGRA);

	result = m_frameConverter->ConvertFrame(frame, convertedFrame);
	if (result != S_OK)
	{
		fprintf(stderr, "Failed to convert frame\n");
		exit(1);
	}

	frame->Release();
	return convertedFrame;
}
